#ifndef __RTP_MEMBER_DEF_H__
#define __RTP_MEMBER_DEF_H__

#include "common_inc.h"
#include "soft_timer.h"
#include "generic_list.h"
#include "rfc3550.h"
#include "ntp_ts.h"

#define RTP_FLAG_SELF                   0x01
#define RTP_FLAG_RTP_HEARD              0x02
#define RTP_FLAG_RTCP_HEARD             0x04
#define RTP_FLAG_BYE_RECEIVED           0x08
#define RTP_FLAG_VALIDATED              0x10
#define RTP_FLAG_SENDER                 0x20
#define RTP_FLAG_CSRC                   0x40

typedef struct
{
  struct list_head    le;

  SoftTimerElem       member_te;
  SoftTimerElem       sender_te;
  SoftTimerElem       leave_te;

  struct sockaddr_in  rtp_addr;
  struct sockaddr_in  rtcp_addr;
  uint32_t            ssrc;
  uint8_t             cname[RTP_CONFIG_SDES_CNAME_MAX + 1];
  uint8_t             cname_len;
  uint32_t            flags;
  rtp_source_t        rtp_src;

  // from SR report
  ntp_ts_t            last_sr;
  ntp_ts_t            last_sr_local_time;
  uint32_t            rtp_ts;
  uint32_t            pkt_count;
  uint32_t            octet_count;
} rtp_member_t;

extern void rtp_member_init(rtp_member_t* m, uint32_t ssrc);

static inline uint8_t
rtp_member_is_self(rtp_member_t* m)
{
  if((m->flags & RTP_FLAG_SELF) != 0)
  {
    return RTP_TRUE;
  }
  return RTP_FALSE;
}

static inline void
rtp_member_set_self(rtp_member_t* m)
{
  m->flags |= RTP_FLAG_SELF;
}

static inline uint8_t
rtp_member_is_validated(rtp_member_t* m)
{
  if((m->flags & RTP_FLAG_VALIDATED) != 0)
  {
    return RTP_TRUE;
  }
  return RTP_FALSE;
}

static inline void
rtp_member_set_validated(rtp_member_t* m)
{
  m->flags |= RTP_FLAG_VALIDATED;
}

static inline void
rtp_member_clear_validated(rtp_member_t* m)
{
  m->flags &= ~RTP_FLAG_VALIDATED;
}

static inline uint8_t
rtp_member_is_sender(rtp_member_t* m)
{
  if((m->flags & RTP_FLAG_SENDER) != 0)
  {
    return RTP_TRUE;
  }
  return RTP_FALSE;
}

static inline void
rtp_member_set_sender(rtp_member_t* m)
{
  m->flags |= RTP_FLAG_SENDER;
}

static inline void
rtp_member_clear_sender(rtp_member_t* m)
{
  m->flags &= ~RTP_FLAG_SENDER;
}

static inline uint8_t
rtp_member_is_rtp_heard(rtp_member_t* m)
{
  if((m->flags & RTP_FLAG_RTP_HEARD) != 0)
  {
    return RTP_TRUE;
  }
  return RTP_FALSE;
}

static inline void
rtp_member_set_rtp_heard(rtp_member_t* m)
{
  m->flags |= RTP_FLAG_RTP_HEARD;
}

static inline void
rtp_member_clear_rtp_heard(rtp_member_t* m)
{
  m->flags &= ~RTP_FLAG_RTP_HEARD;
}

static inline uint8_t
rtp_member_is_rtcp_heard(rtp_member_t* m)
{
  if((m->flags & RTP_FLAG_RTCP_HEARD) != 0)
  {
    return RTP_TRUE;
  }
  return RTP_FALSE;
}

static inline void
rtp_member_set_rtcp_heard(rtp_member_t* m)
{
  m->flags |= RTP_FLAG_RTCP_HEARD;
}

static inline void
rtp_member_clear_rtcp_heard(rtp_member_t* m)
{
  m->flags &= ~RTP_FLAG_RTCP_HEARD;
}

static inline uint8_t
rtp_member_is_bye_received(rtp_member_t* m)
{
  if((m->flags & RTP_FLAG_BYE_RECEIVED) != 0)
  {
    return RTP_TRUE;
  }
  return RTP_FALSE;
}

static inline void
rtp_member_set_bye_received(rtp_member_t* m)
{
  m->flags |= RTP_FLAG_BYE_RECEIVED;
}

static inline void
rtp_member_clear_bye_received(rtp_member_t* m)
{
  m->flags &= ~RTP_FLAG_BYE_RECEIVED;
}

static inline uint8_t
rtp_member_is_csrc(rtp_member_t* m)
{
  if((m->flags & RTP_FLAG_CSRC) != 0)
  {
    return RTP_TRUE;
  }
  return RTP_FALSE;
}

static inline void
rtp_member_set_csrc(rtp_member_t* m)
{
  m->flags |= RTP_FLAG_CSRC;
}

static inline void
rtp_member_clear_csrc(rtp_member_t* m)
{
  m->flags &= ~RTP_FLAG_CSRC;
}

static inline void
rtp_member_set_cname(rtp_member_t* m, const uint8_t* cname, uint8_t len)
{
  m->cname_len = len <= RTP_CONFIG_SDES_CNAME_MAX ? len : RTP_CONFIG_SDES_CNAME_MAX;

  memcpy(m->cname, cname, m->cname_len);
}

#endif /* !__RTP_MEMBER_DEF_H__ */
