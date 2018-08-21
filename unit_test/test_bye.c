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

static void
test_bye_normal_rtp(void)
{
  rtp_session_t*    sess;
  rtp_hdr_t*        hdr;
  uint8_t           buf[256];
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_member_t*     m;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  hdr = (rtp_hdr_t*)buf;
  hdr->version = RTP_VERSION;
  hdr->cc = 0x00;
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 0;
  hdr->ssrc = htonl(1234);
  hdr->seq = htons(10);

  // create member with rtp packet
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64, &_rtp_rem_addr);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_member_first_heard);
  m = rtp_session_lookup_member(sess, 1234);
  CU_ASSERT(m != NULL);

  //
  // send by packet
  //
  rtcp_encoder_sr_begin(&enc,
      1234,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  rtcp_encoder_bye_begin(&enc);
  rtcp_encoder_bye_add_ssrc(&enc, 1234);
  rtcp_encoder_end_packet(&enc);

  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 1);

  pkt_len = rtcp_encoder_msg_len(&enc);
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  m = rtp_session_lookup_member(sess, 1234);
  CU_ASSERT(m != NULL);
  CU_ASSERT(rtp_member_is_bye_received(m) == RTP_TRUE);

  CU_ASSERT(sess->rtcp_var.members == 1);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  CU_ASSERT(is_soft_timer_running(&m->member_te) == 0);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) == 0);
  CU_ASSERT(is_soft_timer_running(&m->leave_te) == 1);

  // rtp packet should be dropped
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64, &_rtp_rem_addr);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_member_bye_in_progress);

  // rtcp packet should be dropped
  pkt_len = rtcp_encoder_msg_len(&enc);
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_member_bye_in_progress);

  // let the leave timer timeout
  for(uint32_t i = 0; i < (RTP_CONFIG_LEAVE_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_timer_tick(sess);
  }
  CU_ASSERT(rtp_session_lookup_member(sess, 1234) == NULL);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
  free(sess);
}

static void
test_bye_normal_rtcp(void)
{
  rtp_session_t*    sess;
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_member_t*     m;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  // create member by rtcp packet
  rtcp_encoder_sr_begin(&enc,
      1234,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  rtcp_encoder_sdes_begin(&enc);
  rtcp_encoder_sdes_chunk_begin(&enc, 1234);
  rtcp_encoder_sdes_chunk_add_cname(&enc, (uint8_t*)"Test Packet", 12);
  rtcp_encoder_sdes_chunk_end(&enc);
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);

  m = rtp_session_lookup_member(sess, 1234);
  CU_ASSERT(m != NULL);
  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  //
  // send by packet
  //
  rtcp_encoder_reset(&enc);

  rtcp_encoder_sr_begin(&enc,
      1234,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);

  rtcp_encoder_bye_begin(&enc);
  rtcp_encoder_bye_add_ssrc(&enc, 1234);
  rtcp_encoder_bye_add_reason(&enc, "Bye Bye...");
  rtcp_encoder_end_packet(&enc);
  pkt_len = rtcp_encoder_msg_len(&enc);
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  m = rtp_session_lookup_member(sess, 1234);
  CU_ASSERT(m != NULL);
  CU_ASSERT(rtp_member_is_bye_received(m) == RTP_TRUE);

  CU_ASSERT(sess->rtcp_var.members == 1);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  // let the leave timer timeout
  for(uint32_t i = 0; i < (RTP_CONFIG_LEAVE_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_timer_tick(sess);
  }
  CU_ASSERT(rtp_session_lookup_member(sess, 1234) == NULL);
  CU_ASSERT(sess->rtcp_var.members == 1);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
  free(sess);
}

void
test_bye_add(CU_pSuite pSuite)
{
  CU_add_test(pSuite, "bye::normal_rtp", test_bye_normal_rtp);
  CU_add_test(pSuite, "bye::normal_rtcp", test_bye_normal_rtcp);
}
