#ifndef __RTP_SOURCE_CONFLICT_DEF_H__
#define __RTP_SOURCE_CONFLICT_DEF_H__

#include "common_inc.h"
#include "generic_list.h"
#include "soft_timer.h"

typedef struct
{
  struct list_head      le;
  SoftTimerElem         te;
  struct sockaddr_in    addr;
} rtp_source_conflict_t;

typedef struct
{
  rtp_source_conflict_t   entries[RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE];
  SoftTimer*              tmr;
  struct list_head        free;
  struct list_head        used;
} rtp_source_conflict_table_t;

extern void rtp_source_conflict_table_init(rtp_source_conflict_table_t* tbl, SoftTimer* tmr);
extern void rtp_source_conflict_table_deinit(rtp_source_conflict_table_t* tbl);
extern uint8_t rtp_source_conflict_lookup(rtp_source_conflict_table_t* tbl, struct sockaddr_in* addr);
extern uint8_t rtp_source_conflict_add(rtp_source_conflict_table_t* tbl, struct sockaddr_in* addr);

#endif /* !__RTP_SOURCE_CONFLICT_DEF_H__ */
