#include "rtp_member.h"
#include "rtp_timers.h"

void
rtp_member_init(rtp_member_t* m, uint32_t ssrc)
{
  m->ssrc       = ssrc;
  m->cname[0]   = '\0';
  m->cname_len  = 0;
  m->flags      = 0;

  ntp_ts_init(&m->last_sr);
  ntp_ts_init(&m->last_sr_local_time);
}
