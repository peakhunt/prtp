#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include "generic_list.h"
#include "cli.h"
#include "io_driver.h"

#include "rtp_task.h"

extern io_driver_t* main_io_driver(void);

static rtp_session_t        _rtp_session;

static struct sockaddr_in   _rtp_addr,
                            _rtcp_addr;

static struct sockaddr_in   _rtp_rem_addr,
                            _rtcp_rem_addr;

static int                  _rtp_sock,
                            _rtcp_sock;
static io_driver_watcher_t  _rtp_watcher;
static io_driver_watcher_t  _rtcp_watcher;

static io_driver_watcher_t  _timer_watcher;
static int                  _timerfd;

static int                  _samplerfd;
static io_driver_watcher_t  _sampler_watcher;

static const char* TAG = "rtp_task";

extern void rtp_task_show_rtp_stats(cli_intf_t* intf);

static char *cname;

static uint32_t    _rtp_ts = 0;

static uint8_t    _rtp_tx_enabled = RTP_TRUE;
static uint8_t    _rtcp_tx_enabled = RTP_TRUE;

#if 0
#define DLOGI       DLOGI
#else
#define DLOGI(tag, str, ...)     (void)tag
#endif

#define DLOGE       RTPLOGE

static int _test_u8_pcm_fd;

static rtp_session_config_t   _session_cfg;

//////////////////////////////////////////////////////////////////////////
//
// from session -> user tx rtp routine
//
//////////////////////////////////////////////////////////////////////////
static int
rtp_task_tx_rtp(rtp_session_t* sess, uint8_t* pkt, uint32_t len)
{
  int ret;

  DLOGI(TAG, "TX RTP %d\n", len);

  ret = sendto(_rtp_sock, pkt, len, 0, (struct sockaddr*)&_rtp_rem_addr, sizeof(_rtp_rem_addr));
  if(ret <= 0)
  {
    DLOGE(TAG, "TX RTP failed: %d\n", ret);
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// from session -> user tx rtcp routine
//
//////////////////////////////////////////////////////////////////////////
static int
rtp_task_tx_rtcp(rtp_session_t* sess, uint8_t* pkt, uint32_t len)
{
  int ret;

  if(_rtcp_tx_enabled == RTP_FALSE)
  {
    return 0;
  }

  DLOGI(TAG, "TX RTCP %d\n", len);

  ret = sendto(_rtp_sock, pkt, len, 0, (struct sockaddr*)&_rtcp_rem_addr, sizeof(_rtcp_rem_addr));
  if(ret <= 0)
  {
    DLOGE(TAG, "TX RTCP failed: %d\n", ret);
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// from session -> user rr report
//
//////////////////////////////////////////////////////////////////////////
static void
rtp_task_rr_report(rtp_session_t* sess, uint32_t from_ssrc, rtcp_rr_t* rr)
{
}

//////////////////////////////////////////////////////////////////////////
//
// from session -> user rx rtp callback
//
//////////////////////////////////////////////////////////////////////////
static int
rtp_task_rx_rtp(rtp_session_t* sess, rtp_rx_report_t* rpt)
{
  DLOGI(TAG, "RX RTP %d\n", rpt->payload_len);
  // FIXME
  return 0;
}

static uint32_t
rtp_task_rtp_timestamp(rtp_session_t* sess)
{
  return _rtp_ts;
}

//////////////////////////////////////////////////////////////////////////
//
// from io driver -> user rx rtp notification
//
//////////////////////////////////////////////////////////////////////////
static void
on_rx_rtp_from_sock(io_driver_watcher_t* watcher, io_driver_event event)
{
  uint8_t buffer[1024];
  int len;
  struct sockaddr_in    from;
  socklen_t             from_len;

  from_len = sizeof(from);

  DLOGI(TAG, "on_rx_rtp_from_sock\n");

  len = recvfrom(_rtp_sock, buffer, 1024, 0, (struct sockaddr*)&from, &from_len);
  if(len <= 0)
  {
    DLOGE(TAG, "on_rx_rtp_from_sock error %d\n", len);
    return;
  }

  DLOGI(TAG, "on_rx_rtcp_from_sock got %d bytes from %s:%d\n", len,
      inet_ntoa(from.sin_addr), htons(from.sin_port));

  rtp_session_rx_rtp(&_rtp_session, buffer, len, &from);
}

//////////////////////////////////////////////////////////////////////////
//
// from io driver -> user rx rtcp notification
//
//////////////////////////////////////////////////////////////////////////
static void
on_rx_rtcp_from_sock(io_driver_watcher_t* watcher, io_driver_event event)
{
  uint8_t buffer[1024];
  int len;
  struct sockaddr_in    from;
  socklen_t             from_len;

  from_len = sizeof(from);

  DLOGI(TAG, "on_rx_rtcp_from_sock\n");

  len = recvfrom(_rtcp_sock, buffer, 1024, 0, (struct sockaddr*)&from, &from_len);
  if(len <= 0)
  {
    DLOGE(TAG, "on_rx_rtcp_from_sock error %d\n", len);
    return;
  }

  DLOGI(TAG, "on_rx_rtcp_from_sock got %d bytes from %s:%d\n", len,
      inet_ntoa(from.sin_addr), htons(from.sin_port));

  rtp_session_rx_rtcp(&_rtp_session, buffer, len, &from);
}

//////////////////////////////////////////////////////////////////////////
//
// from io driver -> user timing notification
//
//////////////////////////////////////////////////////////////////////////
static void
rtp_task_timer_callback(io_driver_watcher_t* watcher, io_driver_event event)
{
  uint64_t  v;

  read(_timerfd, &v, sizeof(v));
  (void)v;

  // DLOGI(TAG, "rtp task timing tick\n");

  rtp_timer_tick(&_rtp_session);
}

static void
rtp_task_sampler_callback(io_driver_watcher_t* watcher, io_driver_event event)
{
  uint64_t  v;

  read(_samplerfd, &v, sizeof(v));
  (void)v;

  if(_rtp_tx_enabled == RTP_FALSE)
  {
    return;
  }
  //
  // 8 KHz sampling rate
  // 20ms packetization time
  // 8 bit quantization
  // 1ms equals 8 samples
  // so packet size is 20 * 8 = 160 bytes
  //
  uint8_t   sample[160];
  int       ret;
  int       nread = 0;

  while(nread < 160)
  {
    ret = read(_test_u8_pcm_fd, &sample[nread], 160 - nread);
    if(ret <= 0)
    {
      lseek(_test_u8_pcm_fd, 0, SEEK_SET);
    }
    nread += ret;
  }
  _rtp_ts += 160;

  rtp_session_tx(&_rtp_session, sample, 160,  _rtp_ts, NULL, 0);
}

//////////////////////////////////////////////////////////////////////////
//
// initializers
//
//////////////////////////////////////////////////////////////////////////
static inline void
rtp_task_init_timing_service(void)
{
  struct itimerspec new_value;

  DLOGI(TAG, "initializing rtp timing service\n");

  _timerfd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
  new_value.it_interval.tv_sec     = 0;
  new_value.it_interval.tv_nsec    = 100 * 1000000;   // nano sec - 20ms
  new_value.it_value = new_value.it_interval;
  timerfd_settime(_timerfd, 0, &new_value, NULL);

  io_driver_watcher_init(&_timer_watcher);
  _timer_watcher.fd = _timerfd;
  _timer_watcher.callback = rtp_task_timer_callback;

  io_driver_watch(main_io_driver(), &_timer_watcher, IO_DRIVER_EVENT_RX);
}

static inline void
rtp_task_init_sampler(void)
{
  //
  // this demo simulates 8KHz 20ms rtp 
  // RTP time inc is 8 Khz / (1000ms / 20ms) = 160
  //
  struct itimerspec new_value;

  DLOGI(TAG, "initializing rtp timing service\n");

  _samplerfd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
  new_value.it_interval.tv_sec     = 0;
  new_value.it_interval.tv_nsec    = 20 * 1000000;   // nano sec - 20ms
  new_value.it_value = new_value.it_interval;
  timerfd_settime(_samplerfd, 0, &new_value, NULL);

  io_driver_watcher_init(&_sampler_watcher);
  _sampler_watcher.fd = _samplerfd;
  _sampler_watcher.callback = rtp_task_sampler_callback;

  _test_u8_pcm_fd = open("test_pcm_ulaw.raw", 0);
  if(_test_u8_pcm_fd < 0)
  {
    DLOGE(TAG, "failed to open test.raw file");
    exit(-1);
  }

  io_driver_watch(main_io_driver(), &_sampler_watcher, IO_DRIVER_EVENT_RX);
}

static inline void
rtp_task_init_sock(void)
{
  _rtp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(_rtp_sock < 0)
  {
    DLOGE(TAG, "_rtp_sock socket() failed\n");
    exit(-1);
  }

  if(bind(_rtp_sock, (struct sockaddr*)&_rtp_addr, sizeof(_rtp_addr)) != 0)
  {
    DLOGE(TAG, "_rtp_sock bind() failed\n");
    exit(-1);
  }

  _rtcp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(_rtcp_sock < 0)
  {
    DLOGE(TAG, "_rtcp_sock socket() failed\n");
    exit(-1);
  }

  if(bind(_rtcp_sock, (struct sockaddr*)&_rtcp_addr, sizeof(_rtcp_addr)) != 0)
  {
    DLOGE(TAG, "_rtcp_sock bind() failed\n");
    exit(-1);
  }

  io_driver_watcher_init(&_rtp_watcher);
  _rtp_watcher.fd = _rtp_sock;
  _rtp_watcher.callback = on_rx_rtp_from_sock;

  io_driver_watcher_init(&_rtcp_watcher);
  _rtcp_watcher.fd = _rtcp_sock;
  _rtcp_watcher.callback = on_rx_rtcp_from_sock;

  io_driver_watch(main_io_driver(), &_rtp_watcher, IO_DRIVER_EVENT_RX);
  io_driver_watch(main_io_driver(), &_rtcp_watcher, IO_DRIVER_EVENT_RX);
}

void
rtp_task_rtp_control(uint8_t enable)
{
  _rtp_tx_enabled = enable;
}

void
rtp_task_rtcp_control(uint8_t enable)
{
  _rtcp_tx_enabled = enable;
}

void
rtp_task_init(uint8_t blah)
{
  _rtp_session.rr_rpt         = rtp_task_rr_report;
  _rtp_session.tx_rtp         = rtp_task_tx_rtp;
  _rtp_session.tx_rtcp        = rtp_task_tx_rtcp;
  _rtp_session.rx_rtp         = rtp_task_rx_rtp;
  _rtp_session.rtp_timestamp  = rtp_task_rtp_timestamp;

  memset(&_rtp_addr, 0, sizeof(_rtp_addr));
  memset(&_rtcp_addr, 0, sizeof(_rtcp_addr));
  memset(&_rtp_rem_addr, 0, sizeof(_rtp_rem_addr));
  memset(&_rtcp_rem_addr, 0, sizeof(_rtcp_rem_addr));

  if(blah == 1)
  {
    _rtp_addr.sin_family       = AF_INET;
    _rtp_addr.sin_addr.s_addr  = inet_addr("192.168.1.115");
    _rtp_addr.sin_port         = htons(16000);

    _rtcp_addr.sin_family       = AF_INET;
    _rtcp_addr.sin_addr.s_addr  = inet_addr("192.168.1.115");
    _rtcp_addr.sin_port         = htons(16001);

    _rtp_rem_addr.sin_family       = AF_INET;
    _rtp_rem_addr.sin_addr.s_addr  = inet_addr("192.168.1.111");
    _rtp_rem_addr.sin_port         = htons(16000);

    _rtcp_rem_addr.sin_family       = AF_INET;
    _rtcp_rem_addr.sin_addr.s_addr  = inet_addr("192.168.1.111");
    _rtcp_rem_addr.sin_port         = htons(16001);

    cname = "Demo1";
  }
  else
  {
    _rtp_addr.sin_family       = AF_INET;
    _rtp_addr.sin_addr.s_addr  = inet_addr("192.168.1.111");
    _rtp_addr.sin_port         = htons(16000);

    _rtcp_addr.sin_family       = AF_INET;
    _rtcp_addr.sin_addr.s_addr  = inet_addr("192.168.1.111");
    _rtcp_addr.sin_port         = htons(16001);

    _rtp_rem_addr.sin_family       = AF_INET;
    _rtp_rem_addr.sin_addr.s_addr  = inet_addr("192.168.1.115");
    _rtp_rem_addr.sin_port         = htons(16000);

    _rtcp_rem_addr.sin_family       = AF_INET;
    _rtcp_rem_addr.sin_addr.s_addr  = inet_addr("192.168.1.115");
    _rtcp_rem_addr.sin_port         = htons(16001);

    cname = "Demo2";
  }

  rtp_task_init_sock();

  // init timing service
  rtp_task_init_timing_service();

  // FIXME socket init
  DLOGI(TAG, "initializing session\n");

  memcpy(&_session_cfg.cname, cname, strlen(cname));
  _session_cfg.cname_len = strlen(cname);
  memcpy(&_session_cfg.rtp_addr, &_rtp_addr, sizeof(_rtp_addr));
  memcpy(&_session_cfg.rtcp_addr, &_rtcp_addr, sizeof(_rtcp_addr));
  _session_cfg.session_bw = 64000;
  _session_cfg.pt = 0;
  _session_cfg.align_by_4 = RTP_FALSE;

  rtp_session_init(&_rtp_session, &_session_cfg);

  rtp_task_init_sampler();

  DLOGI(TAG, "done initializing session\n");
}

rtp_session_t*
rtp_task_get_session(void)
{
  return &_rtp_session;
}
