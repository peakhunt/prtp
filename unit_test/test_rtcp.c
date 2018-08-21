#include "CUnit/Basic.h"
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#include <stdlib.h>
#include <stdio.h>

#include "rtp_session.h"
#include "rtp_session_util.h"
#include "rtcp_encoder.h"

#include "test_common.h"

static rtcp_rr_t    _rr_rx;
static uint32_t     _from_ssrc;

static void
rx_rtcp_rr_callback(rtp_session_t* sess, uint32_t from_ssrc, rtcp_rr_t* rr)
{
  _rr_rx = *rr;
  _from_ssrc = from_ssrc;
}


static void
test_rtcp_pkt_check(void)
{
  rtp_session_t*  sess;
  uint8_t         buf[256];
  rtcp_common_t*  hdr;

  sess = common_session_init();

  hdr = (rtcp_common_t*)buf;
  (void)hdr;

  rtp_session_rx_rtcp(sess, buf, sizeof(rtcp_common_t) - 1, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_header_too_short);

  rtp_session_rx_rtcp(sess, buf, sizeof(rtcp_common_t) + 3, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_len_not_multiple_4);

  hdr->version = RTP_VERSION;
  hdr->p = 0;
  hdr->count = 0;
  hdr->pt = RTCP_SDES;
  hdr->length = 0;
  rtp_session_rx_rtcp(sess, buf, sizeof(rtcp_common_t) + 8, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_invalid_mask);

  hdr->pt = RTCP_BYE;
  rtp_session_rx_rtcp(sess, buf, sizeof(rtcp_common_t) + 8, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_invalid_mask);

  hdr->pt = RTCP_APP;
  rtp_session_rx_rtcp(sess, buf, sizeof(rtcp_common_t) + 8, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_invalid_mask);

  hdr->pt = RTCP_SR;
  rtp_session_rx_rtcp(sess, buf, sizeof(rtcp_common_t) + 8, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_invalid_compound_packet);

  hdr->pt = RTCP_RR;
  rtp_session_rx_rtcp(sess, buf, sizeof(rtcp_common_t) + 8, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_invalid_compound_packet);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_rtcp_valid_pkts(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);
  pkt_len = rtcp_encoder_msg_len(&enc);

  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_member_first_heard);

  rtcp_encoder_reset(&enc);

  rtcp_encoder_rr_begin(&enc, 1002);
  rtcp_encoder_end_packet(&enc);
  pkt_len = rtcp_encoder_msg_len(&enc);

  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_member_first_heard);

  rtcp_encoder_deinit(&enc);

  rtp_session_deinit(sess);
}

static void
test_rtcp_new_member(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;
  rtp_member_t*     m;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  rtcp_encoder_rr_begin(&enc, 1002);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_member_first_heard);

  m = rtp_session_lookup_member(sess, 1001);
  CU_ASSERT(m != NULL);
  CU_ASSERT(rtp_member_is_rtcp_heard(m) == RTP_TRUE);
  CU_ASSERT(is_soft_timer_running(&m->member_te) != 0);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) == 0);

  m = rtp_session_lookup_member(sess, 1002);
  CU_ASSERT(m != NULL);
  CU_ASSERT(rtp_member_is_rtcp_heard(m) == RTP_TRUE);
  CU_ASSERT(is_soft_timer_running(&m->member_te) != 0);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) == 0);

  CU_ASSERT(sess->rtcp_var.members == 3);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

static void
test_rtcp_member_already_exists(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;
  rtp_member_t*     m;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  // first packet
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_member_first_heard);

  m = rtp_session_lookup_member(sess, 1001);
  CU_ASSERT(m != NULL);
  CU_ASSERT(rtp_member_is_rtcp_heard(m) == RTP_TRUE);
  CU_ASSERT(is_soft_timer_running(&m->member_te) != 0);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) == 0);
  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  // second packet 
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_no_error);
  CU_ASSERT(m != NULL);
  CU_ASSERT(rtp_member_is_rtcp_heard(m) == RTP_TRUE);
  CU_ASSERT(is_soft_timer_running(&m->member_te) != 0);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) == 0);
  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

static void
test_rtcp_member_timeout(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;
  rtp_member_t*     m;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  // first packet
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_member_first_heard);

  m = rtp_session_lookup_member(sess, 1001);
  CU_ASSERT(m != NULL);
  CU_ASSERT(rtp_member_is_rtcp_heard(m) == RTP_TRUE);
  CU_ASSERT(is_soft_timer_running(&m->member_te) != 0);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) == 0);
  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  // now let the member timeout
  for(uint32_t i = 0; i < (RTP_CONFIG_MEMBER_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_timer_tick(sess);
  }

  m = rtp_session_lookup_member(sess, 1001);
  CU_ASSERT(m == NULL);
  CU_ASSERT(sess->rtcp_var.members == 1);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

static void
test_rtcp_member_by_rtp(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;
  rtp_member_t*     m;

  uint8_t           buf[256];
  rtp_hdr_t*        hdr;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);


  hdr = (rtp_hdr_t*)buf;

  hdr->version = RTP_VERSION;
  hdr->cc = 0;
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 0;
  hdr->ssrc = htonl(1001);
  hdr->seq = htons(10);

  // first, RTP packet
  rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(4, 64), &_rtp_rem_addr);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_member_first_heard);
  m = rtp_session_lookup_member(sess, 1001);
  CU_ASSERT(m != NULL);

  // second, RTCP packet
  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_member_first_heard);

  m = rtp_session_lookup_member(sess, 1001);
  CU_ASSERT(m != NULL);
  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 1);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

static void
test_rtcp_3rd_party_conflict(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  // first packet
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);

  rtcp_encoder_reset(&enc);
  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);
  // first packet
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr2);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_3rd_party_conflict);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

static void
test_rtcp_own_conflict(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_sr_begin(&enc,
      TEST_OWN_SSRC,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  // first packet
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_ssrc_conflict);
  CU_ASSERT(rtp_source_conflict_lookup(&sess->src_conflict, &_rtcp_rem_addr) == RTP_TRUE);

  // new SSRC for self should have been allocated
  CU_ASSERT(sess->self->ssrc != TEST_OWN_SSRC);

  // second looped packet
  rtcp_encoder_reset(&enc);
  rtcp_encoder_sr_begin(&enc,
      sess->self->ssrc,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);

  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_source_in_conflict_list);

  // let the conflict entry timeout
  for(uint32_t i = 0; i < (RTP_CONFIG_SOURCE_CONFLICT_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_timer_tick(sess);
  }
  CU_ASSERT(rtp_source_conflict_lookup(&sess->src_conflict, &_rtcp_rem_addr) == RTP_FALSE);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

static void
test_rtcp_rx_sr(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;
  rtp_member_t*     m;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  m = rtp_session_lookup_member(sess, 1001);

  CU_ASSERT(m != NULL);
  CU_ASSERT(m->last_sr.second == 1);
  CU_ASSERT(m->last_sr.fraction == 2);
  CU_ASSERT(m->rtp_ts == 3);
  CU_ASSERT(m->pkt_count == 4);
  CU_ASSERT(m->octet_count == 5);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

static void
test_rtcp_rx_rr(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;

  sess = common_session_init();
  sess->rr_rpt = rx_rtcp_rr_callback;

  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_rr_begin(&enc, 1002);
  rtcp_encoder_rr_add_rr(&enc,
      1234,
      11,
      2222,
      3333,
      4444,
      5555,
      6666);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);

  CU_ASSERT(_from_ssrc == 1002);
  CU_ASSERT(ntohl(_rr_rx.ssrc) == 1234);
  CU_ASSERT(_rr_rx.fraction == 11);
  CU_ASSERT(ntohl(_rr_rx.lost << 8) == 2222);
  CU_ASSERT(ntohl(_rr_rx.last_seq) == 3333);
  CU_ASSERT(ntohl(_rr_rx.jitter) == 4444);
  CU_ASSERT(ntohl(_rr_rx.lsr) == 5555);
  CU_ASSERT(ntohl(_rr_rx.dlsr) == 6666);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

void
test_rtcp_add(CU_pSuite pSuite)
{
  CU_add_test(pSuite, "rtcp::pkt_check", test_rtcp_pkt_check);
  CU_add_test(pSuite, "rtcp::valid_pkts", test_rtcp_valid_pkts);
  CU_add_test(pSuite, "rtcp::new_member", test_rtcp_new_member);
  CU_add_test(pSuite, "rtcp::member_already_exists", test_rtcp_member_already_exists);
  CU_add_test(pSuite, "rtcp::member_timeout", test_rtcp_member_timeout);
  CU_add_test(pSuite, "rtcp::member_by_rtp", test_rtcp_member_by_rtp);
  CU_add_test(pSuite, "rtcp::3rd_party_conflict", test_rtcp_3rd_party_conflict);
  CU_add_test(pSuite, "rtcp::own_conflict", test_rtcp_own_conflict);
  CU_add_test(pSuite, "rtcp::rx_sr", test_rtcp_rx_sr);
  CU_add_test(pSuite, "rtcp::rx_rr", test_rtcp_rx_rr);
}
