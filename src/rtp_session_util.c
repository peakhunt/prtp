#include "rtp_session_util.h"
#include "rtp_timers.h"

rtp_member_t*
rtp_session_alloc_member(rtp_session_t* sess, uint32_t ssrc)
{
  rtp_member_t*   m;

  m = rtp_member_table_alloc_member(&sess->member_table, ssrc);
  if(m == NULL)
  {
    return NULL;
  }

  rtp_timers_init_member(sess, m);

  return m;
}

void
rtp_session_dealloc_member(rtp_session_t* sess, rtp_member_t* m)
{
  rtp_timers_deinit_member(sess, m);

  rtp_member_table_free(&sess->member_table, m);
}

rtp_member_t*
rtp_session_lookup_member(rtp_session_t* sess, uint32_t ssrc)
{
  return rtp_member_table_lookup(&sess->member_table, ssrc);
}

uint32_t
rtp_session_timestamp(rtp_session_t* sess)
{
  //
  // https://loadmultiplier.com/content/rtp-timestamp-calculation
  //
  // basically RTP timestamp is
  // RTP timestamp += (sampling rate / (frames per sec))
  //
  return sess->rtp_timestamp(sess);
}
