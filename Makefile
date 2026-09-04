TOOLPREFIX ?= riscv-none-embed-
CC       := $(TOOLPREFIX)gcc
OBJCOPY  := $(TOOLPREFIX)objcopy
SIZE     := $(TOOLPREFIX)size
BUILD    ?= build/daplink
TARGET   ?= DAPLink
ROLE     ?= 1
DEBUG_LOG ?= 0

ARCHFLAGS := -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore
CFLAGS := $(ARCHFLAGS) -Os -g -std=gnu99 -ffunction-sections -fdata-sections -fno-common -fsigned-char \
          -DFIRMWARE_ROLE=$(ROLE)
ifeq ($(DEBUG_LOG),1)
CFLAGS += -DDEBUG=Debug_UART1
endif
INCLUDES := -ISRC/StdPeriphDriver/inc -ISRC/RVMSIS -ISRC/LIB -IHAL/include -IAPP/include -IAPP \
            -IDAP/Config -IDAP/Include -IThirdParty/CherryRB \
            -IThirdParty/CherryUSB/common -IThirdParty/CherryUSB/core \
            -IThirdParty/CherryUSB/class/cdc -IThirdParty/CherryUSB/port/ch32
LDFLAGS := $(ARCHFLAGS) -T SRC/Ld/Link.ld -nostartfiles -Wl,--gc-sections,-Map,$(BUILD)/DAP_CH591F.map \
           --specs=nano.specs --specs=nosys.specs -LSRC/StdPeriphDriver -LSRC/LIB
LDLIBS := -lCH59xBLE -lISP592 -lm

SRCS := APP/main.c APP/dap_main.c APP/usb_platform.c APP/RF_PHY.c APP/wireless_dap.c \
        HAL/uart.c HAL/activity_led.c HAL/flash_save.c HAL/MCU.c HAL/RTC.c HAL/SLEEP.c \
        SRC/RVMSIS/core_riscv.c SRC/StdPeriphDriver/CH59x_clk.c SRC/StdPeriphDriver/CH59x_gpio.c \
        SRC/StdPeriphDriver/CH59x_adc.c SRC/StdPeriphDriver/CH59x_flash.c \
        SRC/StdPeriphDriver/CH59x_pwr.c SRC/StdPeriphDriver/CH59x_sys.c \
        SRC/StdPeriphDriver/CH59x_uart0.c SRC/StdPeriphDriver/CH59x_uart1.c SRC/StdPeriphDriver/CH59x_uart2.c \
        DAP/Source/DAP.c DAP/Source/SW_DP.c DAP/Source/JTAG_DP.c DAP/Source/DAP_vendor.c \
        ThirdParty/CherryRB/chry_ringbuffer.c ThirdParty/CherryUSB/core/usbd_core.c \
        ThirdParty/CherryUSB/class/cdc/usbd_cdc_acm.c ThirdParty/CherryUSB/port/ch32/usb_ch58x_dc_usbfs.c
ASMS := SRC/Startup/startup_CH592.S SRC/LIB/ble_task_scheduler.S
OBJS := $(SRCS:%.c=$(BUILD)/%.o) $(ASMS:%.S=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

# Optimize the SWD/JTAG bit-banging hot paths for throughput.
$(BUILD)/DAP/Source/SW_DP.o: CFLAGS += -O3
$(BUILD)/DAP/Source/JTAG_DP.o: CFLAGS += -O3

.PHONY: all daplink usbdongle firmware clean
all: daplink usbdongle

daplink:
	+$(MAKE) --no-print-directory BUILD=build/daplink TARGET=DAPLink ROLE=1 firmware

usbdongle:
	+$(MAKE) --no-print-directory BUILD=build/usbdongle TARGET=USB_Dongle ROLE=0 firmware

firmware: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex

$(BUILD)/$(TARGET).elf: $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(SIZE) $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -x assembler-with-cpp -MMD -MP -c $< -o $@

clean:
	rm -rf build

-include $(DEPS)
