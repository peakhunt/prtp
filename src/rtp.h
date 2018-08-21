#ifndef __RTP_DEF_H__
#define __RTP_DEF_H__

#include "rtp_session.h"

extern void rtp_init(rtp_session_t* sess);
extern void rtp_deinit(rtp_session_t* sess);
extern void rtp_rx(rtp_session_t* sess, uint8_t* pkt, uint32_t len, struct sockaddr_in* from);
extern void rtp_tx(rtp_session_t* sess, uint8_t* payload, uint32_t payload_len, uint32_t rtp_ts, uint32_t* csrc, uint8_t ncsrc);

#endif /* !__RTP_DEF_H__ */
