#include <stdlib.h>
#include <time.h>
#include "rtcp.h"
#include "rtp_timers.h"
#include "rtp_session_util.h"
#include "rtcp_encoder.h"

#define RTCP_INTERVAL_FLAGS_MEMBER        0x01
#define RTCP_INTERVAL_FLAGS_SENDER        0x02

#define RTCP_EVENT_RX_NON_BYE             0x01
#define RTCP_EVENT_RX_BYE                 0x02
#define RTCP_EVENT_TIMEOUT                0x04

static const char* TAG = "rtcp";

static inline double rtcp_interval_calc(rtcp_control_var_t* cvar);
static uint32_t rtcp_send_report(rtp_session_t* sess);

////////////////////////////////////////////////////////////
//
// RTCP current time
//
////////////////////////////////////////////////////////////
/**
 *
 * @returns time in second
 *  that is, 1030ms becomes 1.03
 */
static inline double
rtcp_current_time(rtp_session_t* sess)
{
#if 0
  struct timespec now;
  double          ret;

  clock_gettime(CLOCK_REALTIME, &now);

  ret = now.tv_sec +  now.tv_nsec / 1000000000.0;
  return ret;
#else
  // this is much better for unit testing
  unsigned long tick_time = soft_timer_get_tick_time(&sess->soft_timer);

  return tick_time / 1000.;
#endif
}

////////////////////////////////////////////////////////////
//
// RTCP packet check
//
////////////////////////////////////////////////////////////
static inline uint8_t
rtcp_check_pkt(rtp_session_t* sess, uint8_t* pkt, uint32_t len)
{
  uint32_t    len_4 = len / 4;
  rtcp_t      *r,
              *end;

  if(len < sizeof(rtcp_common_t))
  {
    RTPLOGE(TAG, "packet is smaller than minimum header: %d\n", len);
    sess->last_rtcp_error = rtcp_rx_error_header_too_short;
    return RTP_FALSE;
  }

  if((len % 4) != 0)
  {
    RTPLOGE(TAG, "len is not multiple of 4: %d\n", len);
    sess->last_rtcp_error = rtcp_rx_error_len_not_multiple_4;
    return RTP_FALSE;
  }

  r = (rtcp_t*)pkt;

  if ((*(uint16_t *)r & RTCP_VALID_MASK) != RTCP_VALID_VALUE)
  {
    RTPLOGE(TAG, "invalid header for the first rtcp packet %d\n", *(uint16_t *)r);
    sess->last_rtcp_error = rtcp_rx_error_invalid_mask;
    return RTP_FALSE;
  }

  end = (rtcp_t *)((uint32_t*)r + len_4);

  do
  {
    RTPLOGI(TAG, "compound pkt: len: %d\n", ntohs(r->common.length));
    r = (rtcp_t *)((uint32_t *)r + ntohs(r->common.length) + 1);
  }
  while (r < end && r->common.version == 2);

  if (r != end) {
    sess->last_rtcp_error = rtcp_rx_error_invalid_compound_packet;
    RTPLOGE(TAG, "invalid compound packet\n");
    return 0;
  }

  RTPLOGI(TAG, "rtcp pkt validation pass\n");

  sess->last_rtcp_error = rtcp_rx_error_no_error;

  return RTP_TRUE;
}

////////////////////////////////////////////////////////////
//
// RTCP interval timer
//
////////////////////////////////////////////////////////////

static inline uint32_t
rtcp_interval_cal_delta_in_ms(double tc, double tn)
{
  int delta = (uint32_t)((tn - tc) * 1000);

  if(delta <= 0)
  {
    //
    // in case delta is 0 or negative, just return 1ms as a hack
    //
    delta = 1;
  }

  return (uint32_t)delta;
}

static inline void
rtcp_interval_schedule(rtp_session_t* sess, double tc, double tn)
{
  sess->rtcp_var.tn = tn;

  // RTPLOGI(TAG, "rtcp_interval_schedule %.2f %.2f\n", tc, tn);
  soft_timer_add(&sess->soft_timer, &sess->rtcp_timer, rtcp_interval_cal_delta_in_ms(tc, tn));
}

static inline void
rtcp_interval_stop_timer(rtp_session_t* sess)
{
  soft_timer_del(&sess->soft_timer, &sess->rtcp_timer);
}

static inline void
rtcp_interval_reschedule(rtp_session_t* sess, double tc, double tn)
{
  sess->rtcp_var.tn = tn;

  RTPLOGI(TAG, "rtcp_interval_reschedule %.2f %.2f\n", tc, tn);

  soft_timer_del(&sess->soft_timer, &sess->rtcp_timer);
  soft_timer_add(&sess->soft_timer, &sess->rtcp_timer, rtcp_interval_cal_delta_in_ms(tc, tn));
}

static void
__rtcp_interval_timeout(SoftTimerElem* te)
{
  /* This function is responsible for deciding whether to send an
   * RTCP report or BYE packet now, or to reschedule transmission.
   * It is also responsible for updating the pmembers, initial, tp,
   * and avg_rtcp_size state variables.  This function should be
   * called upon expiration of the event timer used by Schedule().
   */
  rtp_session_t*      sess = container_of(te, rtp_session_t, rtcp_timer);
  rtcp_control_var_t* cvar = &sess->rtcp_var;
  double              t;     /* Interval */
  double              tn;    /* Next transmit time */
  double              pkt_size;
  double              tc = rtcp_current_time(sess);

  // RTPLOGI(TAG, "__rtcp_interval_timeout\n");

   /* In the case of a BYE, we use "timer reconsideration" to
    * reschedule the transmission of the BYE if necessary */
  // FIXME : we don't support TX BYE timeout at the moment
  if(0)
  {
    t   = rtcp_interval_calc(cvar);
    tn  = cvar->tp + t;

    if(tn <= tc)
    {
      // FIXME
      // send bye packet(e);
      // finish session
    }
    else
    {
      rtcp_interval_schedule(sess, tc, tn);
    }
  }
  else
  {
    t = rtcp_interval_calc(cvar);
    tn = cvar->tp + t;

    // RTPLOGI(TAG, "XXX tn = %f, tc = %f\n", tn, tc);

    if(tn <= tc)
    {
      pkt_size = rtcp_send_report(sess);

      cvar->avg_rtcp_size = (1./16.) * pkt_size + (15./16.)*(cvar->avg_rtcp_size);
      cvar->tp = tc;

      // we must redraw the interval. Don't reuse the
      // one computed above, since it is not actually
      // distributed the same, as we are conditioned
      // on it being small enough to cause a packet to
      // be sent.
      t = rtcp_interval_calc(cvar);

      rtcp_interval_schedule(sess, tc, t + tc);

      cvar->initial = RTP_FALSE;
    }
    else
    {
      // RTPLOGI(TAG, "skipping and scheduling again\n");
      rtcp_interval_schedule(sess, tc, tn);
    }
  }
}

////////////////////////////////////////////////////////////
//
// RTCP interval calculation
//
////////////////////////////////////////////////////////////
static inline double
rtcp_interval_calc(rtcp_control_var_t* cvar)
{
  // RFC 3550, A.7 Computing the RTCP Transmission Interval

  /*
   * Minimum average time between RTCP packets from this site (in
   * seconds).  This time prevents the reports from `clumping' when
   * sessions are small and the law of large numbers isn't helping
   * to smooth out the traffic.  It also keeps the report interval
   * from becoming ridiculously small during transient outages like
   * a network partition.
   */
  double const RTCP_MIN_TIME = 5.;
  /*
   * Fraction of the RTCP bandwidth to be shared among active
   * senders.  (This fraction was chosen so that in a typical
   * session with one or two active senders, the computed report
   * time would be roughly equal to the minimum report time so that
   * we don't unnecessarily slow down receiver reports.)  The
   * receiver fraction must be 1 - the sender fraction.
   */
  double const RTCP_SENDER_BW_FRACTION = 0.25;
  double const RTCP_RCVR_BW_FRACTION = (1-RTCP_SENDER_BW_FRACTION);
  /*
   * To compensate for "timer reconsideration" converging to a
   * value below the intended average.
   */
  double const COMPENSATION = 2.71828 - 1.5;

  double t;                   /* interval */
  double rtcp_min_time = RTCP_MIN_TIME;
  int n;                      /* no. of members for computation */

  double rtcp_bw;

  /*
   * Very first call at application start-up uses half the min
   * delay for quicker notification while still allowing some time
   * before reporting for randomization and to learn about other
   * sources so the report interval will converge to the correct
   * interval more quickly.

   */
  if(cvar->initial == RTP_TRUE)
  {
    rtcp_min_time /= 2;
  }

  /*
   * Dedicate a fraction of the RTCP bandwidth to senders unless
   * the number of senders is large enough that their share is
   * more than that fraction.
   */
  rtcp_bw = cvar->rtcp_bw;

  n = cvar->members;
  if(cvar->senders <= cvar->members * RTCP_SENDER_BW_FRACTION)
  {
    if(cvar->we_sent)
    {
      rtcp_bw *= RTCP_SENDER_BW_FRACTION;
      n = cvar->senders;
    }
    else
    {
      rtcp_bw *= RTCP_RCVR_BW_FRACTION;
      n -= cvar->senders;
    }
  }

  /*
   * The effective number of sites times the average packet size is
   * the total number of octets sent when each site sends a report.
   * Dividing this by the effective bandwidth gives the time
   * interval over which those packets must be sent in order to
   * meet the bandwidth target, with a minimum enforced.  In that
   * time interval we send one report so this time is also our
   * average time between reports.
   */
  t = cvar->avg_rtcp_size * n / rtcp_bw;
  if(t < rtcp_min_time)
  {
    t = rtcp_min_time;
  }

  /*
   * To avoid traffic bursts from unintended synchronization with
   * other sites, we then pick our actual next report interval as a
   * random number uniformly distributed between 0.5*t and 1.5*t.
   */
  t = t * (drand48() + 0.5);
  t = t / COMPENSATION;
  return t;
}

static inline void
rtcp_interval_control_var_init(rtp_session_t* sess)
{
  rtcp_control_var_t* cvar = &sess->rtcp_var;
  //
  // RFC 3550, 6.3.2 Initialization
  //
  cvar->tp              = 0;
  cvar->senders         = 0;
  cvar->pmembers        = 1;
  cvar->members         = 1;
  cvar->we_sent         = RTP_FALSE;
  cvar->initial         = RTP_TRUE;
  cvar->rtcp_bw         = sess->config.session_bw;
  cvar->avg_rtcp_size   = RTP_CONFIG_AVERAGE_RTCP_SIZE;
}

void
rtcp_interval_handle_rtp_event(rtp_session_t* sess, uint8_t new_member, uint8_t new_sender)
{
  rtcp_control_var_t* cvar = &sess->rtcp_var;

  if(new_member)
  {
    // RTPLOGI(TAG, "rtp members += 1\n");
    cvar->members += 1;
  }

  if(new_sender)
  {
    // RTPLOGI(TAG, "rtp senders += 1\n");
    cvar->senders += 1;
  }
}

static void
rtcp_interval_handle_rtcp_event(rtp_session_t* sess, uint32_t event, uint32_t flags, uint32_t pkt_size)
{
  rtcp_control_var_t* cvar = &sess->rtcp_var;
  double              tc = rtcp_current_time(sess);
  double              tn = cvar->tn;

  switch(event)
  {
  case RTCP_EVENT_RX_NON_BYE:
    if(flags & RTCP_INTERVAL_FLAGS_MEMBER)
    {
    RTPLOGI(TAG, "rtcp senders += 1\n");
      cvar->members += 1;
    }

    cvar->avg_rtcp_size = (1./16.) * pkt_size + (15./16.) * (cvar->avg_rtcp_size);
    break;

  case RTCP_EVENT_RX_BYE:
  case RTCP_EVENT_TIMEOUT:
    if(flags & RTCP_INTERVAL_FLAGS_SENDER)
    {
      RTPLOGI(TAG, "rtcp senders -= 1\n");
      cvar->senders -= 1;
    }

    if(flags & RTCP_INTERVAL_FLAGS_MEMBER)
    {
      RTPLOGI(TAG, "rtcp members -= 1\n");
      cvar->members -= 1;
    }

    if (cvar->members < cvar->pmembers)
    {
      tn = tc + (((double) cvar->members)/(cvar->pmembers))*(tn - tc);
      cvar->tp = tc - (((double) cvar->members)/(cvar->pmembers))*(tc - cvar->tp);

      rtcp_interval_reschedule(sess, tc, tn);

      cvar->pmembers = cvar->members;
    }
    break;
  }
}

void
rtcp_interval_tx_bye(rtp_session_t* sess)
{
  rtcp_control_var_t* cvar = &sess->rtcp_var;

  cvar->members += 1;
}

////////////////////////////////////////////////////////////
//
// RTCP member timeout
//
////////////////////////////////////////////////////////////
void
rtcp_interval_member_timedout(rtp_session_t* sess, rtp_member_t* m)
{
  //
  // delete member from member list
  // members--
  //
  if(rtp_member_is_rtp_heard(m))
  {
    rtcp_interval_handle_rtcp_event(sess, RTCP_EVENT_TIMEOUT, RTCP_INTERVAL_FLAGS_SENDER | RTCP_INTERVAL_FLAGS_MEMBER, 0);
  }
  else
  {
    rtcp_interval_handle_rtcp_event(sess, RTCP_EVENT_TIMEOUT, RTCP_INTERVAL_FLAGS_MEMBER, 0);
  }
  rtp_session_dealloc_member(sess, m);
}

void
rtcp_interval_sender_timedout(rtp_session_t* sess, rtp_member_t* m)
{
  //
  // delete member from sender list
  // senders--
  //
  rtp_member_clear_rtp_heard(m);
  if(sess->self == m)
  {
    rtp_member_clear_sender(m);
    sess->rtcp_var.we_sent = RTP_FALSE;
  }

  rtcp_interval_handle_rtcp_event(sess, RTCP_EVENT_TIMEOUT, RTCP_INTERVAL_FLAGS_SENDER, 0);
}

////////////////////////////////////////////////////////////
//
// send RTCP report
//
////////////////////////////////////////////////////////////
#define REPORT_RET_IF_FALSE(a)      \
if(a == RTP_FALSE)                  \
{                                   \
  RTPLOGE(TAG, "rtcp_send_report encoder buffer overflow %d\n", __LINE__);\
  return 0; \
}

static uint8_t
rtcp_send_report_gen_rr(rtp_session_t* sess, rtcp_encoder_t* enc, uint8_t sr, ntp_ts_t* now)
{
  //
  // FIXME
  // 
  // in case there are many members,
  // send rr in round robin fashion
  //
  rtp_member_t*     m;

  m = rtp_member_table_get_first(&sess->member_table);
  while(m != NULL)
  {
    if(!(rtp_member_is_self(m) || rtp_member_is_bye_received(m)))
    {
      //
      // RFC3550 A.3
      //
      uint32_t  extended_max;
      uint32_t  expected;
      int32_t   lost;
      uint32_t  expected_interval;
      uint32_t  received_interval;
      uint32_t  lost_interval;
      uint8_t   fraction;
      rtp_source_t* s;
      uint32_t  dlsr;

      s = &m->rtp_src;

      extended_max = s->cycles + s->max_seq;
      expected = extended_max - s->base_seq + 1;
      lost = expected - s->received;

      if(lost >= 0) lost &= 0x7fffff;
      else lost &= 0x800000;

      expected_interval = expected - s->expected_prior;
      s->expected_prior = expected;

      received_interval = s->received - s->received_prior;
      s->received_prior = s->received;

      lost_interval = expected_interval - received_interval;

      if(m->last_sr_local_time.second == 0 && m->last_sr_local_time.fraction == 0)
      {
        dlsr = 0;
      }
      else
      {
        ntp_ts_t  now;

        ntp_ts_now(&now);

        dlsr = (uint32_t)(ntp_ts_diff_in_sec(&m->last_sr_local_time, &now) * 65536);
      }
      
      if (expected_interval == 0 || lost_interval <= 0) fraction = 0;
      else fraction = (lost_interval << 8) / expected_interval;

      if(sr)
      {
        REPORT_RET_IF_FALSE(
          rtcp_encoder_sr_add_rr(enc,
            m->ssrc,
            fraction,
            lost,
            extended_max,
            s->jitter,
            m->last_sr.second << 16 | m->last_sr.fraction >> 16,
            dlsr
          )
        )
      }
      else
      {
        REPORT_RET_IF_FALSE(
          rtcp_encoder_rr_add_rr(enc,
            m->ssrc,
            fraction,
            lost,
            extended_max,
            s->jitter,
            m->last_sr.second << 16 | m->last_sr.fraction >> 16,
            dlsr
          )
        )
      }
    }
    m = rtp_member_table_get_next(&sess->member_table, m);
  }
  return RTP_TRUE;
}

static uint32_t
rtcp_send_report(rtp_session_t* sess)
{
  rtcp_encoder_t  enc;
  ntp_ts_t        ts;
  uint32_t        rtp_ts;
  uint32_t        pkt_len;

  RTPLOGI(TAG, "RTCP ==> TX\n");

  rtcp_encoder_init(&enc, RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN);

  ntp_ts_now(&ts);
  rtp_ts = rtp_session_timestamp(sess);

  // send and return packet size
  if(rtp_member_is_sender(sess->self) == RTP_TRUE)
  {
    // SR
    REPORT_RET_IF_FALSE(
    rtcp_encoder_sr_begin(&enc,
        sess->self->ssrc,
        ts.second,
        ts.fraction,
        rtp_ts,
        sess->tx_pkt_count,
        sess->tx_octet_count)
    );

    REPORT_RET_IF_FALSE(
    rtcp_send_report_gen_rr(sess, &enc, RTP_TRUE, &ts)
    );

    rtcp_encoder_end_packet(&enc);
  }
  else
  {
    // RR
    REPORT_RET_IF_FALSE(
    rtcp_encoder_rr_begin(&enc, sess->self->ssrc)
    );
    
    REPORT_RET_IF_FALSE(
    rtcp_send_report_gen_rr(sess, &enc, RTP_FALSE, &ts)
    );

    rtcp_encoder_end_packet(&enc);
  }

  // SDES CNAME
  REPORT_RET_IF_FALSE(rtcp_encoder_sdes_begin(&enc));
  REPORT_RET_IF_FALSE(rtcp_encoder_sdes_chunk_begin(&enc, sess->self->ssrc));
  REPORT_RET_IF_FALSE(rtcp_encoder_sdes_chunk_add_cname(&enc, sess->self->cname, sess->self->cname_len));
  REPORT_RET_IF_FALSE(rtcp_encoder_sdes_chunk_end(&enc));
  rtcp_encoder_end_packet(&enc);

  pkt_len = rtcp_encoder_msg_len(&enc);

  sess->tx_rtcp(sess, enc.buf, pkt_len);

  rtcp_encoder_deinit(&enc);

  return pkt_len;
}

////////////////////////////////////////////////////////////
//
// RTCP RX Procedure for a SSRC
//
////////////////////////////////////////////////////////////
static rtp_member_t*
rtcp_handle_ssrc(rtp_session_t* sess, uint32_t ssrc, struct sockaddr_in* from, uint32_t pkt_size)
{
  rtp_member_t*   m;

  m = rtp_member_table_lookup(&sess->member_table, ssrc);
  if(m == NULL)
  {
    //
    // new member
    //
    m= rtp_session_alloc_member(sess, ssrc);
    if(m == NULL)
    {
      RTPLOGE(TAG, "failed to rtp_session_alloc_member: %d\n", ssrc);
      sess->last_rtcp_error = rtcp_rx_error_member_alloc_failed;
      return NULL;
    }
    memcpy(&m->rtcp_addr, from, sizeof(struct sockaddr_in));
    rtp_member_set_rtcp_heard(m);

    rtcp_interval_handle_rtcp_event(sess, RTCP_EVENT_RX_NON_BYE, RTCP_INTERVAL_FLAGS_MEMBER, pkt_size);

    rtp_timers_member_start(sess, m);

    sess->last_rtcp_error = rtcp_rx_error_member_first_heard;

    return m;
  }

  //
  // rtp member found
  //
  if(rtp_member_is_bye_received(m))
  {
    sess->last_rtcp_error = rtcp_rx_error_member_bye_in_progress;
    return NULL;
  }

  //
  // created by RTP and RTCP first heard
  //
  if(rtp_member_is_self(m) == RTP_FALSE && rtp_member_is_rtcp_heard(m) == RTP_FALSE)
  {
    memcpy(&m->rtcp_addr, from, sizeof(struct sockaddr_in));

    rtp_member_set_rtcp_heard(m);

    rtp_timers_member_restart(sess, m);
    sess->last_rtcp_error = rtcp_rx_error_member_first_heard;
    return m;
  }

  //
  // source transport address doesn't match
  //
  if(memcmp(&m->rtcp_addr, from, sizeof(struct sockaddr_in)) != 0)
  {
    // an identifier collision or a loop is detected

    if(ssrc != sess->self->ssrc)
    {
      sess->last_rtcp_error = rtcp_rx_error_3rd_party_conflict;
      // FIXME
      // count third-party loop/collision
      return NULL;
    }

    // a collision or loop of the participant's own packets

    // source transport address is found in the list of
    // conflicting data or control source transport address
    if(rtp_source_conflict_lookup(&sess->src_conflict, from) == RTP_TRUE)
    {
      // abort processing of data packet or control element
      sess->last_rtcp_error = rtcp_rx_error_source_in_conflict_list;
      return NULL;
    }

    RTPLOGI(TAG, "ssrc conflict: %d\n", ssrc);

    rtp_source_conflict_add(&sess->src_conflict, from);
    rtcp_tx_bye(sess);

    rtp_member_table_change_random_ssrc(&sess->member_table, m);
    rtp_session_reset_tx_stats(sess);

    sess->last_rtcp_error = rtcp_rx_error_ssrc_conflict;
    return NULL;
  }

  rtp_member_set_rtcp_heard(m);
  rtp_timers_member_restart(sess, m);

  rtcp_interval_handle_rtcp_event(sess, RTCP_EVENT_RX_NON_BYE, 0, pkt_size);
  sess->last_rtcp_error = rtcp_rx_error_no_error;

  return m;
}

////////////////////////////////////////////////////////////
//
// RTCP Handling
//
////////////////////////////////////////////////////////////
static void
rtcp_handle_rr_item(rtp_session_t* sess, uint32_t from_ssrc, rtcp_rr_t* rr)
{
  sess->rr_rpt(sess, from_ssrc, rr);
}

static void
rtcp_handle_sr(rtp_session_t* sess, rtcp_t* r, struct sockaddr_in* from, uint32_t cpkt_size)
{
  rtp_member_t*   m;

  m = rtcp_handle_ssrc(sess, ntohl(r->r.sr.ssrc), from, cpkt_size);
  if(m == NULL)
  {
    return;
  }

  // from sender report, create rr for the member
  m->last_sr.second   = ntohl(r->r.sr.ntp_sec);
  m->last_sr.fraction = ntohl(r->r.sr.ntp_frac);
  m->rtp_ts           = ntohl(r->r.sr.rtp_ts);
  m->pkt_count        = ntohl(r->r.sr.psent);
  m->octet_count      = ntohl(r->r.sr.osent);

  ntp_ts_now(&m->last_sr_local_time);

  for(uint8_t i = 0; i < r->common.count; i++)
  {
    rtcp_handle_rr_item(sess, ntohl(r->r.sr.ssrc), &r->r.sr.rr[i]);
  }
}

static void
rtcp_handle_rr(rtp_session_t* sess, rtcp_t* r, struct sockaddr_in* from, uint32_t cpkt_size)
{
  rtp_member_t*   m;

  m = rtcp_handle_ssrc(sess, ntohl(r->r.rr.ssrc), from, cpkt_size);
  if(m == NULL)
  {
    return;
  }

  for(uint8_t i = 0; i < r->common.count; i++)
  {
    rtcp_handle_rr_item(sess,ntohl(r->r.rr.ssrc),  &r->r.rr.rr[i]);
  }
}

static void
rtcp_handle_sdes(rtp_session_t* sess, rtcp_t* r, struct sockaddr_in* from, uint32_t cpkt_size)
{
  int count                 = r->common.count;
  rtcp_sdes_t *sd           = &r->r.sdes;
  rtcp_sdes_item_t          *rsp, *rspn;
  rtcp_sdes_item_t          *end = (rtcp_sdes_item_t *)((uint32_t *)r + r->common.length + 1);
  rtp_member_t*             m;

  while(--count >= 0)
  {
    rsp = &sd->item[0];
    if (rsp >= end)
      break;

    m = rtcp_handle_ssrc(sess, ntohl(sd->src), from, cpkt_size);

    for(; rsp->type; rsp = rspn )
    {
      rspn = (rtcp_sdes_item_t *)((char*)rsp+rsp->length+2);

      if(rspn >= end)
      {
        rsp = rspn;
        break;
      }

      if(m != NULL && rsp->type == RTCP_SDES_CNAME)
      {
        rtp_member_set_cname(m, (uint8_t*)rsp->data, rsp->length);
        rtp_member_set_validated(m);
      }
    }
    
    sd = (rtcp_sdes_t *)((uint32_t*)sd + (((char *)rsp - (char *)sd) >> 2)+1);
  }

  if (count >= 0)
  {
    /* XXX: invalid packet format */
  }
}

static void
rtcp_handle_bye(rtp_session_t* sess, rtcp_t* r, struct sockaddr_in* from, uint32_t pkt_size)
{
  rtp_member_t*       m;

  for(int i = 0; i < r->common.count; i++)
  {
    m = rtp_session_lookup_member(sess, ntohl(r->r.bye.src[i]));
    if(m == NULL)
    {
      continue;
    }

    if(rtp_member_is_bye_received(m) == RTP_TRUE)
    {
      continue;
    }

    // bye handling
    RTPLOGI(TAG, "bye received from %d\n", m->ssrc);

    if(rtp_member_is_rtp_heard(m))
    {
      RTPLOGI(TAG, "bye sender & member");
      rtcp_interval_handle_rtcp_event(sess, RTCP_EVENT_RX_BYE,
          RTCP_INTERVAL_FLAGS_SENDER | RTCP_INTERVAL_FLAGS_MEMBER,
          pkt_size);
    }
    else
    {
      RTPLOGI(TAG, "bye member");
      rtcp_interval_handle_rtcp_event(sess, RTCP_EVENT_RX_BYE,
          RTCP_INTERVAL_FLAGS_MEMBER, 
          pkt_size);
    }

    rtp_member_set_bye_received(m);

    rtp_timers_sender_stop(sess, m);
    rtp_timers_member_stop(sess, m);

    rtp_timers_leave_start(sess, m);
  }
}

////////////////////////////////////////////////////////////
//
// public interfaces
//
////////////////////////////////////////////////////////////
void
rtcp_init(rtp_session_t* sess)
{
  double tc = rtcp_current_time(sess);

  rtcp_interval_control_var_init(sess);

  soft_timer_init_elem(&sess->rtcp_timer);
  sess->rtcp_timer.cb = __rtcp_interval_timeout;

  rtcp_interval_schedule(sess, tc, tc + rtcp_interval_calc(&sess->rtcp_var));
}

void
rtcp_deinit(rtp_session_t* sess)
{
  rtcp_interval_stop_timer(sess);
}

void
rtcp_rx(rtp_session_t* sess, uint8_t* pkt, uint32_t len, struct sockaddr_in* from)
{
  rtcp_t      *r,
              *end;
  uint32_t    len_4 = len / 4;

  if(rtcp_check_pkt(sess, pkt, len) == RTP_FALSE)
  {
    sess->invalid_rtcp_pkt++;
    return;
  }

  r = (rtcp_t*)pkt;
  end = (rtcp_t *)((uint32_t*)r + len_4);

  while(r < end)
  {
    // r is a rtcp packet inside a compound rtcp packet
    switch(r->common.pt)
    {
    case RTCP_SR:
      RTPLOGI(TAG, "rx RTCP_SR\n");
      rtcp_handle_sr(sess, r, from, len);
      break;

    case RTCP_RR:
      RTPLOGI(TAG, "rx RTCP_RR\n");
      rtcp_handle_rr(sess, r, from, len);
      break;

    case RTCP_SDES:
      RTPLOGI(TAG, "rx RTCP_SDES\n");
      rtcp_handle_sdes(sess, r, from, len);
      break;

    case RTCP_BYE:
      RTPLOGI(TAG, "rx RTCP_BYE\n");
      rtcp_handle_bye(sess, r, from, len);
      break;

    case RTCP_APP:
      RTPLOGI(TAG, "RTCP_APP handling not yet IMPLEMENTED\n");
      break;

    default:
      RTPLOGI(TAG, "Unknown RTCP Packet %d\n", r->common.pt);
      break;
    }
    r = (rtcp_t*)((uint32_t*)r + (ntohs(r->common.length) + 1));
  }
}

void
rtcp_tx_bye(rtp_session_t* sess)
{
  //
  // XXX We don't implement BYE TX
  // Instead, we just let myself timeout on remote
  // by not sending anything with the self's SSRC
  //
}

void
rtcp_compute_jitter(rtp_session_t* sess, rtp_member_t* m, uint32_t rtp_ts, uint32_t arrival)
{
  //
  // RFC3550 A.8
  //
  rtp_source_t* s = &m->rtp_src;
  uint32_t transit = arrival - rtp_ts;
  int d = transit - s->transit;

  s->transit = transit;

  if(d < 0)
  {
    d = -d;
  }

  s->jitter += (1./16.) * ((double)d - s->jitter);
}
