#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include "generic_list.h"
#include "cli.h"
#include "io_driver.h"

#include "common_inc.h"
#include "rtp_task.h"
#include "rtp_cli.h"

static io_driver_t          _io_driver;

io_driver_t*
main_io_driver(void)
{
  return &_io_driver;
}

int
main(int argc, char** argv)
{
  uint8_t blah;

  if(argc != 2)
  {
    printf("%s [0 or 1]\n", argv[0]);
    return -1;
  }

  blah = atoi(argv[1]);


  RTPLOGI("main", "starting up demo rtp task\n");

  io_driver_init(&_io_driver);

  RTPLOGI("main", "io driver initialized\n");

  rtp_cli_init();

  rtp_task_init(blah);
  RTPLOGI("main", "rtp task initialized\n");

  RTPLOGI("main", "entering main loop\n");

  while(1)
  {
    io_driver_run(&_io_driver);
  }

  return 0;
}
