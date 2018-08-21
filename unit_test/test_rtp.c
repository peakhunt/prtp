#include "CUnit/Basic.h"
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#include <stdlib.h>
#include <stdio.h>

#include "rtp_session.h"
#include "rtp_session_util.h"

#include "test_common.h"
#include "rtcp_encoder.h"

static uint8_t _rtp_rx    = RTP_FALSE;
static uint32_t _rtp_payload_len = 0; 


static uint8_t*   _tx_pkt;
static uint32_t   _tx_len;

static int
dummy_rx_rtp(rtp_session_t* sess, rtp_rx_report_t* rpt)
{
  _rtp_rx = RTP_TRUE;
  _rtp_payload_len = rpt->payload_len;
  return 0;
}

static int
tx_rtp_test(rtp_session_t* sess, uint8_t* pkt, uint32_t len)
{
  _tx_pkt = pkt;
  _tx_len = len;

  return len;
}


static void
test_rtp_check(void)
{
  rtp_session_t*  sess;
  rtp_hdr_t*    hdr;
  uint8_t      buf[256];

  sess = common_session_init();

  hdr = (rtp_hdr_t*)buf;

  // test 1. hdr length too short
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 5,  &_rtp_rem_addr);
  CU_ASSERT(sess->invalid_rtp_pkt == 1);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_header_too_short);

  // test 2. invalid header size
  hdr->version = RTP_VERSION;
  hdr->cc = 0xf;
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + (hdr->cc - 3)  * sizeof(uint32_t),  &_rtp_rem_addr);
  CU_ASSERT(sess->invalid_rtp_pkt == 2);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_invalid_csrc_count);

  // test 3. invalid rtp version
  hdr->version = 0x3;
  hdr->cc = 0x00;
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4,  &_rtp_rem_addr);
  CU_ASSERT(sess->invalid_rtp_pkt == 3);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_invalid_rtp_version);

  // invalid payload type
  hdr->version = RTP_VERSION;
  hdr->cc = 0x00;
  hdr->pt = SESSION_PT - 1;
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4,  &_rtp_rem_addr);
  CU_ASSERT(sess->invalid_rtp_pkt == 4);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_invalid_payload_type);

  // padding
  hdr->version = RTP_VERSION;
  hdr->cc = 0x00;
  hdr->pt = SESSION_PT;
  hdr->p = 1;
  buf[sizeof(rtp_hdr_t) - 4 + 64 - 1] = 84;
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64,  &_rtp_rem_addr);
  CU_ASSERT(sess->invalid_rtp_pkt == 5);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_invalid_octet_count);

  // extension : not implemented
  hdr->version = RTP_VERSION;
  hdr->cc = 0x00;
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 1;
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 32,  &_rtp_rem_addr);
  CU_ASSERT(sess->invalid_rtp_pkt == 6);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_not_implemented);

  // success: no padding
  hdr->version = RTP_VERSION;
  hdr->cc = 0x00;
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 0;
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64,  &_rtp_rem_addr);
  CU_ASSERT(sess->invalid_rtp_pkt == 6);

  // success: padding
  hdr->version = RTP_VERSION;
  hdr->cc = 0x00;
  hdr->pt = SESSION_PT;
  hdr->p = 1;
  hdr->x = 0;
  hdr->ssrc = 1234;
  buf[sizeof(rtp_hdr_t) - 4 + 64 - 1] = 3;
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64,  &_rtp_rem_addr);
  CU_ASSERT(sess->invalid_rtp_pkt == 6);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_rtp_tx(void)
{
  rtp_session_t*  sess;
  rtp_hdr_t*      hdr;
  uint8_t         buf[256];
  uint32_t        csrc_list[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

  sess = common_session_init();
  sess->tx_rtp = tx_rtp_test;

  for(uint32_t i = 0; i < 256; i++)
  {
    buf[i] = (uint8_t)i;
  }

  // case 1. simple.
  rtp_session_tx(sess, buf, 160, 1234, csrc_list, 0);
  CU_ASSERT(_tx_len == 172);
  hdr = (rtp_hdr_t*)_tx_pkt;
  CU_ASSERT(hdr->version == RTP_VERSION);
  CU_ASSERT(hdr->p == 0);
  CU_ASSERT(hdr->x == 0);
  CU_ASSERT(hdr->cc == 0);
  CU_ASSERT(hdr->m == 0);
  CU_ASSERT(hdr->pt == SESSION_PT);
  CU_ASSERT(ntohl(hdr->ts) == 1234);
  CU_ASSERT(ntohl(hdr->ssrc) == TEST_OWN_SSRC);
  CU_ASSERT(memcmp(&_tx_pkt[12], buf, 160) == 0);

  // case 2. csrc
  rtp_session_tx(sess, buf, 160, 1234, csrc_list, 15);
  CU_ASSERT(_tx_len == (160 + 12 + 4 * 15));
  hdr = (rtp_hdr_t*)_tx_pkt;
  CU_ASSERT(hdr->version == RTP_VERSION);
  CU_ASSERT(hdr->p == 0);
  CU_ASSERT(hdr->x == 0);
  CU_ASSERT(hdr->cc == 15);
  CU_ASSERT(hdr->m == 0);
  CU_ASSERT(hdr->pt == SESSION_PT);
  CU_ASSERT(ntohl(hdr->ts) == 1234);
  CU_ASSERT(ntohl(hdr->ssrc) == TEST_OWN_SSRC);
  for(uint8_t i = 0; i < 15; i++)
  {
    CU_ASSERT(ntohl(hdr->csrc[i]) == csrc_list[i]);
  }
  CU_ASSERT(memcmp(&_tx_pkt[12 + 4 * 15], buf, 160) == 0);

  // case 3. no padding
  rtp_session_tx(sess, buf, 163, 1234, csrc_list, 15);
  CU_ASSERT(_tx_len == (163 + 12 + 4 * 15));
  hdr = (rtp_hdr_t*)_tx_pkt;
  CU_ASSERT(hdr->version == RTP_VERSION);
  CU_ASSERT(hdr->p == 0);
  CU_ASSERT(hdr->x == 0);
  CU_ASSERT(hdr->cc == 15);
  CU_ASSERT(hdr->m == 0);
  CU_ASSERT(hdr->pt == SESSION_PT);
  CU_ASSERT(ntohl(hdr->ts) == 1234);
  CU_ASSERT(ntohl(hdr->ssrc) == TEST_OWN_SSRC);
  for(uint8_t i = 0; i < 15; i++)
  {
    CU_ASSERT(ntohl(hdr->csrc[i]) == csrc_list[i]);
  }
  CU_ASSERT(memcmp(&_tx_pkt[12 + 4 * 15], buf, 163) == 0);

  // case 4. padding
  sess->config.align_by_4 = RTP_TRUE;
  rtp_session_tx(sess, buf, 161, 1234, csrc_list, 15);
  CU_ASSERT(_tx_len == (161 + 12 + 4 * 15 + 3));
  hdr = (rtp_hdr_t*)_tx_pkt;
  CU_ASSERT(hdr->version == RTP_VERSION);
  CU_ASSERT(hdr->p == 1);
  CU_ASSERT(hdr->x == 0);
  CU_ASSERT(hdr->cc == 15);
  CU_ASSERT(hdr->m == 0);
  CU_ASSERT(hdr->pt == SESSION_PT);
  CU_ASSERT(ntohl(hdr->ts) == 1234);
  CU_ASSERT(ntohl(hdr->ssrc) == TEST_OWN_SSRC);
  for(uint8_t i = 0; i < 15; i++)
  {
    CU_ASSERT(ntohl(hdr->csrc[i]) == csrc_list[i]);
  }
  CU_ASSERT(memcmp(&_tx_pkt[12 + 4 * 15], buf, 161) == 0);
  CU_ASSERT(_tx_pkt[161 + 12 + 4 * 15 + 0] == 0);
  CU_ASSERT(_tx_pkt[161 + 12 + 4 * 15 + 1] == 0);
  CU_ASSERT(_tx_pkt[161 + 12 + 4 * 15 + 2] == 3);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_rtp_normal_flow(void)
{
  rtp_session_t*  sess;
  rtp_hdr_t*    hdr;
  uint8_t      buf[256];
  rtp_member_t*   m;
  int seq;

  sess = common_session_init();
  sess->rx_rtp = dummy_rx_rtp;

  hdr = (rtp_hdr_t*)buf;

  hdr->version = RTP_VERSION;
  hdr->cc = 0x00;
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 0;
  hdr->ssrc = htonl(1234);
  hdr->seq = htons(10);

  // first rtp packet.
  // member should be created and in probation mode
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64, &_rtp_rem_addr);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_member_first_heard);
  m = rtp_session_lookup_member(sess, 1234);

  CU_ASSERT(m != NULL);
  CU_ASSERT(m->rtp_src.base_seq == 10);
  CU_ASSERT(m->rtp_src.max_seq == (10-1));
  CU_ASSERT(memcmp(&m->rtp_addr, &_rtp_rem_addr, sizeof(_rtp_rem_addr)) == 0);
  CU_ASSERT(rtp_member_is_rtp_heard(m) == RTP_TRUE);
  CU_ASSERT(rtp_member_is_rtcp_heard(m) == RTP_FALSE);
  CU_ASSERT(m->rtp_src.probation == RTP_CONFIG_MIN_SEQUENTIAL);
  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 1);
  CU_ASSERT(is_soft_timer_running(&m->member_te) != 0);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) != 0);
  CU_ASSERT(rtp_member_is_validated(m) == RTP_FALSE);

  for(seq = 0; seq < (RTP_CONFIG_MIN_SEQUENTIAL-1); seq++)
  {
    // following rtp packet
    hdr->seq = htons(10 + seq);
    rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64, &_rtp_rem_addr);

    CU_ASSERT(sess->last_rtp_error == rtp_rx_error_seq_error);
    CU_ASSERT(m->rtp_src.probation == (RTP_CONFIG_MIN_SEQUENTIAL-(seq+1)));
    CU_ASSERT(sess->rtcp_var.members == 2);
    CU_ASSERT(sess->rtcp_var.senders == 1);
    CU_ASSERT(rtp_member_is_validated(m) == RTP_FALSE);
  }

  _rtp_rx = RTP_FALSE;
  _rtp_payload_len = 0;

  // probation should be finished
  hdr->seq = htons(10 + seq);
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64, &_rtp_rem_addr);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_no_error);
  CU_ASSERT(m->rtp_src.probation == 0);
  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 1);
  CU_ASSERT(rtp_member_is_validated(m) == RTP_TRUE);


  // packet should be received by the user
  CU_ASSERT(_rtp_rx == RTP_TRUE);
  CU_ASSERT(_rtp_payload_len == 64);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_rtp_normal_flow_with_csrc(void)
{
  rtp_session_t*  sess;
  rtp_hdr_t*      hdr;
  uint8_t         buf[256];
  rtp_member_t*   m;
  int seq;
  uint32_t csrc_list[] = { 2000, 2001, 2002, 2003 };

  sess = common_session_init();
  sess->rx_rtp = dummy_rx_rtp;

  hdr = (rtp_hdr_t*)buf;

  hdr->version = RTP_VERSION;
  hdr->cc = sizeof(csrc_list)/sizeof(uint32_t);
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 0;
  hdr->ssrc = htonl(1234);
  hdr->seq = htons(10);

  for(int i = 0; i < sizeof(csrc_list)/sizeof(uint32_t); i++)
  {
    hdr->csrc[i] = htonl(csrc_list[i]);
  }

  // first packet
  rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(4, 64), &_rtp_rem_addr);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_member_first_heard);
  m = rtp_session_lookup_member(sess, 1234);
  CU_ASSERT(m != NULL);

  // CSRCs are not created until SSRC is validated
  for(int i = 0; i < sizeof(csrc_list)/sizeof(uint32_t); i++)
  {
    CU_ASSERT(rtp_session_lookup_member(sess, csrc_list[i]) == NULL);
  }

  for(seq = 0; seq < (RTP_CONFIG_MIN_SEQUENTIAL-1); seq++)
  {
    // following rtp packet
    hdr->seq = htons(10 + seq);
    rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(4, 64), &_rtp_rem_addr);

    CU_ASSERT(sess->last_rtp_error == rtp_rx_error_seq_error);

    for(int i = 0; i < sizeof(csrc_list)/sizeof(uint32_t); i++)
    {
      CU_ASSERT(rtp_session_lookup_member(sess, csrc_list[i]) == NULL);
    }
  }

  // probation finished.
  // CSRCs should be created
  hdr->seq = htons(10 + seq);
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64, &_rtp_rem_addr);

  for(int i = 0; i < sizeof(csrc_list)/sizeof(uint32_t); i++)
  {
    m = rtp_session_lookup_member(sess, csrc_list[i]);
    CU_ASSERT(m != NULL);
    CU_ASSERT(rtp_member_is_validated(m) == RTP_TRUE);
    CU_ASSERT(rtp_member_is_rtp_heard(m) == RTP_TRUE);
    CU_ASSERT(is_soft_timer_running(&m->sender_te) != 0);
    CU_ASSERT(is_soft_timer_running(&m->member_te) != 0);
  }

  CU_ASSERT(sess->rtcp_var.members == 6);
  CU_ASSERT(sess->rtcp_var.senders == 5);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_rtp_sender_member_timeout(void)
{
  rtp_session_t*  sess;
  rtp_hdr_t*    hdr;
  uint8_t      buf[256];
  int seq;
  uint32_t csrc_list[] = { 2000, 2001, 2002, 2003 };

  sess = common_session_init();
  sess->rx_rtp = dummy_rx_rtp;

  hdr = (rtp_hdr_t*)buf;

  hdr->version = RTP_VERSION;
  hdr->cc = sizeof(csrc_list)/sizeof(uint32_t);
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 0;
  hdr->ssrc = htonl(1234);
  hdr->seq = htons(10);

  for(int i = 0; i < sizeof(csrc_list)/sizeof(uint32_t); i++)
  {
    hdr->csrc[i] = htonl(csrc_list[i]);
  }

  // first packet
  rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(4, 64), &_rtp_rem_addr);

  for(seq = 0; seq < (RTP_CONFIG_MIN_SEQUENTIAL-1); seq++)
  {
    // following rtp packet
    hdr->seq = htons(10 + seq);
    rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(4, 64), &_rtp_rem_addr);
  }

  // probation finished.
  hdr->seq = htons(10 + seq);
  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64, &_rtp_rem_addr);

  CU_ASSERT(sess->rtcp_var.members == 6);
  CU_ASSERT(sess->rtcp_var.senders == 5);

  // now let the sender timeout
  for(uint32_t i = 0; i < (RTP_CONFIG_SENDER_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_session_timer_tick(sess);
  }

  CU_ASSERT(sess->rtcp_var.members == 6);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  for(int i = 0; i < sizeof(csrc_list)/sizeof(uint32_t); i++)
  {
    rtp_member_t* m;

    m = rtp_session_lookup_member(sess, csrc_list[i]);
    CU_ASSERT(m != NULL);
    CU_ASSERT(rtp_member_is_rtp_heard(m) == RTP_FALSE);
  }

  // now let the member timeout
  // XXX longer than config but it should be ok for the testing
  for(uint32_t i = 0; i < (RTP_CONFIG_MEMBER_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_session_timer_tick(sess);
  }

  for(int i = 0; i < sizeof(csrc_list)/sizeof(uint32_t); i++)
  {
    rtp_member_t* m;

    m = rtp_session_lookup_member(sess, csrc_list[i]);
    CU_ASSERT(m == NULL);
  }

  CU_ASSERT(sess->rtcp_var.members == 1);
  CU_ASSERT(sess->rtcp_var.senders == 0);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_rtp_created_first_by_rtcp(void)
{
  // FIXME
}

static void
test_rtp_3rd_party_conflict(void)
{
  rtp_session_t*  sess;
  rtp_hdr_t*    hdr;
  uint8_t      buf[256];
  int seq;

  sess = common_session_init();
  sess->rx_rtp = dummy_rx_rtp;

  hdr = (rtp_hdr_t*)buf;

  hdr->version = RTP_VERSION;
  hdr->cc = 0;
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 0;
  hdr->ssrc = htonl(1234);
  hdr->seq = htons(10);

  // first packet
  rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(0, 64), &_rtp_rem_addr);

  for(seq = 0; seq < (RTP_CONFIG_MIN_SEQUENTIAL-1); seq++)
  {
    // following rtp packet
    hdr->seq = htons(10 + seq);
    rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(0, 64), &_rtp_rem_addr);
  }

  // probation finished.
  hdr->seq = htons(10 + seq);
  rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(0, 64), &_rtp_rem_addr);

  CU_ASSERT(sess->rtcp_var.members == 2);
  CU_ASSERT(sess->rtcp_var.senders == 1);

  CU_ASSERT(_rtp_rx == RTP_TRUE);
  CU_ASSERT(_rtp_payload_len == 64);

  _rtp_rx = RTP_FALSE;
  _rtp_payload_len = 0;

  // now cause a conflict
  rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(0, 64), &_rtp_rem_addr2);

  CU_ASSERT(_rtp_rx == RTP_FALSE);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_3rd_party_conflict);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_rtp_own_ssrc_conflict(void)
{
  rtp_session_t*  sess;
  rtp_hdr_t*    hdr;
  uint8_t      buf[256];

  sess = common_session_init();
  sess->rx_rtp = dummy_rx_rtp;

  hdr = (rtp_hdr_t*)buf;

  hdr->version = RTP_VERSION;
  hdr->cc = 0;
  hdr->pt = SESSION_PT;
  hdr->p = 0;
  hdr->x = 0;
  hdr->ssrc = htonl(TEST_OWN_SSRC);
  hdr->seq = htons(10);

  // first packet
  rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(0, 64), &_rtp_rem_addr);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_ssrc_conflict);
  CU_ASSERT(sess->self->ssrc != TEST_OWN_SSRC);
  CU_ASSERT(rtp_source_conflict_lookup(&sess->src_conflict, &_rtp_rem_addr) == RTP_TRUE);

  // second looped packet after ssrc change
  // should be filtered by ssrc conflict list
  //
  hdr->ssrc = htonl(sess->self->ssrc);
  rtp_session_rx_rtp(sess, buf, RTP_PKT_SIZE(0, 64), &_rtp_rem_addr);
  CU_ASSERT(sess->last_rtp_error == rtp_rx_error_source_in_conflict_list);

  // let the conflict entry timeout
  for(uint32_t i = 0; i < (RTP_CONFIG_SOURCE_CONFLICT_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_session_timer_tick(sess);
  }
  CU_ASSERT(rtp_source_conflict_lookup(&sess->src_conflict, &_rtp_rem_addr) == RTP_FALSE);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_rtp_member_by_rtcp(void)
{
  rtcp_encoder_t    enc;
  uint32_t          pkt_len;
  rtp_session_t*    sess;
  rtp_member_t*     m;

  uint8_t           buf[256];
  rtp_hdr_t*        hdr;

  sess = common_session_init();
  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  // first, RTCP packet
  rtcp_encoder_sr_begin(&enc,
      1001,
      1, 2, 3, 4, 5);
  rtcp_encoder_end_packet(&enc);
  pkt_len = rtcp_encoder_msg_len(&enc);
  rtp_session_rx_rtcp(sess, enc.buf, pkt_len, &_rtcp_rem_addr);
  CU_ASSERT(sess->last_rtcp_error == rtcp_rx_error_member_first_heard);

  // second, RTP packet
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

  rtcp_encoder_deinit(&enc);
  rtp_session_deinit(sess);
}

void
test_rtp_add(CU_pSuite pSuite)
{
  CU_add_test(pSuite, "rtp::check", test_rtp_check);
  CU_add_test(pSuite, "rtp::tx", test_rtp_tx);
  CU_add_test(pSuite, "rtp::normal_flow", test_rtp_normal_flow);
  CU_add_test(pSuite, "rtp::normal_flow_with_csrc", test_rtp_normal_flow_with_csrc);
  CU_add_test(pSuite, "rtp::sender_member_timeout", test_rtp_sender_member_timeout);
  CU_add_test(pSuite, "rtp::created_first_by_rtcp", test_rtp_created_first_by_rtcp);
  CU_add_test(pSuite, "rtp::3rd_party_conflict", test_rtp_3rd_party_conflict);
  CU_add_test(pSuite, "rtp::own_ssrc_conflict", test_rtp_own_ssrc_conflict);
  CU_add_test(pSuite, "rtp::member_by_rtcp", test_rtp_member_by_rtcp);
}
