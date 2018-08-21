#include "CUnit/Basic.h"
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#include <stdlib.h>
#include <stdio.h>

#include "test_common.h"

struct sockaddr_in     _rtp_addr,
                       _rtcp_addr;

struct sockaddr_in     _rtp_rem_addr,
                       _rtcp_rem_addr;

struct sockaddr_in     _rtp_rem_addr2,
                       _rtcp_rem_addr2;

static uint32_t       _own_rtp_ts = 0;

static int
dummy_tx_rtp(rtp_session_t* sess, uint8_t* pkt, uint32_t len)
{
  return 0;
}

static int
dummy_tx_rtcp(rtp_session_t* sess, uint8_t* pkt, uint32_t len)
{
  return 0;
}

static void
dummy_rr_rpt(rtp_session_t* sess, uint32_t ssrc, rtcp_rr_t* rr)
{
}

static uint32_t
test_rtp_timestamp(rtp_session_t* sess)
{
  return _own_rtp_ts;
}

void
test_rtp_timestamp_set(rtp_session_t* sess, uint32_t ts)
{
  _own_rtp_ts = ts;
}

rtp_session_t*
common_session_init(void)
{
  rtp_session_t*  sess;
  rtp_session_config_t      cfg;

  sess = malloc(sizeof(rtp_session_t));
  CU_ASSERT(sess != NULL);

  sess->rr_rpt = dummy_rr_rpt;
  sess->rtp_timestamp = test_rtp_timestamp;
  sess->tx_rtp = dummy_tx_rtp;
  sess->tx_rtcp = dummy_tx_rtcp;

  memcpy(&cfg.rtp_addr, &_rtp_addr, sizeof(_rtp_addr));
  memcpy(&cfg.rtcp_addr, &_rtcp_addr, sizeof(_rtcp_addr));
  cfg.session_bw = 64 * 1000;
  memcpy(cfg.cname, SESSION_NAME, strlen(SESSION_NAME));
  cfg.cname_len = strlen(SESSION_NAME);
  cfg.pt = SESSION_PT;
  cfg.align_by_4 = RTP_FALSE;

  rtp_session_init(sess, &cfg);
  rtp_member_table_change_ssrc(&sess->member_table, sess->self, TEST_OWN_SSRC);

  return sess;
}

void
test_common_init(void)
{
  memset(&_rtp_addr, 0, sizeof(_rtp_addr));
  memset(&_rtcp_addr, 0, sizeof(_rtcp_addr));

  _rtp_addr.sin_family       = AF_INET;
  _rtp_addr.sin_addr.s_addr  = inet_addr("192.168.1.1");
  _rtp_addr.sin_port         = htons(16000);

  _rtcp_addr.sin_family       = AF_INET;
  _rtcp_addr.sin_addr.s_addr  = inet_addr("192.168.1.1");
  _rtcp_addr.sin_port         = htons(16001);

  _rtp_rem_addr.sin_family       = AF_INET;
  _rtp_rem_addr.sin_addr.s_addr  = inet_addr("192.168.1.10");
  _rtp_rem_addr.sin_port         = htons(17000);

  _rtcp_rem_addr.sin_family       = AF_INET;
  _rtcp_rem_addr.sin_addr.s_addr  = inet_addr("192.168.1.10");
  _rtcp_rem_addr.sin_port         = htons(17001);

  _rtp_rem_addr2.sin_family       = AF_INET;
  _rtp_rem_addr2.sin_addr.s_addr  = inet_addr("192.168.1.20");
  _rtp_rem_addr2.sin_port         = htons(17000);

  _rtcp_rem_addr2.sin_family       = AF_INET;
  _rtcp_rem_addr2.sin_addr.s_addr  = inet_addr("192.168.1.20");
  _rtcp_rem_addr2.sin_port         = htons(17001);

  _own_rtp_ts = 0;
}
