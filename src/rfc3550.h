#ifndef __RFC3550_DEF_H__
#define __RFC3550_DEF_H__

#include "common_inc.h"

#define RTP_VERSION           2
#define RTP_SEQ_MOD           (1<<16)

/*
 * RTP data header
 */
typedef struct {
#if RTP_CONFIG_ENDIAN == RTP_CONFIG_BIGENDIAN
  uint32_t version:2;       /* protocol version */
  uint32_t p:1;             /* padding flag */
  uint32_t x:1;             /* header extension flag */
  uint32_t cc:4;            /* CSRC count */
  uint32_t m:1;             /* marker bit */
  uint32_t pt:7;            /* payload type */
  uint32_t seq:16;          /* sequence number */
#elif RTP_CONFIG_ENDIAN == RTP_CONFIG_LITTLEENDIAN
  //
  // XXX
  // on little endian machines with GCC,
  // bits are allocated LSB first and
  // low byte goes to low address
  //
  uint32_t cc:4;            /* CSRC count */
  uint32_t x:1;             /* header extension flag */
  uint32_t p:1;             /* padding flag */
  uint32_t version:2;       /* protocol version */
  uint32_t pt:7;            /* payload type */
  uint32_t m:1;             /* marker bit */
  uint32_t seq:16;          /* sequence number */
#endif

  uint32_t ts;              /* timestamp */
  uint32_t ssrc;            /* synchronization source */
  uint32_t csrc[1];         /* optional CSRC list */
} rtp_hdr_t;

typedef struct {
  uint16_t            max_seq;          /* highest seq. number seen                 */
  uint32_t            cycles;           /* shifted count of seq. number cycles      */
  uint32_t            base_seq;         /* base seq. number                         */
  uint32_t            bad_seq;          /* last bad seq number + 1                  */
  uint32_t            probation;        /* seq. packets till source is valid        */
  uint32_t            received;         /* packets received                         */
  uint32_t            expected_prior;   /* packet expected at last interval         */
  uint32_t            received_prior;   /* packet received at last interval         */
  uint32_t            transit;          /* relative trans time for prev packet.     */
  uint32_t            jitter;           /* estimated jitter                         */
} rtp_source_t;

#define RTP_MAX_SDES          255           /* max text length of SDES */

typedef enum {
  RTCP_SR   = 200,
  RTCP_RR   = 201,
  RTCP_SDES = 202,
  RTCP_BYE  = 203,
  RTCP_APP  = 204
} rtcp_type_t;

typedef enum {
  RTCP_SDES_END   = 0,
  RTCP_SDES_CNAME = 1,
  RTCP_SDES_NAME  = 2,
  RTCP_SDES_EMAIL = 3,
  RTCP_SDES_PHONE = 4,
  RTCP_SDES_LOC   = 5,
  RTCP_SDES_TOOL  = 6,
  RTCP_SDES_NOTE  = 7,
  RTCP_SDES_PRIV  = 8
} rtcp_sdes_type_t;

/*
 * RTCP common header word
 */
typedef struct {
#if RTP_CONFIG_ENDIAN == RTP_CONFIG_BIGENDIAN
  uint32_t version:2;       /* protocol version */
  uint32_t p:1;             /* padding flag */
  uint32_t count:5;         /* varies by packet type */
  uint32_t pt:8;            /* RTCP packet type */
  uint32_t length:16;       /* pkt len in words, w/o this word */
#elif RTP_CONFIG_ENDIAN == RTP_CONFIG_LITTLEENDIAN
  uint32_t count:5;         /* varies by packet type */
  uint32_t p:1;             /* padding flag */
  uint32_t version:2;       /* protocol version */
  uint32_t pt:8;            /* RTCP packet type */
  uint32_t length:16;       /* pkt len in words, w/o this word */
#endif
} rtcp_common_t;

/*
 * Big-endian mask for version, padding bit and packet type pair
 */
#if RTP_CONFIG_ENDIAN == RTP_CONFIG_BIGENDIAN

# define RTCP_VALID_MASK (0xc000 | 0x2000 | 0xfe)
# define RTCP_VALID_VALUE ((RTP_VERSION << 14) | RTCP_SR)

#elif RTP_CONFIG_ENDIAN == RTP_CONFIG_LITTLEENDIAN

# define RTCP_VALID_MASK ( 0xfe00 | 0x20 | 0xc0 )    /* pad | ver | pt */
# define RTCP_VALID_VALUE ( ( RTCP_SR << 8 ) | (RTP_VERSION << 6) )

#endif

/*
 * Reception report block
 */
typedef struct {
  uint32_t ssrc;          /* data source being reported */
#if RTP_CONFIG_ENDIAN == RTP_CONFIG_BIGENDIAN
  uint32_t fraction:8;    /* fraction lost since last SR/RR */
  int32_t  lost:24;       /* cumul. no. pkts lost (signed!) */
#elif RTP_CONFIG_ENDIAN == RTP_CONFIG_LITTLEENDIAN
  uint32_t fraction:8;    /* fraction lost since last SR/RR */
  int32_t  lost:24;       /* cumul. no. pkts lost (signed!) */
#endif
  uint32_t last_seq;      /* extended last seq. no. received */
  uint32_t jitter;        /* interarrival jitter */
  uint32_t lsr;           /* last SR packet from this source */
  uint32_t dlsr;          /* delay since last SR packet */
} rtcp_rr_t;

/*
 * SDES item
 */
typedef struct {
  uint8_t type;           /* type of item (rtcp_sdes_type_t) */
  uint8_t length;         /* length of item (in octets) */
  char data[1];           /* text, not null-terminated */
} rtcp_sdes_item_t;

/*
 * One RTCP packet
 */
typedef struct {
  rtcp_common_t common;     /* common header */
  union {
    /* sender report (SR) */
    struct {
      uint32_t ssrc;          /* sender generating this report */
      uint32_t ntp_sec;       /* NTP timestamp */
      uint32_t ntp_frac;
      uint32_t rtp_ts;        /* RTP timestamp */
      uint32_t psent;         /* packets sent */
      uint32_t osent;         /* octets sent */
      rtcp_rr_t rr[1];        /* variable-length list */
    } sr;

    /* reception report (RR) */
    struct {
      uint32_t ssrc;          /* receiver generating this report */
      rtcp_rr_t rr[1];        /* variable-length list */
    } rr;

    /* source description (SDES) */
    struct rtcp_sdes {
      uint32_t src;               /* first SSRC/CSRC */
      rtcp_sdes_item_t item[1];   /* list of SDES items */
    } sdes;

    /* BYE */
    struct {
      uint32_t src[1];            /* list of sources */
      /* can't express trailing text for reason */
    } bye;
  } r;
} rtcp_t;

typedef struct
{
  double      tp;           // the last time an RTCP packet was transmitted
  double      tn;           // the next scheduled transmission time of an RTCP packet
  uint32_t    pmembers;     // the estimated number of session members at the time tn was last recomputed
  uint32_t    members;      // the most current estimate for the number of session members
  uint32_t    senders;      // the most current estimate for the number of senders in the session
  double      rtcp_bw;      // the target RTCP bandwidth
  double      avg_rtcp_size;
  uint8_t     we_sent;
  uint8_t     initial;
} rtcp_control_var_t;

typedef struct rtcp_sdes rtcp_sdes_t;

#define RTP_HDR_SIZE(csrc)                  (sizeof(rtp_hdr_t) - 4 + (csrc * sizeof(uint32_t)))
#define RTP_PKT_SIZE(csrc, payload_len)     (RTP_HDR_SIZE(csrc) + payload_len)

#endif /* !__RFC3550_DEF_H__ */
