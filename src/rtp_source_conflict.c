#include "rtp_source_conflict.h"

static const char* TAG = "conflict";

static void
rtp_source_conflict_timer_expired(SoftTimerElem* st)
{
  rtp_source_conflict_t*        c;
  rtp_source_conflict_table_t*  tbl = (rtp_source_conflict_table_t*)st->priv;

  c = container_of(st, rtp_source_conflict_t, te);

  list_del_init(&c->le);
  list_add_tail(&c->le, &tbl->free);
}

void
rtp_source_conflict_table_init(rtp_source_conflict_table_t* tbl, SoftTimer* tmr)
{
  tbl->tmr = tmr;

  INIT_LIST_HEAD(&tbl->free);
  INIT_LIST_HEAD(&tbl->used);

  for(uint32_t i = 0; i < RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE; i++)
  {
    INIT_LIST_HEAD(&tbl->entries[i].le);

    soft_timer_init_elem(&tbl->entries[i].te);

    tbl->entries[i].te.cb   = rtp_source_conflict_timer_expired;
    tbl->entries[i].te.priv = tbl;

    list_add_tail(&tbl->entries[i].le, &tbl->free);
  }
}

void
rtp_source_conflict_table_deinit(rtp_source_conflict_table_t* tbl)
{
  rtp_source_conflict_t* c;

  list_for_each_entry(c, &tbl->used, le)
  {
    soft_timer_del(tbl->tmr, &c->te);
  }
}

uint8_t
rtp_source_conflict_lookup(rtp_source_conflict_table_t* tbl, struct sockaddr_in* addr)
{
  rtp_source_conflict_t* c;

  list_for_each_entry(c, &tbl->used, le)
  {
    if(memcmp(&c->addr, addr, sizeof(struct sockaddr_in)) == 0)
    {
      soft_timer_del(tbl->tmr, &c->te);
      soft_timer_add(tbl->tmr, &c->te, RTP_CONFIG_SOURCE_CONFLICT_TIMEOUT);
      return RTP_TRUE;
    }
  }
  return RTP_FALSE;
}

uint8_t
rtp_source_conflict_add(rtp_source_conflict_table_t* tbl, struct sockaddr_in* addr)
{
  rtp_source_conflict_t*    c;

  if(list_empty(&tbl->free))
  {
    RTPLOGE(TAG, "no free entry in conflict table\n");
    return RTP_FALSE;
  }

  c = list_first_entry(&tbl->free, rtp_source_conflict_t, le);
  list_del_init(&c->le);

  memcpy(&c->addr, addr, sizeof(struct sockaddr_in));
  list_add_tail(&c->le, &tbl->used);

  soft_timer_add(tbl->tmr, &c->te, RTP_CONFIG_SOURCE_CONFLICT_TIMEOUT);

  return RTP_TRUE;
}
