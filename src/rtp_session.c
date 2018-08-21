#include "rtp_session.h"
#include "rtp_session_util.h"
#include "rtp.h"
#include "rtcp.h"
#include "rtp_random.h"
#include "rtp_timers.h"

////////////////////////////////////////////////////////////
//
// module privates
//
////////////////////////////////////////////////////////////
static void
rtp_session_init_self(rtp_session_t* sess, const struct sockaddr_in* rtp_addr, const struct sockaddr_in* rtcp_addr,
    const uint8_t* cname, uint8_t cname_len)
{
  // initialize self
  sess->self = rtp_session_alloc_member(sess, rtp_random32(RTP_CONFIG_RANDOM_TYPE));

  rtp_member_set_self(sess->self);
  rtp_member_set_cname(sess->self, cname, cname_len);

  memcpy(&sess->self->rtp_addr, rtp_addr, sizeof(struct sockaddr_in));
  memcpy(&sess->self->rtcp_addr, rtcp_addr, sizeof(struct sockaddr_in));
}

////////////////////////////////////////////////////////////
//
// public APIs and events
//
////////////////////////////////////////////////////////////
void
rtp_session_init(rtp_session_t* sess, const rtp_session_config_t* config)
{
  memcpy(&sess->config, config, sizeof(rtp_session_config_t));

  sess->last_rtp_error    = rtp_rx_error_no_error;
  sess->last_rtcp_error   = rtcp_rx_error_no_error;

  soft_timer_init(&sess->soft_timer, 100);
  rtp_source_conflict_table_init(&sess->src_conflict, &sess->soft_timer);
  rtp_member_table_init(&sess->member_table);

  sess->invalid_rtcp_pkt  = 0;
  sess->invalid_rtp_pkt   = 0;

  sess->seq = (uint16_t)rtp_random32(RTP_CONFIG_RANDOM_TYPE);

  rtp_session_reset_tx_stats(sess);

  rtp_session_init_self(sess, &config->rtp_addr, &config->rtcp_addr, config->cname, config->cname_len);

  rtp_init(sess);
  rtcp_init(sess);
}

void
rtp_session_reset_tx_stats(rtp_session_t* sess)
{
  sess->tx_pkt_count      = 0;
  sess->tx_octet_count    = 0;
}

void
rtp_session_deinit(rtp_session_t* sess)
{
  rtp_member_t*     m;

  rtp_deinit(sess);
  rtcp_deinit(sess);

  m = rtp_member_table_get_first(&sess->member_table);
  while(m != NULL)
  {
    rtp_timers_deinit_member(sess, m);

    m = rtp_member_table_get_next(&sess->member_table, m);
  }

  rtp_source_conflict_table_deinit(&sess->src_conflict);

  soft_timer_deinit(&sess->soft_timer);
}

int
rtp_session_tx(rtp_session_t* sess, uint8_t* payload, uint32_t payload_len, uint32_t rtp_ts, uint32_t* csrc, uint8_t ncsrc)
{
  rtp_tx(sess, payload, payload_len, rtp_ts, csrc, ncsrc);
  return 0;
}

int
rtp_session_bye(rtp_session_t* sess)
{
  rtcp_tx_bye(sess);
  return 0;
}

void
rtp_session_rx_rtp(rtp_session_t* sess, uint8_t* pkt, uint32_t len, struct sockaddr_in* from)
{
  rtp_rx(sess, pkt, len, from);
}

void
rtp_session_rx_rtcp(rtp_session_t* sess, uint8_t* pkt, uint32_t len, struct sockaddr_in* from)
{
  rtcp_rx(sess, pkt, len, from);
}

void
rtp_timer_tick(rtp_session_t* sess)
{
  soft_timer_drive(&sess->soft_timer);
}
