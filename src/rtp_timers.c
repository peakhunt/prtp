#include "rtp_timers.h"
#include "rtcp.h"
#include "rtp_session_util.h"

//
// 6.3.8. Updating we_sent (sender timer)
//
// set to true if participant sent any RTP packet, and  add to sender list
// set to false after tc -2T, and remove from sender list
//

//
// 6.3.5. Timing Out an SSRC
// Any other session member who has not sent an RTP or RTCP packet
// since time tc - mTd (M is the timeout multiplier, and defualt to 5) is
// timed out. This means that its SSRC is removed from the member list,
// and members is updated.
//

//
// member leaver timer
//

//
// RTCP interval timer
//
static void
__rtp_timers_member_timedout(SoftTimerElem* te)
{
  rtp_session_t*    sess = (rtp_session_t*)te->priv;
  rtp_member_t*     m = container_of(te, rtp_member_t, member_te);

  rtcp_interval_member_timedout(sess, m);
}

static void
__rtp_timers_sender_timedout(SoftTimerElem* te)
{
  rtp_session_t*    sess = (rtp_session_t*)te->priv;
  rtp_member_t*     m = container_of(te, rtp_member_t, sender_te);

  rtcp_interval_sender_timedout(sess, m);
}

static void
__rtp_timers_leave_timedout(SoftTimerElem* te)
{
  rtp_session_t*    sess = (rtp_session_t*)te->priv;
  rtp_member_t*     m = container_of(te, rtp_member_t, leave_te);

  rtp_session_dealloc_member(sess, m);
}

////////////////////////////////////////////////////////////
//
// timer init
//
////////////////////////////////////////////////////////////
void
rtp_timers_init_member(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_init_elem(&m->member_te);
  m->member_te.cb   = __rtp_timers_member_timedout;
  m->member_te.priv = sess;

  soft_timer_init_elem(&m->sender_te);
  m->sender_te.cb   = __rtp_timers_sender_timedout;
  m->sender_te.priv = sess;

  soft_timer_init_elem(&m->leave_te);
  m->leave_te.cb   = __rtp_timers_leave_timedout;
  m->leave_te.priv = sess;
}

void
rtp_timers_deinit_member(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_del(&sess->soft_timer, &m->member_te);
  soft_timer_del(&sess->soft_timer, &m->sender_te);
  soft_timer_del(&sess->soft_timer, &m->leave_te);
}

////////////////////////////////////////////////////////////
//
// RTP sender timer 
//
////////////////////////////////////////////////////////////
void
rtp_timers_sender_start(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_add(&sess->soft_timer, &m->sender_te, RTP_CONFIG_SENDER_TIMEOUT);
}

void
rtp_timers_sender_stop(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_del(&sess->soft_timer, &m->sender_te);
}

void
rtp_timers_sender_restart(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_del(&sess->soft_timer, &m->sender_te);
  soft_timer_add(&sess->soft_timer, &m->sender_te, RTP_CONFIG_SENDER_TIMEOUT);
}

////////////////////////////////////////////////////////////
//
// RTP member timer
//
////////////////////////////////////////////////////////////
void
rtp_timers_member_start(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_add(&sess->soft_timer, &m->member_te, RTP_CONFIG_MEMBER_TIMEOUT);
}

void
rtp_timers_member_stop(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_del(&sess->soft_timer, &m->member_te);
}

void
rtp_timers_member_restart(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_del(&sess->soft_timer, &m->member_te);
  soft_timer_add(&sess->soft_timer, &m->member_te, RTP_CONFIG_MEMBER_TIMEOUT);
}

////////////////////////////////////////////////////////////
//
// RTP leave timer
//
////////////////////////////////////////////////////////////
void
rtp_timers_leave_start(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_add(&sess->soft_timer, &m->leave_te, RTP_CONFIG_LEAVE_TIMEOUT);
}

void
rtp_timers_leave_stop(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_del(&sess->soft_timer, &m->leave_te);
}

void
rtp_timers_leave_restart(rtp_session_t* sess, rtp_member_t* m)
{
  soft_timer_del(&sess->soft_timer, &m->leave_te);
  soft_timer_add(&sess->soft_timer, &m->leave_te, RTP_CONFIG_LEAVE_TIMEOUT);
}
