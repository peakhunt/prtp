#ifndef __RTP_TASK_DEF_H__
#define __RTP_TASK_DEF_H__

#include "rtp_session.h"

extern void rtp_task_init(uint8_t blah);
extern rtp_session_t* rtp_task_get_session(void);
extern void rtp_task_rtp_control(uint8_t enable);
extern void rtp_task_rtcp_control(uint8_t enable);

#endif /* !__RTP_TASK_DEF_H__ */
