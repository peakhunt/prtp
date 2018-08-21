#ifndef __NTP_TS_DEF_H__
#define __NTP_TS_DEF_H__

#include "common_inc.h"

typedef struct
{
  uint32_t      second;
  uint32_t      fraction;
} ntp_ts_t;

extern void ntp_ts_to_unix_time(ntp_ts_t* nt, struct timeval* ut);
extern void ntp_ts_from_unix_time(ntp_ts_t* nt, struct timeval* ut);
extern void ntp_ts_now(ntp_ts_t* nt);
extern double ntp_ts_diff_in_sec(ntp_ts_t* a, ntp_ts_t* b);

static inline void
ntp_ts_init(ntp_ts_t* nt)
{
  nt->second      = 0;
  nt->fraction    = 0;
}

#endif /* !__NTP_TS_DEF_H__ */
