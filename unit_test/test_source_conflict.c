#include "CUnit/Basic.h"
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#include <stdlib.h>
#include <stdio.h>

#include "rtp_session.h"
#include "rtp_session_util.h"

#include "test_common.h"

#define TEST_ADDRESS    "192.168.1.1"
#define TEST_PORT_START       17000

#define stringfy(a)        #a

static inline void
sock_addr_init(struct sockaddr_in* saddr, const char* addr, int port)
{
  memset(saddr, 0, sizeof(struct sockaddr_in));

  saddr->sin_family         = AF_INET;
  saddr->sin_addr.s_addr    = inet_addr(addr);
  saddr->sin_port           = htons(port);
}

static void
test_source_conflict(void)
{
  SoftTimer                     stmr;
  rtp_source_conflict_table_t   stbl;
  struct sockaddr_in            test_addrs[RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE];

  soft_timer_init(&stmr, 100);
  rtp_source_conflict_table_init(&stbl, &stmr);

  for(int i = 0; i < RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE; i++)
  {
    sock_addr_init(&test_addrs[i], TEST_ADDRESS, TEST_PORT_START + i);
  }

  for(int i = 0; i < RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE; i++)
  {
    CU_ASSERT(rtp_source_conflict_lookup(&stbl, &test_addrs[i]) == RTP_FALSE);
  }

  for(int i = 0; i < RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE; i++)
  {
    CU_ASSERT(rtp_source_conflict_add(&stbl, &test_addrs[i]) == RTP_TRUE);
    CU_ASSERT(rtp_source_conflict_lookup(&stbl, &test_addrs[i]) == RTP_TRUE);
  }

  // full
  CU_ASSERT(rtp_source_conflict_add(&stbl, &test_addrs[0]) == RTP_FALSE);

  // timeout test
  for(int i = 0; i < (RTP_CONFIG_SOURCE_CONFLICT_TIMEOUT/stmr.tick_rate); i++)
  {
    soft_timer_drive(&stmr);
    CU_ASSERT(rtp_source_conflict_lookup(&stbl, &test_addrs[0]) == RTP_TRUE);
  }

  for(int i = 1; i < RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE; i++)
  {
    CU_ASSERT(rtp_source_conflict_lookup(&stbl, &test_addrs[i]) == RTP_FALSE);
  }
  CU_ASSERT(rtp_source_conflict_lookup(&stbl, &test_addrs[0]) == RTP_TRUE);

  for(int i = 0; i < (RTP_CONFIG_SOURCE_CONFLICT_TIMEOUT/stmr.tick_rate); i++)
  {
    soft_timer_drive(&stmr);
  }
  CU_ASSERT(rtp_source_conflict_lookup(&stbl, &test_addrs[0]) == RTP_FALSE);

  for(int i = 0; i < RTP_CONFIG_SOURCE_CONFLICT_TABLE_SIZE; i++)
  {
    CU_ASSERT(rtp_source_conflict_add(&stbl, &test_addrs[i]) == RTP_TRUE);
    CU_ASSERT(rtp_source_conflict_lookup(&stbl, &test_addrs[i]) == RTP_TRUE);
  }

  // full
  CU_ASSERT(rtp_source_conflict_add(&stbl, &test_addrs[0]) == RTP_FALSE);

  rtp_source_conflict_table_deinit(&stbl);
}

void
test_source_conflict_add(CU_pSuite pSuite)
{
  CU_add_test(pSuite, "source_conflict", test_source_conflict);
}
