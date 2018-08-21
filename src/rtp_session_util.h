#ifndef __RTP_SESSION_UTIL_DEF_H__
#define __RTP_SESSION_UTIL_DEF_H__

#include "rtp_session.h"

extern rtp_member_t* rtp_session_alloc_member(rtp_session_t* sess, uint32_t ssrc);
extern void rtp_session_dealloc_member(rtp_session_t* sess, rtp_member_t* m);
extern rtp_member_t* rtp_session_lookup_member(rtp_session_t* sess, uint32_t ssrc);
extern uint32_t rtp_session_timestamp(rtp_session_t* sess);

#endif /* !__RTP_SESSION_UTIL_DEF_H__ */
