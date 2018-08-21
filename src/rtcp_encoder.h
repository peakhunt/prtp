#ifndef __RTCP_ENCODER_DEF_H__
#define __RTCP_ENCODER_DEF_H__

#include "common_inc.h"
#include "rfc3550.h"

typedef struct
{
  uint8_t           buf[RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN];
  rtcp_t*           rtcp;
  uint32_t          buf_len;
  uint32_t          write_ndx;
  uint32_t          chunk_begin;
} rtcp_encoder_t;

extern void rtcp_encoder_init(rtcp_encoder_t* re, uint32_t buf_len);
extern void rtcp_encoder_reset(rtcp_encoder_t* re);
extern void rtcp_encoder_deinit(rtcp_encoder_t* re);
extern uint32_t rtcp_encoder_msg_len(rtcp_encoder_t* re);

extern uint8_t rtcp_encoder_sr_begin(rtcp_encoder_t* re,
    uint32_t ssrc,
    uint32_t ntp_msb,
    uint32_t ntp_lsb,
    uint32_t rtp_ts,
    uint32_t pkt_count,
    uint32_t octet_count);
extern uint8_t rtcp_encoder_sr_add_rr(rtcp_encoder_t* re,
    uint32_t ssrc,
    uint8_t fraction,
    uint32_t pkt_lost,
    uint32_t highest_seq,
    uint32_t jitter,
    uint32_t lsr,
    uint32_t dlsr);

extern uint8_t rtcp_encoder_rr_begin(rtcp_encoder_t* re, uint32_t ssrc);
extern uint8_t rtcp_encoder_rr_add_rr(rtcp_encoder_t* re,
    uint32_t ssrc,
    uint8_t fraction,
    uint32_t pkt_lost,
    uint32_t highest_seq,
    uint32_t jitter,
    uint32_t lsr,
    uint32_t dlsr);

extern uint8_t rtcp_encoder_sdes_begin(rtcp_encoder_t* re);
extern uint8_t rtcp_encoder_sdes_chunk_begin(rtcp_encoder_t* re, uint32_t ssrc);
extern uint8_t rtcp_encoder_sdes_chunk_add_cname(rtcp_encoder_t* re, const uint8_t* name, uint8_t len);
extern uint8_t rtcp_encoder_sdes_chunk_end(rtcp_encoder_t* re);

extern uint8_t rtcp_encoder_bye_begin(rtcp_encoder_t* re);
extern uint8_t rtcp_encoder_bye_add_ssrc(rtcp_encoder_t* re, uint32_t ssrc);
extern uint8_t rtcp_encoder_bye_add_reason(rtcp_encoder_t* re, const char* reason);

extern void rtcp_encoder_end_packet(rtcp_encoder_t* re);

static inline uint32_t
rtcp_encoder_space_left(rtcp_encoder_t* re)
{
  return re->buf_len - re->write_ndx;
}

#endif /* !__RTCP_ENCODER_DEF_H__ */
