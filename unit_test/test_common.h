#ifndef __TEST_COMMON_DEF_H__
#define __TEST_COMMON_DEF_H__

#include "rtp_session.h"
#include "rtp_session_util.h"

#define SESSION_NAME          "test_session"
#define SESSION_PT            5
#define TEST_OWN_SSRC         9999

extern rtp_session_t* common_session_init(void);
extern void test_common_init(void);

extern struct sockaddr_in     _rtp_addr,
                              _rtcp_addr;

extern struct sockaddr_in     _rtp_rem_addr,
                              _rtcp_rem_addr;

extern struct sockaddr_in     _rtp_rem_addr2,
                              _rtcp_rem_addr2;

extern void test_rtp_timestamp_set(rtp_session_t* sess, uint32_t ts);

#endif /* !__TEST_COMMON_DEF_H__ */
