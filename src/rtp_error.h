#ifndef __RTP_ERROR_DEF_H__
#define __RTP_ERROR_DEF_H__

typedef enum
{
  rtp_rx_error_no_error = 0,
  rtp_rx_error_header_too_short,
  rtp_rx_error_invalid_csrc_count,
  rtp_rx_error_invalid_rtp_version,
  rtp_rx_error_invalid_payload_type,
  rtp_rx_error_invalid_octet_count,
  rtp_rx_error_not_implemented,
  rtp_rx_error_seq_error,
  rtp_rx_error_member_alloc_failed,
  rtp_rx_error_member_first_heard,
  rtp_rx_error_member_bye_in_progress,
  rtp_rx_error_member_first_rtp_after_rtcp,
  rtp_rx_error_3rd_party_conflict,
  rtp_rx_error_source_in_conflict_list,
  rtp_rx_error_ssrc_conflict,
} rtp_rx_error_t;

typedef enum
{
  rtcp_rx_error_no_error = 0,
  rtcp_rx_error_header_too_short,
  rtcp_rx_error_len_not_multiple_4,
  rtcp_rx_error_invalid_mask,
  rtcp_rx_error_invalid_compound_packet,
  rtcp_rx_error_member_alloc_failed,
  rtcp_rx_error_member_first_heard,
  rtcp_rx_error_member_bye_in_progress,
  rtcp_rx_error_3rd_party_conflict,
  rtcp_rx_error_source_in_conflict_list,
  rtcp_rx_error_ssrc_conflict,
} rtcp_rx_error_t;

#endif /* !__RTP_ERROR_DEF_H__ */
