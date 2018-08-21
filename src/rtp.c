#include "rtp.h"
#include "rtcp.h"
#include "rtp_timers.h"
#include "rtp_session_util.h"
#include "rtp_error.h"

static const char* TAG = "rtp";

////////////////////////////////////////////////////////////
//
// RTP sequence number
//
////////////////////////////////////////////////////////////
static inline void
rtp_init_seq(rtp_source_t* s, uint16_t seq, uint8_t first)
{
  s->base_seq       = seq;
  s->max_seq        = seq;
  s->bad_seq        = RTP_SEQ_MOD + 1;    /* so seq == bad_seq is false */
  s->cycles         = 0;
  s->received       = 0;
  s->received_prior = 0;
  s->expected_prior = 0;

  if(first == RTP_TRUE)
  {
    s->max_seq        = seq - 1;
    s->probation      = RTP_CONFIG_MIN_SEQUENTIAL;
  }
}

static inline uint8_t
rtp_update_seq(rtp_source_t* s, uint16_t seq)
{
  uint16_t udelta = seq - s->max_seq;

  /*
   * Source is not valid until RTP_CONFIG_MIN_SEQUENTIAL packets with
   * sequential sequence numbers have been received.
   */
  if(s->probation)
  {
    /* packet is in sequence */
    if(seq == s->max_seq + 1)
    {
      s->probation--;
      s->max_seq = seq;
      if(s->probation == 0)
      {
        rtp_init_seq(s, seq, RTP_FALSE);
        s->received++;
        return RTP_TRUE;
      }
    }
    else
    {
      s->probation = RTP_CONFIG_MIN_SEQUENTIAL - 1;
      s->max_seq = seq;
    }
    return RTP_FALSE;
  }
  else if(udelta < RTP_CONFIG_MAX_DROPOUT)
  {
    /* in order, with permissible gap */
    if(seq < s->max_seq)
    {
      /*
       * Sequence number wrapped - count another 64K cycle.
       */
      s->cycles += RTP_SEQ_MOD;
    }
    s->max_seq = seq;
  }
  else if(udelta <= RTP_SEQ_MOD - RTP_CONFIG_MAX_MISORDER)
  {
    /* the sequence number made a very large jump */
    if(seq == s->bad_seq)
    {
      /*
       * Two sequential packets -- assume that the other side
       * restarted without telling us so just re-sync
       * (i.e., pretend this was the first packet).
       */
      rtp_init_seq(s, seq, RTP_TRUE);
    }
    else
    {
      s->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
      return RTP_FALSE;
    }
  }
  else
  {
    /* duplicate or reordered packet */
  }
  s->received++;
  return RTP_TRUE;
}

////////////////////////////////////////////////////////////
//
// RTP packet check
//
////////////////////////////////////////////////////////////
static inline uint8_t
rtp_header_validity_check(rtp_session_t* sess, uint8_t* msg, uint32_t len, uint8_t pt, uint8_t** payload, uint32_t* payload_len)
{
  rtp_hdr_t* hdr = (rtp_hdr_t*)msg;
  uint32_t hdr_size;
  uint8_t padding_len = 0;

  //
  // from RFC3550 A.1 RTP Data Header Validity Checks
  //

  // The length of the packet must be consistent with CC
  if(len < (sizeof(rtp_hdr_t) - 4))
  {
    sess->last_rtp_error = rtp_rx_error_header_too_short;
    RTPLOGE(TAG, "rtp packet too small: %d\n", len);
    return RTP_FALSE;
  }

  hdr_size = (sizeof(rtp_hdr_t) - 4) + (sizeof(uint32_t) * hdr->cc);
  if(len < hdr_size)
  {
    sess->last_rtp_error = rtp_rx_error_invalid_csrc_count;
    RTPLOGE(TAG, "rtp invalid hdr size: %d:%d\n", len, hdr_size);
    return RTP_FALSE;
  }


  // RTP version field must equal 2
  if(hdr->version != RTP_VERSION)
  {
    sess->last_rtp_error = rtp_rx_error_invalid_rtp_version;
    RTPLOGE(TAG, "invalid RTP version %d\n", hdr->version);
    return RTP_FALSE;
  }

  // The payload type must be known, and in particular it must not be
  // equal to SR or RR
  if(hdr->pt != pt)
  {
    sess->last_rtp_error = rtp_rx_error_invalid_payload_type;
    RTPLOGE(TAG, "payload type mismatch %d:%d\n", pt, hdr->pt);
    return RTP_FALSE;
  }

  // If the P bit is set, then the last octet of the packet must contain
  // a valid octet count, in particular, less than the total
  // packet length minus the header size
  if(hdr->p)
  {
    padding_len = msg[len-1];

    if(padding_len >= (len - hdr_size))
    {
      RTPLOGE(TAG, "P bit is set. Invalid last octet count: %d:%d\n", padding_len, len);
      sess->last_rtp_error = rtp_rx_error_invalid_octet_count;
      return RTP_FALSE;
    }
  }

  // The X bit must be zero if the profile does not specify that the
  // header extension mechanism may be used. Otherwise, the extension
  // length field must be less than the total packet size minus the
  // fixed header length and padding
  //
  // FIXME : allow extension
  // In current version, we don't allow extension
  //
  if(hdr->x)
  {
    sess->last_rtp_error = rtp_rx_error_not_implemented;
    RTPLOGE(TAG, "NOT IMPLEMENTED. Extension bit is set\n");
    return RTP_FALSE;
  }

  //
  // The length of the packet must be consistent with CC and payload
  // type (if payloads have a known length).
  // XXX: done at the beginning. payload length check must be done by user.
  //

  *payload      = &msg[hdr_size];
  *payload_len  = (len - hdr_size - padding_len);
  return RTP_TRUE;
}

////////////////////////////////////////////////////////////
//
// RTP RX Procedure for a SSRC
//
////////////////////////////////////////////////////////////
static rtp_member_t*
rtp_handle_ssrc(rtp_session_t* sess, uint32_t ssrc, uint16_t seq, struct sockaddr_in* from, uint8_t csrc)
{
  rtp_member_t*   m;

  m = rtp_session_lookup_member(sess, ssrc);
  if(m == NULL)
  {
    //
    // new member
    //
    m = rtp_session_alloc_member(sess, ssrc);
    if(m == NULL)
    {
      sess->last_rtp_error = rtp_rx_error_member_alloc_failed;
      RTPLOGE(TAG, "failed to rtp_session_alloc_member: %d\n", ssrc);
      return NULL;
    }

    rtp_init_seq(&m->rtp_src, seq, RTP_TRUE);
    memcpy(&m->rtp_addr, from, sizeof(struct sockaddr_in));
    rtp_member_set_rtp_heard(m);

    rtcp_interval_handle_rtp_event(sess, RTP_TRUE, RTP_TRUE);

    rtp_timers_sender_start(sess, m);
    rtp_timers_member_start(sess, m);

    sess->last_rtp_error = rtp_rx_error_member_first_heard;

    if(csrc == RTP_TRUE)
    {
      rtp_member_set_validated(m);
      rtp_member_set_csrc(m);
      return m;
    }

    return NULL;
  }

  //
  // rtp member found
  //
  if(rtp_member_is_bye_received(m))
  {
    sess->last_rtp_error = rtp_rx_error_member_bye_in_progress;
    return NULL;
  }

  //
  // created by RTCP and RTP first heard
  //
  if(rtp_member_is_self(m) == RTP_FALSE && rtp_member_is_rtp_heard(m) == RTP_FALSE)
  {
    // first RTP packet
    rtp_init_seq(&m->rtp_src, seq, RTP_TRUE);
    memcpy(&m->rtp_addr, from, sizeof(struct sockaddr_in));
    rtp_member_set_rtp_heard(m);

    rtcp_interval_handle_rtp_event(sess, RTP_FALSE, RTP_TRUE);

    rtp_timers_sender_start(sess, m);
    rtp_timers_member_restart(sess, m);

    sess->last_rtp_error = rtp_rx_error_member_first_heard;

    return NULL;
  }

  //
  // source transport address doesn't match
  //
  if(memcmp(&m->rtp_addr, from, sizeof(struct sockaddr_in)) != 0)
  {
    //
    // an identifier collision or a loop is detected

    // source identifier is not the participants' own
    if(rtp_member_is_self(m) == RTP_FALSE)
    {
      // FIXME
      // count third-party loop/collision
      sess->last_rtp_error = rtp_rx_error_3rd_party_conflict;
      return NULL;
    }
    // a collision or loop of the participant's own packets

    // source transport address is found in the list of
    // conflicting data or control source transport address
    //
    // XXX
    // a) for own SSRC collision with a valid 3rd party member
    //    by changing our own SSRC, next time own member won't be picked up by the SSRC
    //
    // b) for looped packet,
    //    the source is basically blocked by the following logic
    //
    if(rtp_source_conflict_lookup(&sess->src_conflict, from) == RTP_TRUE)
    {
      // abort processing of data packet or control element
      sess->last_rtp_error = rtp_rx_error_source_in_conflict_list;
      return NULL;
    }

    // new collision, change SSRC identifier
    RTPLOGI(TAG, "ssrc conflict: %d\n", ssrc);

    rtp_source_conflict_add(&sess->src_conflict, from);
    rtcp_tx_bye(sess);

    rtp_member_table_change_random_ssrc(&sess->member_table, m);
    rtp_session_reset_tx_stats(sess);

    sess->last_rtp_error = rtp_rx_error_ssrc_conflict;

    return NULL;
  }
  
  rtp_member_set_rtp_heard(m);

  rtp_timers_sender_restart(sess, m);
  rtp_timers_member_restart(sess, m);

  return m;
}

////////////////////////////////////////////////////////////
//
// public interfaces
//
////////////////////////////////////////////////////////////
void
rtp_init(rtp_session_t* sess)
{
}

void
rtp_deinit(rtp_session_t* sess)
{
}

void
rtp_rx(rtp_session_t* sess, uint8_t* pkt, uint32_t len, struct sockaddr_in* from)
{
  rtp_member_t*   m;
  uint8_t*        payload;
  uint32_t        payload_len;
  rtp_hdr_t*      hdr;
  uint32_t        arrival = rtp_session_timestamp(sess);
  uint32_t        csrc_list[15];

  if(rtp_header_validity_check(sess, pkt, len, sess->config.pt, &payload, &payload_len) == RTP_FALSE)
  {
    sess->invalid_rtp_pkt++;
    return;
  }

  hdr = (rtp_hdr_t*)pkt;

  m = rtp_handle_ssrc(sess, ntohl(hdr->ssrc), ntohs(hdr->seq), from, RTP_FALSE);
  if(m == NULL)
  {
    return;
  }

  if(rtp_update_seq(&m->rtp_src, ntohs(hdr->seq)) == RTP_FALSE)
  {
    sess->last_rtp_error = rtp_rx_error_seq_error;
    return;
  }

  rtcp_compute_jitter(sess, m, ntohl(hdr->ts), arrival);

  rtp_member_set_validated(m);

  // FIXME review
  // for a validated ssrc member
  // we check CSRC too
  // CSRC check
  //
  for(uint32_t i = 0; i < hdr->cc; i++)
  {
    // XXX
    // confusing.
    // if an SSRC has a list of CSRCs,
    // is it better to assume that SSRC is a sender or not?
    //
    csrc_list[i] = ntohl(hdr->csrc[i]);

    m = rtp_handle_ssrc(sess, ntohl(hdr->csrc[i]), ntohs(hdr->seq), from, RTP_TRUE);
    if(m != NULL)
    {
      rtcp_compute_jitter(sess, m, ntohl(hdr->ts), arrival);
    }
  }

  sess->last_rtp_error = rtp_rx_error_no_error;

  {
    rtp_rx_report_t rpt;

    rpt.payload     = payload;
    rpt.payload_len = payload_len;
    rpt.rtp_ts      = ntohl(hdr->ts);
    rpt.seq         = ntohs(hdr->seq);
    rpt.csrc        = csrc_list;
    rpt.ncsrc       = hdr->cc;

    sess->rx_rtp(sess, &rpt);
  }
}

void
rtp_tx(rtp_session_t* sess, uint8_t* payload, uint32_t payload_len, uint32_t rtp_ts, uint32_t* csrc, uint8_t ncsrc)
{
  rtp_hdr_t*  hdr;
  uint32_t    pkt_size;

  if(sess->rtcp_var.we_sent == RTP_FALSE)
  {
    sess->rtcp_var.we_sent = RTP_TRUE;

    rtp_member_set_sender(sess->self);
    rtcp_interval_handle_rtp_event(sess, RTP_FALSE, RTP_TRUE);
  }

  rtp_timers_sender_restart(sess, sess->self);

  sess->tx_pkt_count++;
  sess->tx_octet_count += payload_len;

  if(RTP_PKT_SIZE(ncsrc, payload_len) > RTP_CONFIG_MAX_RTP_PKT_SIZE)
  {
    RTPLOGE(TAG, "pkt size too big\n");
    return;
  }

  //
  // XXX
  // later, this code should be changed to more efficient one
  //
  memcpy(&sess->rtp_pkt[RTP_HDR_SIZE(ncsrc)], payload, payload_len);

  hdr = (rtp_hdr_t*)&sess->rtp_pkt[0];

  hdr->version  = RTP_VERSION;
  hdr->p        = 0;
  hdr->x        = 0;
  hdr->cc       = ncsrc;
  hdr->m        = 0;
  hdr->pt       = sess->config.pt;
  hdr->seq      = htons(sess->seq);
  hdr->ts       = htonl(rtp_ts);
  hdr->ssrc     = htonl(sess->self->ssrc);

  for(uint8_t i = 0; i < ncsrc; i++)
  {
    hdr->csrc[i] = htonl(csrc[i]);
  }

  pkt_size = RTP_PKT_SIZE(ncsrc, payload_len);

  if(sess->config.align_by_4 == RTP_TRUE)
  {
    uint8_t   mod = pkt_size % 4;

    if(mod != 0)
    {
      uint8_t   pad;
      pad = 4 - mod;

      if((pkt_size + 4) > RTP_CONFIG_MAX_RTP_PKT_SIZE)
      {
        RTPLOGE(TAG, "pkt size too big. can't pad\n");
        return;
      }

      for(uint8_t i = 0; i < pad - 1; i++)
      {
        sess->rtp_pkt[pkt_size++] = 0;
      }
      sess->rtp_pkt[pkt_size++] = pad;
      hdr->p  = 1;
    }
  }

  sess->tx_rtp(sess, sess->rtp_pkt, pkt_size);

  sess->seq++;
}
