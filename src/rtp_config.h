#ifndef __RTP_CONFIG_DEF_H__
#define __RTP_CONFIG_DEF_H__

/*
 *
 * @desc 
 * this definitions decides the maximum number of members possible per session
 */
#define RTP_CONFIG_MAX_MEMBERS_PER_SESSION        32

#define RTP_CONFIG_MAX_RTP_PKT_SIZE               1024

/*
 *
 * @desc
 * average rtcp packet size presumed in bytes
 */
#define RTP_CONFIG_AVERAGE_RTCP_SIZE              256

/*
 *
 * @desc
 * RTP BYE timeout value. Upon receiving bye, the member is
 * actually removed after this timeout to handle misordered
 * packets. The unit is millisecond.
 */
#define RTP_CONFIG_BYE_RX_TIMEOUT                 5000

#define RTP_CONFIG_RANDOM_TYPE                    time(NULL)

#define RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE     16
#define RTP_CONFIG_SOURCE_CONFLICT_TIMEOUT        5000      // 5000 ms

#define RTP_CONFIG_SDES_CNAME_MAX                 256

#define RTP_CONFIG_SENDER_TIMEOUT                 5000
#define RTP_CONFIG_MEMBER_TIMEOUT                 10000
#define RTP_CONFIG_LEAVE_TIMEOUT                  3000

#define RTP_CONFIG_MAX_DROPOUT                    3000
#define RTP_CONFIG_MAX_MISORDER                   100
#define RTP_CONFIG_MIN_SEQUENTIAL                 2

#define RTP_CONFIG_RTCP_ENCODER_BUFFER_LEN        256

#define RTP_CONFIG_BIGENDIAN                      1
#define RTP_CONFIG_LITTLEENDIAN                   0

#define RTP_CONFIG_ENDIAN                        RTP_CONFIG_LITTLEENDIAN

#ifndef RTP_CONFIG_ENDIAN
#error "RTP_CONFRIG_ENDIAN not defined"
#endif

#if RTP_CONFIG_ENDIAN != RTP_CONFIG_BIGENDIAN && RTP_CONFIG_ENDIAN != RTP_CONFIG_LITTLEENDIAN
#error "Invalid RTP_CONFRIG_ENDIAN"
#endif

#if 1
#define RTPLOGI(tag, str, ...)       printf("%ld:%s:%d, %s:"str, time(NULL), __func__, __LINE__, tag, ##__VA_ARGS__); fflush(stdout)
#define RTPLOGE(tag, str, ...)       printf("%ld:%s:%d, %s:"str, time(NULL), __func__, __LINE__, tag, ##__VA_ARGS__); fflush(stdout)
#else
#define RTPLOGI(tag, str, ...)     (void)tag
#define RTPLOGE(tag, str, ...)     (void)tag
#endif

#endif /* !__RTP_CONFIG_DEF_H__ */
