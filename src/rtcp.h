#ifndef __RTCP_DEF_H__
#define __RTCP_DEF_H__

#include "rtp_session.h"


extern void rtcp_init(rtp_session_t* sess);
extern void rtcp_deinit(rtp_session_t* sess);
extern void rtcp_rx(rtp_session_t* sess, uint8_t* pkt, uint32_t len, struct sockaddr_in* from);
extern void rtcp_tx_bye(rtp_session_t* sess);

extern void rtcp_interval_handle_rtp_event(rtp_session_t* sess, uint8_t new_member, uint8_t new_sender);
extern void rtcp_interval_tx_bye(rtp_session_t* sess);

extern void rtcp_interval_member_timedout(rtp_session_t* sess, rtp_member_t* m);
extern void rtcp_interval_sender_timedout(rtp_session_t* sess, rtp_member_t* m);

extern void rtcp_compute_jitter(rtp_session_t* sess, rtp_member_t* m, uint32_t rtp_ts, uint32_t arrival);

#endif /* !__RTCP_DEF_H__ */
