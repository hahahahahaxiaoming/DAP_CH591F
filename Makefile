TOOLPREFIX ?= riscv-none-embed-
CC       := $(TOOLPREFIX)gcc
OBJCOPY  := $(TOOLPREFIX)objcopy
SIZE     := $(TOOLPREFIX)size
BUILD    := build

ARCHFLAGS := -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore
CFLAGS := $(ARCHFLAGS) -Os -g -std=gnu99 -ffunction-sections -fdata-sections -fno-common -fsigned-char
INCLUDES := -ISRC/StdPeriphDriver/inc -ISRC/RVMSIS -IHAL/include -IAPP/include -IAPP \
            -IDAP/Config -IDAP/Include -IThirdParty/CherryRB \
            -IThirdParty/CherryUSB/common -IThirdParty/CherryUSB/core \
            -IThirdParty/CherryUSB/class/cdc -IThirdParty/CherryUSB/port/ch32
LDFLAGS := $(ARCHFLAGS) -T SRC/Ld/Link.ld -nostartfiles -Wl,--gc-sections,-Map,$(BUILD)/DAP_CH591F.map \
           --specs=nano.specs --specs=nosys.specs -LSRC/StdPeriphDriver
LDLIBS := -lISP592 -lm

SRCS := APP/main.c APP/dap_main.c APP/usb_platform.c HAL/uart.c HAL/activity_led.c HAL/hal_time.c \
        SRC/RVMSIS/core_riscv.c SRC/StdPeriphDriver/CH59x_clk.c SRC/StdPeriphDriver/CH59x_gpio.c \
        SRC/StdPeriphDriver/CH59x_pwr.c SRC/StdPeriphDriver/CH59x_sys.c \
        SRC/StdPeriphDriver/CH59x_uart0.c SRC/StdPeriphDriver/CH59x_uart1.c SRC/StdPeriphDriver/CH59x_uart2.c \
        DAP/Source/DAP.c DAP/Source/SW_DP.c DAP/Source/JTAG_DP.c DAP/Source/DAP_vendor.c \
        ThirdParty/CherryRB/chry_ringbuffer.c ThirdParty/CherryUSB/core/usbd_core.c \
        ThirdParty/CherryUSB/class/cdc/usbd_cdc_acm.c ThirdParty/CherryUSB/port/ch32/usb_ch58x_dc_usbfs.c
ASMS := SRC/Startup/startup_CH592.S
OBJS := $(SRCS:%.c=$(BUILD)/%.o) $(ASMS:%.S=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

# Optimize the SWD/JTAG bit-banging hot paths for throughput.
$(BUILD)/DAP/Source/SW_DP.o: CFLAGS += -O3
$(BUILD)/DAP/Source/JTAG_DP.o: CFLAGS += -O3

.PHONY: all clean
all: $(BUILD)/DAP_CH591F.elf $(BUILD)/DAP_CH591F.hex

$(BUILD)/DAP_CH591F.elf: $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(SIZE) $@

$(BUILD)/DAP_CH591F.hex: $(BUILD)/DAP_CH591F.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -x assembler-with-cpp -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPS)
