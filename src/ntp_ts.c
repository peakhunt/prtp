#include "ntp_ts.h"

void
ntp_ts_to_unix_time(ntp_ts_t* nt, struct timeval* ut)
{
  ut->tv_sec  = nt->second - 0x83AA7E80; // the seconds from Jan 1, 1900 to Jan 1, 1970
  ut->tv_usec = (uint32_t)( (double)nt->fraction * 1.0e6 / (double)(1LL<<32) );
}

void
ntp_ts_from_unix_time(ntp_ts_t* nt, struct timeval* ut)
{
  nt->second    = ut->tv_sec + 0x83AA7E80;
  nt->fraction  = (uint32_t)( (double)(ut->tv_usec+1) * (double)(1LL<<32) * 1.0e-6 );
}

void
ntp_ts_now(ntp_ts_t* nt)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  ntp_ts_from_unix_time(nt, &tv);
}

double
ntp_ts_diff_in_sec(ntp_ts_t* a, ntp_ts_t* b)
{
  struct timeval ua, ub;
  double  la, lb;
  double  diff;

  ntp_ts_to_unix_time(a, &ua);
  ntp_ts_to_unix_time(b, &ub);

  la = ua.tv_sec * 1000000 + ua.tv_usec;
  lb = ub.tv_sec * 1000000 + ub.tv_usec;

  diff = lb - la;

  return diff / 1000000;
}
