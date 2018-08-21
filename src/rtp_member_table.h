#ifndef __RTP_MEMBER_TABLE_DEF_H__
#define __RTP_MEMBER_TABLE_DEF_H__

#include "common_inc.h"
#include "soft_timer.h"
#include "generic_list.h"
#include "rtp_member.h"

typedef struct
{
  rtp_member_t      member_array[RTP_CONFIG_MAX_MEMBERS_PER_SESSION];
  struct list_head  free_list;
  struct list_head  used_list;
  uint32_t          num_members;
} rtp_member_table_t;

extern void rtp_member_table_init(rtp_member_table_t* mt);
extern void rtp_member_table_deinit(rtp_member_table_t* mt);
extern rtp_member_t* rtp_member_table_lookup(rtp_member_table_t* mt, uint32_t ssrc);
extern rtp_member_t* rtp_member_table_alloc_member(rtp_member_table_t* mt, uint32_t ssrc);
extern void rtp_member_table_free(rtp_member_table_t* mt, rtp_member_t* m);
extern void rtp_member_table_change_random_ssrc(rtp_member_table_t* mt, rtp_member_t* m);
extern void rtp_member_table_change_ssrc(rtp_member_table_t* mt, rtp_member_t* m, uint32_t ssrc);
extern rtp_member_t* rtp_member_table_get_first(rtp_member_table_t* mt);

//
// this shouldn't be called in any context that might change the list
//
extern rtp_member_t* rtp_member_table_get_next(rtp_member_table_t* mt, rtp_member_t* m);

#endif /* !__RTP_MEMBER_TABLE_DEF_H__ */
