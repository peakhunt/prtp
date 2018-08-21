#include "CUnit/Basic.h"
#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#include <stdlib.h>
#include <stdio.h>

#include "rtp_session.h"
#include "rtp_session_util.h"

#include "test_common.h"

static void
test_member_table(void)
{
  rtp_member_table_t    tbl;
  rtp_member_t*         members[RTP_CONFIG_MAX_MEMBERS_PER_SESSION];
  rtp_member_t*         m;
  int                   ndx;

  rtp_member_table_init(&tbl);

  CU_ASSERT(tbl.num_members == 0);

  CU_ASSERT(rtp_member_table_lookup(&tbl, 0) == NULL);
  CU_ASSERT(rtp_member_table_lookup(&tbl, 1) == NULL);
  CU_ASSERT(rtp_member_table_lookup(&tbl, 2) == NULL);
  CU_ASSERT(rtp_member_table_lookup(&tbl, 3) == NULL);

  for(int i = 0; i < RTP_CONFIG_MAX_MEMBERS_PER_SESSION; i++)
  {
    members[i] = rtp_member_table_alloc_member(&tbl, i);
    CU_ASSERT(members[i] != NULL);
  }

  // no more free member left
  CU_ASSERT(rtp_member_table_alloc_member(&tbl, 0) == NULL);

  CU_ASSERT(tbl.num_members == RTP_CONFIG_MAX_MEMBERS_PER_SESSION);

  ndx = 1;
  m = rtp_member_table_get_first(&tbl);
  CU_ASSERT(m == members[0]);

  while((m = rtp_member_table_get_next(&tbl, m)) != NULL)
  {
    CU_ASSERT(m == members[ndx]);
    ndx++;
  }
  CU_ASSERT(ndx == RTP_CONFIG_MAX_MEMBERS_PER_SESSION);

  for(int i = 0; i < RTP_CONFIG_MAX_MEMBERS_PER_SESSION; i++)
  {
    CU_ASSERT(members[i] == rtp_member_table_lookup(&tbl, i));
  }

  CU_ASSERT(rtp_member_table_lookup(&tbl, RTP_CONFIG_MAX_MEMBERS_PER_SESSION) == NULL);

  for(int i = 0; i < RTP_CONFIG_MAX_MEMBERS_PER_SESSION; i++)
  {
    rtp_member_table_free(&tbl, members[i]);
    CU_ASSERT(tbl.num_members == (RTP_CONFIG_MAX_MEMBERS_PER_SESSION - i - 1));
    CU_ASSERT(rtp_member_table_lookup(&tbl, i) == NULL);
  }

  rtp_member_table_deinit(&tbl);
}

void
test_member_table_add(CU_pSuite pSuite)
{
  CU_add_test(pSuite, "member_table", test_member_table);
}
