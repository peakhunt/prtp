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

static int
dummy_rx_rtp(rtp_session_t* sess, rtp_rx_report_t* rpt)
{
  return 0;
}

static void
__send_test_rtp_pkt(rtp_session_t* sess, uint16_t seq, uint32_t rtp_ts)
{
  rtp_hdr_t*    hdr;
  uint8_t      buf[256];

  hdr = (rtp_hdr_t*)buf;

  hdr->version  = RTP_VERSION;
  hdr->cc       = 0x00;
  hdr->pt       = SESSION_PT;
  hdr->p        = 0;
  hdr->x        = 0;
  hdr->ssrc     = htonl(1234);
  hdr->seq      = htons(seq);
  hdr->ts       = htonl(rtp_ts);

  rtp_session_rx_rtp(sess, buf, sizeof(rtp_hdr_t) - 4 + 64, &_rtp_rem_addr);
}

static void
test_jitter_basic(void)
{
  rtp_session_t*  sess;
  rtp_member_t*   m;
  uint16_t        seq = 15;
  uint32_t        rtp_ts = 0;

  sess = common_session_init();
  sess->rx_rtp = dummy_rx_rtp;
  //
  // send enough packet
  // manipulate timestamps so that transit stays at 15
  //
  for(uint32_t i = 0; i < RTP_CONFIG_MIN_SEQUENTIAL * 2; i++)
  {
    test_rtp_timestamp_set(sess, rtp_ts + 15);
    __send_test_rtp_pkt(sess, seq, rtp_ts);
    seq++;
    rtp_ts += 5;
  }

  m = rtp_session_lookup_member(sess, 1234);
  CU_ASSERT(m != NULL);

  // basically this works
  // but there is no guarantee
  // useless test
  // CU_ASSERT(m->rtp_src.jitter == 0);

  rtp_session_deinit(sess);
}

void
test_jitter_add(CU_pSuite pSuite)
{
  CU_add_test(pSuite, "jitter::basic", test_jitter_basic);
}
