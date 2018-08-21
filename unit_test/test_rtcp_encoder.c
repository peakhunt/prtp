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
test_rtcp_encoder_basic(void)
{
  rtcp_encoder_t    enc;

  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  CU_ASSERT(enc.buf_len == RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);
  CU_ASSERT(enc.write_ndx == 0);
  CU_ASSERT(enc.chunk_begin == 0);
  CU_ASSERT(enc.rtcp == (rtcp_t*)&enc.buf[0]);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 0);

  enc.write_ndx = 15;
  enc.rtcp = (rtcp_t*)&enc.buf[1];
  enc.chunk_begin = 13;

  rtcp_encoder_reset(&enc);

  CU_ASSERT(enc.buf_len == RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);
  CU_ASSERT(enc.write_ndx == 0);
  CU_ASSERT(enc.chunk_begin == 0);
  CU_ASSERT(enc.rtcp == (rtcp_t*)&enc.buf[0]);
  CU_ASSERT(rtcp_encoder_space_left(&enc) == RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  rtcp_encoder_deinit(&enc);

  rtcp_encoder_init(&enc, 1);
  CU_ASSERT(rtcp_encoder_space_left(&enc) == 1);

  rtcp_encoder_deinit(&enc);
}

static void
test_rtcp_encoder_sr(void)
{
  int               ndx;
  rtcp_encoder_t    enc;

  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  CU_ASSERT(
    rtcp_encoder_sr_begin(&enc,
      0,
      1, 2,
      3,
      4,
      5) == RTP_TRUE
  );
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 28);

  CU_ASSERT(((enc.buf[0] & 0xc0) >> 6) == RTP_VERSION);     // version
  CU_ASSERT(((enc.buf[0] & 0x20) >> 5) == 0);               // padding
  CU_ASSERT(((enc.buf[0] & 0x1f) >> 0) == 0);               // rc
  CU_ASSERT(((enc.buf[1] & 0xff) >> 0) == RTCP_SR);         // pt

  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[4]) == 0);           // ssrc
  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[8]) == 1);           // ntp msb
  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[12]) == 2);          // ntp lsb
  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[16]) == 3);          // rtp ts
  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[20]) == 4);          // pkt count
  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[24]) == 5);          // octet count

  for(int i = 0; i < 4; i++)
  {
    ndx = 28 + i * 24;
    CU_ASSERT(
        rtcp_encoder_sr_add_rr(&enc,
          100 + i,
          101 + i,
          0x010203,
          103 + i,
          104 + i,
          105 + i,
          106 + i) == RTP_TRUE
    );

    CU_ASSERT(rtcp_encoder_msg_len(&enc) == (28 + ((i+1) * 24)));

    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 0]) == (100 + i));          // ssrc
    CU_ASSERT(enc.buf[ndx + 4] == (101 + i));                              // fraction
    CU_ASSERT(enc.rtcp->r.sr.rr[i].fraction == (101 + i));

    CU_ASSERT(enc.buf[ndx + 5] == 0x01);                             // lost0
    CU_ASSERT(enc.buf[ndx + 6] == 0x02);                             // lost1
    CU_ASSERT(enc.buf[ndx + 7] == 0x03);                             // lost2
    CU_ASSERT(ntohl(enc.rtcp->r.sr.rr[0].lost << 8) == 0x010203);

    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 8]) == (103 + i));          // last seq
    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 12]) == (104 + i));         // jitter
    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 16]) == (105 + i));          // lsr
    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 20]) == (106 + i));          // dlsr

    CU_ASSERT(enc.rtcp->common.count == (i + 1));
  }

  rtcp_encoder_end_packet(&enc);

  CU_ASSERT(ntohs(*(uint16_t*)&enc.buf[2]) == 30);           // length

  rtcp_encoder_deinit(&enc);
}

static void
test_rtcp_encoder_rr(void)
{
  rtcp_encoder_t    enc;
  int               ndx;

  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  CU_ASSERT(rtcp_encoder_rr_begin(&enc, 100) == RTP_TRUE);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 8);

  CU_ASSERT(((enc.buf[0] & 0xc0) >> 6) == RTP_VERSION);     // version
  CU_ASSERT(((enc.buf[0] & 0x20) >> 5) == 0);               // padding
  CU_ASSERT(((enc.buf[0] & 0x1f) >> 0) == 0);               // rc
  CU_ASSERT(((enc.buf[1] & 0xff) >> 0) == RTCP_RR);         // pt

  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[4]) == 100);         // ssrc

  for(int i = 0; i < 4; i++)
  {
    ndx = 8 + i * 24;
    CU_ASSERT(
        rtcp_encoder_rr_add_rr(&enc,
          100 + i,
          101 + i,
          0x010203,
          103 + i,
          104 + i,
          105 + i,
          106 + i) == RTP_TRUE
    );

    CU_ASSERT(rtcp_encoder_msg_len(&enc) == (8 + ((i+1) * 24)));

    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 0]) == (100 + i));          // ssrc
    CU_ASSERT(enc.buf[ndx + 4] == (101 + i));                              // fraction
    CU_ASSERT(enc.rtcp->r.rr.rr[i].fraction == (101 + i));

    CU_ASSERT(enc.buf[ndx + 5] == 0x01);                             // lost0
    CU_ASSERT(enc.buf[ndx + 6] == 0x02);                             // lost1
    CU_ASSERT(enc.buf[ndx + 7] == 0x03);                             // lost2
    CU_ASSERT(ntohl(enc.rtcp->r.rr.rr[0].lost << 8) == 0x010203);

    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 8]) == (103 + i));          // last seq
    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 12]) == (104 + i));         // jitter
    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 16]) == (105 + i));          // lsr
    CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[ndx + 20]) == (106 + i));          // dlsr

    CU_ASSERT(enc.rtcp->common.count == (i + 1));
  }

  rtcp_encoder_end_packet(&enc);
  CU_ASSERT(ntohs(enc.rtcp->common.length) == 25);

  rtcp_encoder_deinit(&enc);
}

static void
test_rtcp_encoder_sdes(void)
{
  rtcp_encoder_t    enc;

  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  CU_ASSERT(rtcp_encoder_sdes_begin(&enc) == RTP_TRUE);

  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 4);

  CU_ASSERT(((enc.buf[0] & 0xc0) >> 6) == RTP_VERSION);     // version
  CU_ASSERT(((enc.buf[0] & 0x20) >> 5) == 0);               // padding
  CU_ASSERT(((enc.buf[0] & 0x1f) >> 0) == 0);               // rc
  CU_ASSERT(((enc.buf[1] & 0xff) >> 0) == RTCP_SDES);       // pt
  
  // empty chunk
  CU_ASSERT(rtcp_encoder_sdes_chunk_begin(&enc, 100) == RTP_TRUE);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 8);
  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[4]) == 100);         // ssrc

  CU_ASSERT(rtcp_encoder_sdes_chunk_end(&enc) == RTP_TRUE);
  CU_ASSERT(enc.buf[8] == 0);
  CU_ASSERT(enc.buf[9] == 0);
  CU_ASSERT(enc.buf[10] == 0);
  CU_ASSERT(enc.buf[11] == 0);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 12);
  // len so far: 2

  // chunk with cname: end of chunk 4 byte aligned
  CU_ASSERT(rtcp_encoder_sdes_chunk_begin(&enc, 101) == RTP_TRUE);

  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[12]) == 101);         // ssrc
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 16);

  CU_ASSERT(rtcp_encoder_sdes_chunk_add_cname(&enc, (uint8_t*)"12347", 5) == RTP_TRUE);
  CU_ASSERT(enc.buf[16] == RTCP_SDES_CNAME);
  CU_ASSERT(enc.buf[17] == 5);
  CU_ASSERT(enc.buf[18] == '1');
  CU_ASSERT(enc.buf[19] == '2');
  CU_ASSERT(enc.buf[20] == '3');
  CU_ASSERT(enc.buf[21] == '4');
  CU_ASSERT(enc.buf[22] == '7');
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 23);

  CU_ASSERT(rtcp_encoder_sdes_chunk_end(&enc) == RTP_TRUE);
  CU_ASSERT(enc.buf[23] == 0);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 24);
  // len so far: 5

  // chunk with cname: end of chunk not aligned
  CU_ASSERT(rtcp_encoder_sdes_chunk_begin(&enc, 102) == RTP_TRUE);

  CU_ASSERT(ntohl(*(uint32_t*)&enc.buf[24]) == 102);         // ssrc
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 28);

  CU_ASSERT(rtcp_encoder_sdes_chunk_add_cname(&enc, (uint8_t*)"abcdefgh", 8) == RTP_TRUE);
  CU_ASSERT(enc.buf[28] == RTCP_SDES_CNAME);
  CU_ASSERT(enc.buf[29] == 8);
  CU_ASSERT(enc.buf[30] == 'a');
  CU_ASSERT(enc.buf[31] == 'b');
  CU_ASSERT(enc.buf[32] == 'c');
  CU_ASSERT(enc.buf[33] == 'd');
  CU_ASSERT(enc.buf[34] == 'e');
  CU_ASSERT(enc.buf[35] == 'f');
  CU_ASSERT(enc.buf[36] == 'g');
  CU_ASSERT(enc.buf[37] == 'h');
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 38);
  CU_ASSERT(rtcp_encoder_sdes_chunk_end(&enc) == RTP_TRUE);
  CU_ASSERT(enc.buf[38] == 0);
  CU_ASSERT(enc.buf[39] == 0);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 40);

  rtcp_encoder_end_packet(&enc);
  CU_ASSERT(ntohs(enc.rtcp->common.length) == 9);
  CU_ASSERT(enc.rtcp->common.count == 3);

  rtcp_encoder_deinit(&enc);
}

static void
test_rtcp_encoder_bye(void)
{
  rtcp_encoder_t    enc;

  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  CU_ASSERT(rtcp_encoder_bye_begin(&enc) == RTP_TRUE);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 4);

  CU_ASSERT(((enc.buf[0] & 0xc0) >> 6) == RTP_VERSION);     // version
  CU_ASSERT(((enc.buf[0] & 0x20) >> 5) == 0);               // padding
  CU_ASSERT(((enc.buf[0] & 0x1f) >> 0) == 0);               // rc
  CU_ASSERT(((enc.buf[1] & 0xff) >> 0) == RTCP_BYE);       // pt

  // zero length bye packet
  rtcp_encoder_end_packet(&enc);
  CU_ASSERT(ntohs(enc.rtcp->common.length) == 0);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 4);

  rtcp_encoder_reset(&enc);

  CU_ASSERT(rtcp_encoder_bye_begin(&enc) == RTP_TRUE);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 4);

  CU_ASSERT(((enc.buf[0] & 0xc0) >> 6) == RTP_VERSION);     // version
  CU_ASSERT(((enc.buf[0] & 0x20) >> 5) == 0);               // padding
  CU_ASSERT(((enc.buf[0] & 0x1f) >> 0) == 0);               // rc
  CU_ASSERT(((enc.buf[1] & 0xff) >> 0) == RTCP_BYE);       // pt

  for(uint8_t i = 0; i < 5; i++)
  {
    CU_ASSERT(rtcp_encoder_bye_add_ssrc(&enc, 100 + i) == RTP_TRUE);
    CU_ASSERT(ntohl(enc.rtcp->r.bye.src[i]) == (100 + i));
    CU_ASSERT(rtcp_encoder_msg_len(&enc) == 4 + (4*(i+1)));
    CU_ASSERT(enc.rtcp->common.count == (i + 1));
  }

  // bye with no reason
  rtcp_encoder_end_packet(&enc);
  CU_ASSERT(ntohs(enc.rtcp->common.length) == 5);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 24);

  rtcp_encoder_reset(&enc);

  // bye with reason
  CU_ASSERT(rtcp_encoder_bye_begin(&enc) == RTP_TRUE);
  for(uint8_t i = 0; i < 5; i++)
  {
    CU_ASSERT(rtcp_encoder_bye_add_ssrc(&enc, 100 + i) == RTP_TRUE);
  }
  CU_ASSERT(rtcp_encoder_bye_add_reason(&enc, "Good Bye!") == RTP_TRUE);
  CU_ASSERT(enc.buf[24] == 9);
  CU_ASSERT(memcmp(&enc.buf[25], "Good Bye!", 9) == 0);
  CU_ASSERT(enc.buf[34] == 0);
  CU_ASSERT(enc.buf[35] == 0);

  rtcp_encoder_end_packet(&enc);

  CU_ASSERT(enc.rtcp->common.count == 5);
  CU_ASSERT(ntohs(enc.rtcp->common.length) == 8);
  CU_ASSERT(rtcp_encoder_msg_len(&enc) == 36);

  rtcp_encoder_deinit(&enc);
}

void
test_rtcp_encder_add(CU_pSuite pSuite)
{
  CU_add_test(pSuite, "rtcp_encoder::basic", test_rtcp_encoder_basic);
  CU_add_test(pSuite, "rtcp_encoder::sr", test_rtcp_encoder_sr);
  CU_add_test(pSuite, "rtcp_encoder::rr", test_rtcp_encoder_rr);
  CU_add_test(pSuite, "rtcp_encoder::sdes", test_rtcp_encoder_sdes);
  CU_add_test(pSuite, "rtcp_encoder::bye", test_rtcp_encoder_bye);
}
