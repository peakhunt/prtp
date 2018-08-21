#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#include <stdlib.h>
#include <stdio.h>

#include "rtp_session.h"
#include "rtp_session_util.h"

#include "test_common.h"

static void
test_basic_session_init(void)
{
  rtp_session_t*  sess;
  rtp_member_t*   m;

  sess = common_session_init();

  CU_ASSERT(memcmp(sess->self->cname, SESSION_NAME, sess->self->cname_len) == 0);     // cname


  // RFC3550, 6.3.2 Initialization
  CU_ASSERT(sess->rtcp_var.tp == 0);
  // XXX tc is meaningless
  CU_ASSERT(sess->rtcp_var.pmembers == 1);
  CU_ASSERT(sess->rtcp_var.members == 1);
  CU_ASSERT(sess->rtcp_var.senders == 0);
  CU_ASSERT(sess->rtcp_var.we_sent == RTP_FALSE);
  CU_ASSERT(sess->config.session_bw == (64 * 1000));          // session bandwidth
  CU_ASSERT(sess->rtcp_var.initial == RTP_TRUE);
  CU_ASSERT(sess->rtcp_var.avg_rtcp_size == RTP_CONFIG_AVERAGE_RTCP_SIZE);
  CU_ASSERT(sess->rtcp_var.tn != 0);


  //
  // self
  //
  CU_ASSERT(sess->self != NULL);
  CU_ASSERT(sess->self->ssrc == TEST_OWN_SSRC);
  CU_ASSERT(memcmp(&sess->self->rtp_addr, &_rtp_addr, sizeof(_rtp_addr)) == 0);
  CU_ASSERT(memcmp(&sess->self->rtcp_addr, &_rtcp_addr, sizeof(_rtcp_addr)) == 0);
  CU_ASSERT(rtp_member_is_self(sess->self) == RTP_TRUE);
  CU_ASSERT(rtp_member_is_validated(sess->self) == RTP_FALSE);
  CU_ASSERT(rtp_member_is_sender(sess->self) == RTP_FALSE);
  CU_ASSERT(rtp_member_is_rtp_heard(sess->self) == RTP_FALSE);
  CU_ASSERT(rtp_member_is_rtcp_heard(sess->self) == RTP_FALSE);
  CU_ASSERT(rtp_member_is_bye_received(sess->self) == RTP_FALSE);

  m = rtp_session_lookup_member(sess, TEST_OWN_SSRC);
  CU_ASSERT(m != NULL);
  CU_ASSERT(m == sess->self);

  rtp_session_deinit(sess);
  free(sess);
}

static void
test_basic_self_send(void)
{
  rtp_session_t*  sess;
  rtp_member_t*   m;
  uint8_t         msg[128];

  sess = common_session_init();

  m = sess->self;

  // after first send,
  // we_sent should be true
  // sender timer should start
  //
  rtp_session_tx(sess, msg, 128, 0, NULL, 0);

  CU_ASSERT(sess->rtcp_var.we_sent == RTP_TRUE);
  CU_ASSERT(sess->rtcp_var.senders == 1);
  CU_ASSERT(rtp_member_is_sender(m) == RTP_TRUE);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) != 0);

  //
  // let the sender timer timeout
  //
  for(uint32_t i = 0; i < (RTP_CONFIG_SENDER_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_timer_tick(sess);
  }

  CU_ASSERT(sess->rtcp_var.we_sent == RTP_FALSE);
  CU_ASSERT(sess->rtcp_var.senders == 0);
  CU_ASSERT(rtp_member_is_sender(m) == RTP_FALSE);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) == 0);

  rtp_session_tx(sess, msg, 128, 100, NULL, 0);

  CU_ASSERT(sess->rtcp_var.we_sent == RTP_TRUE);
  CU_ASSERT(sess->rtcp_var.senders == 1);
  CU_ASSERT(rtp_member_is_sender(m) == RTP_TRUE);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) != 0);

  //
  // don't let the sender timer timeout
  //
  for(uint32_t i = 0; i < (RTP_CONFIG_SENDER_TIMEOUT / sess->soft_timer.tick_rate)/2; i++)
  {
    rtp_timer_tick(sess);
  }

  CU_ASSERT(sess->rtcp_var.we_sent == RTP_TRUE);
  CU_ASSERT(sess->rtcp_var.senders == 1);
  CU_ASSERT(rtp_member_is_sender(m) == RTP_TRUE);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) != 0);

  //
  // let the sender timer timeout
  //
  for(uint32_t i = 0; i < (RTP_CONFIG_SENDER_TIMEOUT / sess->soft_timer.tick_rate); i++)
  {
    rtp_timer_tick(sess);
  }

  CU_ASSERT(sess->rtcp_var.we_sent == RTP_FALSE);
  CU_ASSERT(sess->rtcp_var.senders == 0);
  CU_ASSERT(rtp_member_is_sender(m) == RTP_FALSE);
  CU_ASSERT(is_soft_timer_running(&m->sender_te) == 0);

  rtp_session_deinit(sess);
  free(sess);
}

void
test_basic_add(CU_pSuite pSuite)
{
  CU_add_test(pSuite, "basic::session init", test_basic_session_init);
  CU_add_test(pSuite, "basic::self_send", test_basic_self_send);
}
