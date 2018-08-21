#ifndef __RTP_TIMERS_DEF_H__
#define __RTP_TIMERS_DEF_H__

#include "rtp_session.h"

extern void rtp_timers_init_member(rtp_session_t* sess, rtp_member_t* m);
extern void rtp_timers_deinit_member(rtp_session_t* sess, rtp_member_t* m);

extern void rtp_timers_sender_start(rtp_session_t* sess, rtp_member_t* m);
extern void rtp_timers_sender_stop(rtp_session_t* sess, rtp_member_t* m);
extern void rtp_timers_sender_restart(rtp_session_t* sess, rtp_member_t* m);

extern void rtp_timers_member_start(rtp_session_t* sess, rtp_member_t* m);
extern void rtp_timers_member_stop(rtp_session_t* sess, rtp_member_t* m);
extern void rtp_timers_member_restart(rtp_session_t* sess, rtp_member_t* m);

extern void rtp_timers_leave_start(rtp_session_t* sess, rtp_member_t* m);
extern void rtp_timers_leave_stop(rtp_session_t* sess, rtp_member_t* m);
extern void rtp_timers_leave_restart(rtp_session_t* sess, rtp_member_t* m);

#endif /* !__RTP_TIMERS_DEF_H__ */
