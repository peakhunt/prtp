#include "CUnit/Basic.h"
#include "CUnit/Console.h"
#include "CUnit/Automated.h"

#include <stdlib.h>
#include <stdio.h>

#include "test_common.h"

extern void test_member_table_add(CU_pSuite pSuite);
extern void test_source_conflict_add(CU_pSuite pSuite);
extern void test_rtcp_encder_add(CU_pSuite pSuite);
extern void test_basic_add(CU_pSuite pSuite);
extern void test_rtp_add(CU_pSuite pSuite);
extern void test_rtcp_add(CU_pSuite pSuite);
extern void test_bye_add(CU_pSuite pSuite);
extern void test_jitter_add(CU_pSuite pSuite);

int init_suite_success(void) { return 0; }
int init_suite_failure(void) { return -1; }
int clean_suite_success(void) { return 0; }
int clean_suite_failure(void) { return -1; }

int
main()
{
  CU_pSuite pSuite = NULL;

  /* initialize the CUnit test registry */
  if (CUE_SUCCESS != CU_initialize_registry())
    return CU_get_error();

  /* add a suite to the registry */
  pSuite = CU_add_suite("Suite_success", init_suite_success, clean_suite_success);

  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }

  test_common_init();

  test_member_table_add(pSuite);
  test_source_conflict_add(pSuite);
  test_rtcp_encder_add(pSuite);
  test_basic_add(pSuite);
  test_rtp_add(pSuite);
  test_rtcp_add(pSuite);
  test_bye_add(pSuite);
  test_jitter_add(pSuite);

  /* Run all tests using the basic interface */
  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  printf("\n");
  CU_basic_show_failures(CU_get_failure_list());
  printf("\n\n");

  /* Clean up registry and return */
  CU_cleanup_registry();
  return CU_get_error();


  return 0;
}
