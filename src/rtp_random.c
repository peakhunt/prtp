#include <sys/types.h>   /* u_long */
#include <sys/time.h>    /* gettimeofday() */
#include <unistd.h>      /* get..() */
#include <stdio.h>       /* printf() */
#include <time.h>        /* clock() */
#include <sys/utsname.h> /* uname() */

#include "rtp_random.h"
#include "md5.h"         /* from RFC 1321 */

#define MD_CTX MD5_CTX
#define MDInit MD5Init
#define MDUpdate MD5Update
#define MDFinal MD5Final

static uint32_t
md_32(char *string, int length)
{
  MD_CTX context;
  union
  {
    char   c[16];
    u_long x[4];
  } digest;
  uint32_t r;
  int i;

  MDInit(&context);

  MDUpdate(&context, (unsigned char*)string, length);
  MDFinal((unsigned char *)&digest, &context);
  r = 0;
  for (i = 0; i < 3; i++) {
    r ^= digest.x[i];
  }
  return r;
}

/*
 * Return random unsigned 32-bit quantity.  Use 'type' argument if
 * you need to generate several different values in close succession.
 */
uint32_t
rtp_random32(int type)
{
  struct {
    int     type;
    struct  timeval tv;
    clock_t cpu;
    pid_t   pid;
    u_long  hid;
    uid_t   uid;
    gid_t   gid;
    struct  utsname name;
  } s;

  gettimeofday(&s.tv, 0);
  uname(&s.name);
  s.type = type;
  s.cpu  = clock();
  s.pid  = getpid();
  s.hid  = gethostid();
  s.uid  = getuid();
  s.gid  = getgid();
  /* also: system uptime */

  return md_32((char *)&s, sizeof(s));
}
