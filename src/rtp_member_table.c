#include "rtp_member_table.h"
#include "rtp_random.h"

////////////////////////////////////////////////////////////
//
// public interfaces
//
////////////////////////////////////////////////////////////
void
rtp_member_table_init(rtp_member_table_t* mt)
{
  INIT_LIST_HEAD(&mt->free_list);
  INIT_LIST_HEAD(&mt->used_list);
  mt->num_members = 0;

  for(uint32_t i = 0; i < RTP_CONFIG_MAX_MEMBERS_PER_SESSION; i++)
  {
    list_add_tail(&mt->member_array[i].le, &mt->free_list);
  }
}

void
rtp_member_table_deinit(rtp_member_table_t* mt)
{
  // nothing to do with this implementation
}

rtp_member_t*
rtp_member_table_lookup(rtp_member_table_t* mt, uint32_t ssrc)
{
  rtp_member_t*   m;

  list_for_each_entry(m, &mt->used_list, le)
  {
    if(m->ssrc == ssrc)
    {
      return m;
    }
  }
  return NULL;
}

rtp_member_t*
rtp_member_table_alloc_member(rtp_member_table_t* mt, uint32_t ssrc)
{
  rtp_member_t* m;

  if(list_empty(&mt->free_list))
  {
    return NULL;
  }

  m = list_first_entry(&mt->free_list, rtp_member_t, le);
  list_del_init(&m->le);

  rtp_member_init(m, ssrc);

  list_add_tail(&m->le, &mt->used_list);
  mt->num_members++;

  return m;
}

void
rtp_member_table_free(rtp_member_table_t* mt, rtp_member_t* m)
{
  list_del_init(&m->le);
  mt->num_members--;

  list_add_tail(&m->le, &mt->free_list);
}

rtp_member_t*
rtp_member_table_get_first(rtp_member_table_t* mt)
{
  rtp_member_t*   m;

  if(list_empty(&mt->used_list))
  {
    return NULL;
  }

  m = list_first_entry(&mt->used_list, rtp_member_t, le);
  return m;
}

rtp_member_t*
rtp_member_table_get_next(rtp_member_table_t* mt, rtp_member_t* m)
{
  if(m == NULL)
  {
    return rtp_member_table_get_first(mt);
  }

  if(list_is_last(&m->le, &mt->used_list))
  {
    return NULL;
  }

  return list_entry(m->le.next, rtp_member_t, le);
}

void
rtp_member_table_change_random_ssrc(rtp_member_table_t* mt, rtp_member_t* m)
{
  //
  // allocated SSRC must be unique across the entire table
  //
  uint32_t ssrc;

  while(1)
  {
    ssrc = rtp_random32(RTP_CONFIG_RANDOM_TYPE);

    if(rtp_member_table_lookup(mt, ssrc) == NULL)
    {
      break;
    }
  }
  m->ssrc = ssrc;
}

void
rtp_member_table_change_ssrc(rtp_member_table_t* mt, rtp_member_t* m, uint32_t ssrc)
{
  // XXX
  // this routine is for unit testing only.
  // it is not supposed to use this in normal code
  //
  m->ssrc = ssrc;
}
