#include "rtcp_encoder.h"

static inline void
rtcp_encoder_set_current_pkt(rtcp_encoder_t* re)
{
  re->rtcp = (rtcp_t*)&re->buf[re->write_ndx];

  re->rtcp->common.version  = RTP_VERSION;
  re->rtcp->common.p        = 0;
  re->rtcp->common.count    = 0;
  re->rtcp->common.length   = htons(0);
}

void
rtcp_encoder_init(rtcp_encoder_t* re, uint32_t buf_len)
{
  re->buf_len   = buf_len;

  rtcp_encoder_reset(re);
}

void
rtcp_encoder_reset(rtcp_encoder_t* re)
{
  re->write_ndx   = 0;
  re->chunk_begin = 0;
  rtcp_encoder_set_current_pkt(re);
}

void
rtcp_encoder_deinit(rtcp_encoder_t* re)
{
  // nothing to do with this implementation
}

uint32_t
rtcp_encoder_msg_len(rtcp_encoder_t* re)
{
  return re->write_ndx;
}

void
rtcp_encoder_end_packet(rtcp_encoder_t* re)
{
  uint8_t*    current = &re->buf[re->write_ndx];
  uint8_t*    begin = (uint8_t*)re->rtcp;
  uint32_t    len_in_bytes = (uint32_t)(current - begin);

  if ((len_in_bytes % 4) != 0)
  {
    RTPCRASH("BUG RTCP Packet is not 4 byte aligned");
  }
  re->rtcp->common.length = htons(len_in_bytes / 4 - 1);
}

uint8_t
rtcp_encoder_sr_begin(rtcp_encoder_t* re,
    uint32_t ssrc,
    uint32_t ntp_msb,
    uint32_t ntp_lsb,
    uint32_t rtp_ts,
    uint32_t pkt_count,
    uint32_t octet_count)
{
  #define RTCP_SR_SIZE_IN_4BYTES_WITH_HEADER        7

  if(rtcp_encoder_space_left(re) < (RTCP_SR_SIZE_IN_4BYTES_WITH_HEADER*4))
  {
    return RTP_FALSE;
  }

  rtcp_encoder_set_current_pkt(re);

  re->rtcp->common.pt       = RTCP_SR;

  re->rtcp->r.sr.ssrc      = htonl(ssrc);
  re->rtcp->r.sr.ntp_sec   = htonl(ntp_msb);
  re->rtcp->r.sr.ntp_frac  = htonl(ntp_lsb);
  re->rtcp->r.sr.rtp_ts    = htonl(rtp_ts);
  re->rtcp->r.sr.psent     = htonl(pkt_count);
  re->rtcp->r.sr.osent     = htonl(octet_count);

  re->write_ndx += (RTCP_SR_SIZE_IN_4BYTES_WITH_HEADER * 4);

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_sr_add_rr(rtcp_encoder_t* re,
    uint32_t ssrc,
    uint8_t fraction,
    uint32_t pkt_lost,
    uint32_t highest_seq,
    uint32_t jitter,
    uint32_t lsr,
    uint32_t dlsr)
{
  #define RTCP_RR_SIZE_IN_4BYTES          6
  uint8_t   ndx;


  if(rtcp_encoder_space_left(re) < (RTCP_RR_SIZE_IN_4BYTES*4))
  {
    return RTP_FALSE;
  }

  ndx = re->rtcp->common.count;

  re->rtcp->r.sr.rr[ndx].ssrc     = htonl(ssrc);
  re->rtcp->r.sr.rr[ndx].fraction = fraction;
  re->rtcp->r.sr.rr[ndx].lost     = htonl(pkt_lost << 8);     // FIXME big endian
  re->rtcp->r.sr.rr[ndx].last_seq = htonl(highest_seq);
  re->rtcp->r.sr.rr[ndx].jitter   = htonl(jitter);
  re->rtcp->r.sr.rr[ndx].lsr      = htonl(lsr);
  re->rtcp->r.sr.rr[ndx].dlsr     = htonl(dlsr);

  re->rtcp->common.count = (ndx + 1);

  re->write_ndx += (RTCP_RR_SIZE_IN_4BYTES * 4);

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_rr_begin(rtcp_encoder_t* re, uint32_t ssrc)
{
  #define RTCP_RR_SIZE_IN_4BYTES_WITH_HEADER        2

  if(rtcp_encoder_space_left(re) < (RTCP_RR_SIZE_IN_4BYTES_WITH_HEADER*4))
  {
    return RTP_FALSE;
  }

  rtcp_encoder_set_current_pkt(re);

  re->rtcp->common.pt       = RTCP_RR;

  re->rtcp->r.rr.ssrc     = htonl(ssrc);

  re->write_ndx += (RTCP_RR_SIZE_IN_4BYTES_WITH_HEADER * 4);

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_rr_add_rr(rtcp_encoder_t* re,
    uint32_t ssrc,
    uint8_t fraction,
    uint32_t pkt_lost,
    uint32_t highest_seq,
    uint32_t jitter,
    uint32_t lsr,
    uint32_t dlsr)
{
  #define RTCP_RR_SIZE_IN_4BYTES          6
  uint8_t   ndx;

  if(rtcp_encoder_space_left(re) < (RTCP_RR_SIZE_IN_4BYTES*4))
  {
    return RTP_FALSE;
  }

  ndx = re->rtcp->common.count;

  re->rtcp->r.rr.rr[ndx].ssrc     = htonl(ssrc);
  re->rtcp->r.rr.rr[ndx].fraction = fraction;
  re->rtcp->r.rr.rr[ndx].lost     = htonl(pkt_lost << 8); // FIXME big endian
  re->rtcp->r.rr.rr[ndx].last_seq = htonl(highest_seq);
  re->rtcp->r.rr.rr[ndx].jitter   = htonl(jitter);
  re->rtcp->r.rr.rr[ndx].lsr      = htonl(lsr);
  re->rtcp->r.rr.rr[ndx].dlsr     = htonl(dlsr);

  re->rtcp->common.count = (ndx + 1);

  re->write_ndx += (RTCP_RR_SIZE_IN_4BYTES * 4);

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_sdes_begin(rtcp_encoder_t* re)
{
  #define RTCP_SDES_SIZE_IN_4BYTES_WITH_HEADER        1

  if(rtcp_encoder_space_left(re) < (RTCP_SDES_SIZE_IN_4BYTES_WITH_HEADER * 4))
  {
    return RTP_FALSE;
  }

  rtcp_encoder_set_current_pkt(re);

  re->rtcp->common.pt       = RTCP_SDES;

  re->write_ndx += (RTCP_SDES_SIZE_IN_4BYTES_WITH_HEADER * 4);

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_sdes_chunk_begin(rtcp_encoder_t* re, uint32_t ssrc)
{
  #define RTCP_SDES_CHUNK_BEGIN_SIZE_IN_4BYTES          1
  uint8_t   ndx;
  uint32_t* ptr;

  if(rtcp_encoder_space_left(re) < (RTCP_SDES_CHUNK_BEGIN_SIZE_IN_4BYTES * 4))
  {
    return RTP_FALSE;
  }

  ndx = re->rtcp->common.count;

  re->rtcp->common.count = (ndx + 1);

  ptr = (uint32_t*)&re->buf[re->write_ndx];
  *ptr = htonl(ssrc);

  re->chunk_begin = re->write_ndx;

  re->write_ndx += (RTCP_SDES_CHUNK_BEGIN_SIZE_IN_4BYTES * 4);

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_sdes_chunk_add_cname(rtcp_encoder_t* re, const uint8_t* name, uint8_t len)
{
  rtcp_sdes_item_t*   item;
  size_t              name_len = len;

  if(rtcp_encoder_space_left(re) < (name_len + 2))
  {
    return RTP_FALSE;
  }

  item = (rtcp_sdes_item_t*)&re->buf[re->write_ndx];

  item->type    = RTCP_SDES_CNAME;
  item->length  = name_len;

  memcpy(item->data, name, item->length);

  re->write_ndx += (2 + name_len);

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_sdes_chunk_end(rtcp_encoder_t* re)
{
  //
  // including null octet for end of chunk
  //
  uint32_t    chunk_size;

  if(rtcp_encoder_space_left(re) < 1) return RTP_FALSE;

  // end of chunk
  re->buf[re->write_ndx++] = 0;

  chunk_size = (re->write_ndx - re->chunk_begin);

  if((chunk_size % 4) != 0)
  {
    uint32_t pad;

    pad = 4 - (chunk_size % 4);
    if(rtcp_encoder_space_left(re) < pad) return RTP_FALSE;

    for(uint32_t i = 0; i < pad; i++)
    {
      re->buf[re->write_ndx++] = 0;
    }
    chunk_size += pad;
  }

  // now 4 bytes aligned
  // update length of sdes
  return RTP_TRUE;
}

uint8_t
rtcp_encoder_bye_begin(rtcp_encoder_t* re)
{
  #define RTCP_BYE_SIZE_BEGIN_IN_4BYTES_WITH_HEADER        1

  if(rtcp_encoder_space_left(re) < (RTCP_BYE_SIZE_BEGIN_IN_4BYTES_WITH_HEADER*4))
  {
    return RTP_FALSE;
  }

  rtcp_encoder_set_current_pkt(re);

  re->rtcp->common.pt       = RTCP_BYE;

  re->write_ndx += (RTCP_BYE_SIZE_BEGIN_IN_4BYTES_WITH_HEADER * 4);

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_bye_add_ssrc(rtcp_encoder_t* re, uint32_t ssrc)
{
  uint8_t   ndx;

  if(rtcp_encoder_space_left(re) < 4)
  {
    return RTP_FALSE;
  }

  ndx = re->rtcp->common.count;

  re->rtcp->r.bye.src[ndx] = htonl(ssrc);

  re->rtcp->common.count = ndx + 1;

  re->write_ndx += 4;

  return RTP_TRUE;
}

uint8_t
rtcp_encoder_bye_add_reason(rtcp_encoder_t* re, const char* reason)
{
  uint8_t   len = strlen(reason);
  uint32_t  total = (1 + len);  // including length
  uint8_t   pad = 0;

  if((total % 4) != 0)
  {
    pad = 4 - (total % 4);
    total += pad;
  }

  if(rtcp_encoder_space_left(re) < total)
  {
    return RTP_FALSE;
  }

  // length
  re->buf[re->write_ndx++] = len;

  // reason
  memcpy(&re->buf[re->write_ndx], reason, len);
  re->write_ndx += len;

  // padding
  for(uint8_t i = 0; i < pad; i++)
  {
    re->buf[re->write_ndx++] = 0;
  }

  return RTP_TRUE;
}
