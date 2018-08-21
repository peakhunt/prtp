include Rules.mk

#######################################
# list of source files
########################################
LIB_HRTP_SOURCES =                                  \
src/md5.c                                           \
src/soft_timer.c                                    \
src/rtp_random.c                                    \
src/rtp_member.c                                    \
src/rtp_member_table.c                              \
src/rtp_session.c                                   \
src/rtp_session_util.c                              \
src/rtp.c                                           \
src/rtcp.c                                          \
src/rtp_source_conflict.c                           \
src/rtp_timers.c                                    \
src/rtcp_encoder.c                                  \
src/ntp_ts.c

LIB_UTILS_SOURCES =                                 \
libutils/circ_buffer.c                              \
libutils/cli.c                                      \
libutils/cli_telnet.c                               \
libutils/io_driver.c                                \
libutils/stream.c                                   \
libutils/sock_util.c                                \
libutils/tcp_server.c                               \
libutils/tcp_server_ipv4.c                          \
libutils/telnet_reader.c

DEMO_SOURCES =                                      \
demo/main.c                                         \
demo/rtp_task.c                                     \
demo/rtp_cli.c

#######################################
C_DEFS  = 

#######################################
# include and lib setup
#######################################
C_INCLUDES =                              \
-Isrc                                     \
-Ilibutils                                \
-Itest

LIBS = -lpthread -lm
LIBDIR = 

#######################################
# for verbose output
#######################################
# Prettify output
V = 0
ifeq ($V, 0)
  Q = @
  P = > /dev/null
else
  Q =
  P =
endif

#######################################
# build directory and target setup
#######################################
BUILD_DIR = build
TARGET    = petra_rtp_test

#######################################
# compile & link flags
#######################################
CFLAGS += -g $(C_DEFS) $(C_INCLUDES)

# Generate dependency information
CFLAGS += -MMD -MF .dep/$(*F).d

LDFLAGS +=  $(LIBDIR) $(LIBS)

#######################################
# build target
#######################################
all: $(BUILD_DIR)/$(TARGET)

#######################################
# target source setup
#######################################
TARGET_SOURCES := $(LIB_HRTP_SOURCES)
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(TARGET_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(TARGET_SOURCES)))

LIB_UTILS_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(LIB_UTILS_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(LIB_UTILS_SOURCES)))

DEMO_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(DEMO_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(DEMO_SOURCES)))

#######################################
# C source build rule
#######################################
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "[CC]         $(notdir $<)"
	$Q$(CC) -c $(CFLAGS) $< -o $@

#######################################
# main target
#######################################
$(BUILD_DIR)/$(TARGET): $(DEMO_OBJECTS) $(OBJECTS) $(LIB_UTILS_OBJECTS) Makefile
	@echo "[LD]         $@"
	$Q$(CC) $(DEMO_OBJECTS) $(OBJECTS) $(LIB_UTILS_OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR):
	@echo "MKDIR          $(BUILD_DIR)"
	$Qmkdir $@

#######################################
# unit test target
#######################################
TEST_SRC=   \
unit_test/test_common.c \
unit_test/test_member_table.c \
unit_test/test_source_conflict.c \
unit_test/test_rtcp_encoder.c \
unit_test/test_basic.c \
unit_test/test_rtp.c \
unit_test/test_rtcp.c \
unit_test/test_bye.c \
unit_test/test_jitter.c

TEST_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(TEST_SRC:.c=.o)))
vpath %.c $(sort $(dir $(TEST_SRC)))

.PHONY: unit_test
unit_test: $(BUILD_DIR)/$(TARGET)_unit_test
	@echo "unit_test taget built"
	@$(BUILD_DIR)/$(TARGET)_unit_test

$(BUILD_DIR)/$(TARGET)_unit_test: unit_test/main.o $(TEST_OBJS) $(OBJECTS) Makefile
	@echo "[LD]         $@"
	$Q$(CC) unit_test/main.o $(TEST_OBJS) $(OBJECTS) $(LDFLAGS) -o $@ -lcunit

#######################################
# clean up
#######################################
clean:
	@echo "[CLEAN]          $(TARGET) $(BUILD_DIR) .dep"
	$Q-rm -fR .dep $(BUILD_DIR)
	$Q-rm -f test/*.o unit_test/*.o

#######################################
# dependencies
#######################################
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)
