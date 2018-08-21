#ifndef __COMMON_INC_DEF_H__
#define __COMMON_INC_DEF_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdint.h>

#include "rtp_config.h"

#define RTP_TRUE      1
#define RTP_FALSE     0

#define RTPCRASH(msg)    \
{\
  char* ptr = 0;\
  RTPLOGE("CRASH", #msg);\
  *ptr = 0;\
}


#endif /* !__COMMON_INC_DEF_H__ */
