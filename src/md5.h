#ifndef __MD5_DEF_H__
#define __MD5_DEF_H__

#include "common_inc.h"


#if 0
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned long int UINT4;
#else
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
typedef uint16_t UINT2;

/* UINT4 defines a four byte word */
typedef uint32_t UINT4;
#endif

typedef struct 
{
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

extern void MD5Init(MD5_CTX *);
extern void MD5Update(MD5_CTX *, unsigned char *, unsigned int);
extern void MD5Final(unsigned char [16], MD5_CTX *);

#endif /* !__MD5_DEF_H__ */