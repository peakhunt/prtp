#ifndef __RTP_SESSION_DEF_H__
#define __RTP_SESSION_DEF_H__

#include "common_inc.h"
#include "soft_timer.h"
#include "generic_list.h"
#include "rtp_member_table.h"
#include "rtp_source_conflict.h"
#include "rtp_error.h"

struct __rtp_session_t;
typedef struct __rtp_session_t rtp_session_t;

typedef struct
{
  uint8_t*      payload;
  uint32_t      payload_len;
  uint32_t      rtp_ts;
  uint16_t      seq;
  uint32_t*     csrc;
  uint8_t       ncsrc;
} rtp_rx_report_t;

typedef struct
{
  struct sockaddr_in    rtp_addr;
  struct sockaddr_in    rtcp_addr;
  uint32_t              session_bw;
  uint8_t               cname[256 + 16];
  uint8_t               cname_len;
  uint8_t               pt;
  uint8_t               align_by_4;
} rtp_session_config_t;

struct __rtp_session_t
{
  ////////////////////////////////////////////////////////////
  //
  // for interfaces with user/transport
  //
  ////////////////////////////////////////////////////////////

  //
  // user layer callbacks
  //
  int (*rx_rtp)(rtp_session_t* sess, rtp_rx_report_t* rpt);
  uint32_t (*rtp_timestamp)(rtp_session_t* sess);
  void (*rr_rpt)(rtp_session_t* sess, uint32_t from_ssrc, rtcp_rr_t* rr);

  //
  // transport layer services for network I/O
  //
  int (*tx_rtp)(rtp_session_t* sess, uint8_t* pkt, uint32_t len);
  int (*tx_rtcp)(rtp_session_t* sess, uint8_t* pkt, uint32_t len);

  ////////////////////////////////////////////////////////////
  //
  // internal timer
  //
  ////////////////////////////////////////////////////////////
  SoftTimer   soft_timer;

  ////////////////////////////////////////////////////////////
  //
  // RTP packet
  //
  ////////////////////////////////////////////////////////////
  uint8_t             rtp_pkt[RTP_CONFIG_AVERAGE_RTCP_SIZE];
  uint16_t            seq;

  ////////////////////////////////////////////////////////////
  //
  // RTCP related session variables
  //
  ////////////////////////////////////////////////////////////
  rtp_member_table_t    member_table;
  rtcp_control_var_t    rtcp_var;
  SoftTimerElem         rtcp_timer;

  ////////////////////////////////////////////////////////////
  //
  // how am I gonna call it? interesting
  //
  ////////////////////////////////////////////////////////////
  rtp_member_t*       self;

  ////////////////////////////////////////////////////////////
  //
  // source conflict table
  //
  ////////////////////////////////////////////////////////////
  rtp_source_conflict_table_t   src_conflict;

  ////////////////////////////////////////////////////////////
  //
  // stats
  //
  ////////////////////////////////////////////////////////////
  uint32_t            invalid_rtcp_pkt;
  uint32_t            invalid_rtp_pkt;

  rtp_rx_error_t      last_rtp_error;
  rtcp_rx_error_t     last_rtcp_error;

  uint32_t            tx_pkt_count;
  uint32_t            tx_octet_count;

  ////////////////////////////////////////////////////////////
  //
  // config
  //
  ////////////////////////////////////////////////////////////
  rtp_session_config_t    config;
};

// 
// services for user & others
//
extern void rtp_session_init(rtp_session_t* sess, const rtp_session_config_t* config);
extern void rtp_session_deinit(rtp_session_t* sess);
extern void rtp_session_reset_tx_stats(rtp_session_t* sess);

extern int rtp_session_bye(rtp_session_t* sess);
extern int rtp_session_tx(rtp_session_t* sess, uint8_t* payload, uint32_t payload_len, uint32_t rtp_ts, uint32_t* csrc, uint8_t ncsrc);
 
//
// RX events from transport
//
extern void rtp_session_rx_rtp(rtp_session_t* sess, uint8_t* pkt, uint32_t len, struct sockaddr_in* from);
extern void rtp_session_rx_rtcp(rtp_session_t* sess, uint8_t* pkt, uint32_t len, struct sockaddr_in* from);

//
// 100ms tick event from library user
//
extern void rtp_timer_tick(rtp_session_t* sess);

#endif /*! __RTP_SESSION_DEF_H__ */
