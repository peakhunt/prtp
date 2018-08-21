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

#define CONFIG_TELNET_PORT        9999

extern io_driver_t* main_io_driver(void);

static void cli_command_stats(cli_intf_t* intf, int argc, const char** argv);
static void cli_command_self(cli_intf_t* intf, int argc, const char** argv);
static void cli_command_members(cli_intf_t* intf, int argc, const char** argv);
static void cli_command_conflicts(cli_intf_t* intf, int argc, const char** argv);
static void cli_command_rtp(cli_intf_t* intf, int argc, const char** argv);
static void cli_command_rtcp(cli_intf_t* intf, int argc, const char** argv);
static void cli_command_ssrc(cli_intf_t* intf, int argc, const char** argv);

io_driver_t*
cli_io_driver(void)
{
  return main_io_driver();
}

static cli_command_t      _rtp_commands[] =
{
  {
    "stats",
    "show rtp session stats",
    cli_command_stats,
  },
  {
    "self",
    "show rtp self",
    cli_command_self,
  },
  {
    "members",
    "show rtp members",
    cli_command_members,
  },
  {
    "conflicts",
    "show source conflict table",
    cli_command_conflicts,
  },
  {
    "rtp",
    "enable/disable RTP TX",
    cli_command_rtp,
  },
  {
    "rtcp",
    "enable/disable RTCP TX",
    cli_command_rtcp,
  },
  {
    "ssrc",
    "change own ssrc",
    cli_command_ssrc,
  }
};

static void
print_member(cli_intf_t* intf, rtp_member_t* m)
{
  cli_printf(intf, "rtp addr: %s:%d"CLI_EOL, inet_ntoa(m->rtp_addr.sin_addr), ntohs(m->rtp_addr.sin_port));
  cli_printf(intf, "rtcp addr: %s:%d"CLI_EOL, inet_ntoa(m->rtcp_addr.sin_addr), ntohs(m->rtcp_addr.sin_port));
  cli_printf(intf, "ssrc: %u"CLI_EOL, m->ssrc);
  cli_printf(intf, "cname len %d, ", m->cname_len);
  for(int i = 0; i < m->cname_len; i++)
  {
    cli_printf(intf, "0x%02x ", m->cname[i]);
  }
  cli_printf(intf, CLI_EOL);
  cli_printf(intf, "flags : %08x"CLI_EOL, m->flags);

  cli_printf(intf, "rtp_src.max_seq : %u"CLI_EOL, m->rtp_src.max_seq);

  cli_printf(intf, "rtp_src.cycles : %u"CLI_EOL, m->rtp_src.cycles);
  cli_printf(intf, "rtp_src.base_seq : %u"CLI_EOL, m->rtp_src.base_seq);
  cli_printf(intf, "rtp_src.probation : %u"CLI_EOL, m->rtp_src.probation);
  cli_printf(intf, "rtp_src.receiver : %u"CLI_EOL, m->rtp_src.received);
  cli_printf(intf, "rtp_src.expected_prior : %u"CLI_EOL, m->rtp_src.expected_prior);
  cli_printf(intf, "rtp_src.received_prior : %u"CLI_EOL, m->rtp_src.received_prior);
  cli_printf(intf, "rtp_src.transit : %u"CLI_EOL, m->rtp_src.transit);
  cli_printf(intf, "rtp_src.jitter : %u"CLI_EOL, m->rtp_src.jitter);

  cli_printf(intf, "last_sr.second : %u"CLI_EOL, m->last_sr.second);
  cli_printf(intf, "last_sr.fraction : %u"CLI_EOL, m->last_sr.fraction);

  cli_printf(intf, "last_sr_local_time.second : %u"CLI_EOL, m->last_sr_local_time.second);
  cli_printf(intf, "last_sr_local_time.fraction : %u"CLI_EOL, m->last_sr_local_time.fraction);

  cli_printf(intf, "rtp_ts: %u"CLI_EOL, m->rtp_ts);
  cli_printf(intf, "pkt_count: %u"CLI_EOL, m->pkt_count);
  cli_printf(intf, "octet_count: %u"CLI_EOL, m->octet_count);
}

static void
cli_command_stats(cli_intf_t* intf, int argc, const char** argv)
{
  rtp_session_t* sess = rtp_task_get_session();

  cli_printf(intf, "session_bandwidth %d"CLI_EOL, sess->config.session_bw);
  cli_printf(intf, "seq %d"CLI_EOL, sess->seq);

  cli_printf(intf, "rtcp_var.tp   %.2f"CLI_EOL, sess->rtcp_var.tp);
  cli_printf(intf, "rtcp_var.tn   %.2f"CLI_EOL, sess->rtcp_var.tn);
  cli_printf(intf, "rtcp_var.pmembers   %u"CLI_EOL, sess->rtcp_var.pmembers);
  cli_printf(intf, "rtcp_var.members   %u"CLI_EOL, sess->rtcp_var.members);
  cli_printf(intf, "rtcp_var.sesnders   %u"CLI_EOL, sess->rtcp_var.senders);
  cli_printf(intf, "rtcp_var.rtcp_bw   %f"CLI_EOL, sess->rtcp_var.rtcp_bw);
  cli_printf(intf, "rtcp_var.avg_rtcp_size   %f"CLI_EOL, sess->rtcp_var.avg_rtcp_size);
  cli_printf(intf, "rtcp_var.we_sent   %d"CLI_EOL, sess->rtcp_var.we_sent);
  cli_printf(intf, "rtcp_var.initial   %d"CLI_EOL, sess->rtcp_var.initial);

  cli_printf(intf, "pt %d"CLI_EOL, sess->config.pt);
  cli_printf(intf, "invalid_rtcp_pkt %d"CLI_EOL, sess->invalid_rtcp_pkt);
  cli_printf(intf, "invalid_rtp_pkt %d"CLI_EOL, sess->invalid_rtp_pkt);
  cli_printf(intf, "last_rtp_error %d"CLI_EOL, sess->last_rtp_error);
  cli_printf(intf, "last_rtcp_error %d"CLI_EOL, sess->last_rtcp_error);
  cli_printf(intf, "tx_pkt_count %d"CLI_EOL, sess->tx_pkt_count);
  cli_printf(intf, "tx_octet_count %d"CLI_EOL, sess->tx_octet_count);
}

static void
cli_command_self(cli_intf_t* intf, int argc, const char** argv)
{
  rtp_session_t* sess = rtp_task_get_session();

  print_member(intf, sess->self);
}

static void
cli_command_members(cli_intf_t* intf, int argc, const char** argv)
{
  rtp_session_t* sess = rtp_task_get_session();
  rtp_member_t* m;

  cli_printf(intf, "num_members: %u"CLI_EOL, sess->member_table.num_members);
  m = rtp_member_table_get_first(&sess->member_table);

  while(m != NULL)
  {
    if(rtp_member_is_self(m) == RTP_FALSE)
    {
      cli_printf(intf, "======================================"CLI_EOL);
      print_member(intf, m);
    }

    m = rtp_member_table_get_next(&sess->member_table, m);
  }
}

static void
cli_command_conflicts(cli_intf_t* intf, int argc, const char** argv)
{
  rtp_session_t* sess = rtp_task_get_session();
  rtp_source_conflict_t*    c;
  int i = 0;

  list_for_each_entry(c, &sess->src_conflict.used, le)
  {
    cli_printf(intf, "======================================"CLI_EOL);
    cli_printf(intf, "%d: %s:%d"CLI_EOL, i, inet_ntoa(c->addr.sin_addr), ntohs(c->addr.sin_port));
    i++;
  }
}

static void
cli_command_rtp(cli_intf_t* intf, int argc, const char** argv)
{
  if(argc != 2)
  {
    goto error;
  }

  if(strcmp(argv[1], "on") == 0)
  {
    rtp_task_rtp_control(RTP_TRUE);
  }
  else if(strcmp(argv[1], "off") == 0)
  {
    rtp_task_rtp_control(RTP_FALSE);
  }
  else
  {
    goto error;
  }

  return;

error:
  cli_printf(intf, "Error"CLI_EOL);
  cli_printf(intf, "%s [on|off]"CLI_EOL, argv[0]);

}

static void
cli_command_rtcp(cli_intf_t* intf, int argc, const char** argv)
{
  if(argc != 2)
  {
    goto error;
  }

  if(strcmp(argv[1], "on") == 0)
  {
    rtp_task_rtcp_control(RTP_TRUE);
  }
  else if(strcmp(argv[1], "off") == 0)
  {
    rtp_task_rtcp_control(RTP_FALSE);
  }
  else
  {
    goto error;
  }

  return;

error:
  cli_printf(intf, "Error"CLI_EOL);
  cli_printf(intf, "%s [on|off]"CLI_EOL, argv[0]);

}

static void
cli_command_ssrc(cli_intf_t* intf, int argc, const char** argv)
{
  rtp_session_t* sess = rtp_task_get_session();
  uint32_t    ssrc;

  if(argc != 2)
  {
    goto error;
  }

  ssrc = atoi(argv[1]);

  rtp_member_table_change_ssrc(&sess->member_table, sess->self, ssrc);
  cli_printf(intf, "changed own ssrc to %u"CLI_EOL, ssrc);

  return;

error:
  cli_printf(intf, "Error"CLI_EOL);
  cli_printf(intf, "%s ssrc"CLI_EOL, argv[0]);
}

void
rtp_cli_init(void)
{
  cli_init(_rtp_commands, NARRAY(_rtp_commands), CONFIG_TELNET_PORT);

  RTPLOGI("main", "cli initialized\n");
}
