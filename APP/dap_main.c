#include "dap_main.h"
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "DAP_config.h"
#include "DAP.h"
#include "CH59x_common.h"
#include "uart.h"
#include "chry_ringbuffer.h"
#include "activity_led.h"
#include "wireless_dap.h"
#include <string.h>

#define USB_MPS 64
#define CONFIG_TOTAL_LEN (9 + 23 + CDC_ACM_DESCRIPTOR_LEN)
#define CDC_UART_COALESCE_TICKS (60000000U / 500U) /* 2 ms at 60 MHz */
#define CDC_USB_IN_TIMEOUT_TICKS (60000000U / 10U) /* 100 ms at 60 MHz */
#define DEBUG_DATA_PREVIEW_SIZE 16U
#define CDC_USB_OUT_BUFFER_SIZE 1024U

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_1, 0xEF, 0x02, 0x01, 0x0D28, 0x0204, 0x0100, 1)
};
static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(CONFIG_TOTAL_LEN, 3, 1, USB_CONFIG_BUS_POWERED, 200),
    USB_INTERFACE_DESCRIPTOR_INIT(0, 0, 2, 0xFF, 0, 0, 2),
    USB_ENDPOINT_DESCRIPTOR_INIT(DAP_OUT_EP, USB_ENDPOINT_TYPE_BULK, USB_MPS, 0),
    USB_ENDPOINT_DESCRIPTOR_INIT(DAP_IN_EP, USB_ENDPOINT_TYPE_BULK, USB_MPS, 0),
    CDC_ACM_DESCRIPTOR_INIT(1, CDC0_INT_EP, CDC0_OUT_EP, CDC0_IN_EP, USB_MPS, 0),
};
#define MSOS20_DESC_LEN 170
static const uint8_t msos20_descriptor[] = {
    /* Microsoft OS 2.0 descriptor set header (10 bytes). */
    0x0A,0x00, 0x00,0x00, 0x00,0x00,0x03,0x06, MSOS20_DESC_LEN,0x00,

    /* Function subset for interface 0 (160 bytes). */
    0x08,0x00, 0x02,0x00, 0x00,0x00, 0xA0,0x00,

    /* WINUSB compatible ID descriptor (20 bytes). */
    0x14,0x00, 0x03,0x00,
    'W','I','N','U','S','B',0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    /* CMSIS-DAP WinUSB interface GUID used by the reference firmware. */
    0x84,0x00, 0x04,0x00, 0x07,0x00, 0x2A,0x00,
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,
    'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,'a',0,'c',0,'e',0,
    'G',0,'U',0,'I',0,'D',0,'s',0,0,0,
    0x50,0x00,
    '{',0,
    'C',0,'D',0,'B',0,'3',0,'B',0,'5',0,'A',0,'D',0,'-',0,
    '2',0,'9',0,'3',0,'B',0,'-',0,
    '4',0,'6',0,'6',0,'3',0,'-',0,
    'A',0,'A',0,'3',0,'6',0,'-',0,
    '1',0,'A',0,'A',0,'E',0,'4',0,'6',0,'4',0,'6',0,'3',0,'7',0,'7',0,'6',0,
    '}',0,0,0,0,0
};
typedef char msos20_descriptor_size_must_be_170[
    (sizeof(msos20_descriptor) == MSOS20_DESC_LEN) ? 1 : -1];
static const uint8_t bos_descriptor[] = {
    0x05,USB_DESCRIPTOR_TYPE_BINARY_OBJECT_STORE,0x21,0,1,
    0x1C,USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY,0x05,0,
    0xDF,0x60,0xDD,0xD8,0x89,0x45,0xC7,0x4C,0x9C,0xD2,0x65,0x9D,0x9E,0x64,0x8A,0x9F,
    0,0,3,6,MSOS20_DESC_LEN,0,0x20,0
};
char ch592_dap_serial[17] = "0000000000000000";
static char *string_descriptors[] = {
    (char[]){0x09,0x04}, "Arm", "CMSIS-DAP v2", ch592_dap_serial, "CMSIS-DAP v2"
};
static const uint8_t *device_desc_cb(uint8_t speed) { (void)speed; return device_descriptor; }
static const uint8_t *config_desc_cb(uint8_t speed) { (void)speed; return config_descriptor; }
static const char *string_desc_cb(uint8_t speed, uint8_t index) {
    (void)speed;
    if (index >= sizeof(string_descriptors)/sizeof(string_descriptors[0])) return NULL;
    return string_descriptors[index];
}
static struct usb_msosv2_descriptor msosv2_desc = {
    .compat_id=msos20_descriptor, .compat_id_len=sizeof(msos20_descriptor), .vendor_code=0x20
};
static struct usb_bos_descriptor bos_desc = { .string=bos_descriptor, .string_len=sizeof(bos_descriptor) };
static const struct usb_descriptor dap_descriptor = {
    .device_descriptor_callback=device_desc_cb, .config_descriptor_callback=config_desc_cb,
    .string_descriptor_callback=string_desc_cb, .bos_descriptor=&bos_desc, .msosv2_descriptor=&msosv2_desc
};

static volatile uint16_t dap_req_index_in, dap_req_index_out;
static volatile uint16_t dap_req_count_in, dap_req_count_out;
static volatile uint8_t dap_req_idle;
static volatile uint16_t dap_resp_index_in, dap_resp_index_out;
static volatile uint16_t dap_resp_count_in, dap_resp_count_out;
static volatile uint8_t dap_resp_idle;
static USB_MEM_ALIGNX uint8_t dap_request[DAP_PACKET_COUNT][DAP_PACKET_SIZE];
static USB_MEM_ALIGNX uint8_t dap_response[DAP_PACKET_COUNT][DAP_PACKET_SIZE];
static uint16_t dap_request_size[DAP_PACKET_COUNT];
static uint16_t dap_response_size[DAP_PACKET_COUNT];
static USB_MEM_ALIGNX uint8_t cdc_out_data[USB_MPS], cdc_in_data[USB_MPS];
static USB_MEM_ALIGNX uint8_t cdc_uart_tx_pool[CDC_USB_OUT_BUFFER_SIZE];
static chry_ringbuffer_t cdc_uart_tx_rb;
static volatile uint8_t cdc_tx_busy;
static uint32_t cdc_tx_deadline;
static volatile uint32_t cdc_usb_out_dropped;
static struct cdc_line_coding line_coding = {115200,0,0,8};
static uint8_t cdc_usb_out_pending[USB_MPS];
static uint8_t cdc_usb_out_pending_length;
static uint8_t cdc_uart_pending[USB_MPS];
static uint8_t cdc_uart_pending_length;
static uint32_t cdc_uart_observed_length;
static uint32_t cdc_uart_quiet_deadline;
static uint8_t cdc_config_pending[7];
static volatile uint8_t cdc_config_pending_valid;

static bool wired_usb_active(void)
{
    return usb_device_is_configured(0) && !usb_device_is_suspend(0);
}

static void log_uart_data(const char *direction, const uint8_t *data, uint32_t len)
{
#ifdef DEBUG
    uint32_t i;
    uint32_t preview = len > DEBUG_DATA_PREVIEW_SIZE ?
                       DEBUG_DATA_PREVIEW_SIZE : len;
    printf("[UART0 %s] %lu byte:", direction, (unsigned long)len);
    for (i = 0; i < preview; i++) printf(" %02X", data[i]);
    if(preview != len) printf(" ...");
    printf("\r\n");
#else
    (void)direction; (void)data; (void)len;
#endif
}

static const char *dap_command_name(uint8_t command)
{
    switch (command) {
        case 0x00: return "Info";
        case 0x01: return "HostStatus";
        case 0x02: return "Connect";
        case 0x03: return "Disconnect";
        case 0x04: return "TransferConfigure";
        case 0x05: return "Transfer";
        case 0x06: return "TransferBlock";
        case 0x08: return "WriteABORT";
        case 0x09: return "Delay";
        case 0x0A: return "ResetTarget";
        case 0x10: return "SWJ_Pins";
        case 0x11: return "SWJ_Clock";
        case 0x12: return "SWJ_Sequence";
        case 0x13: return "SWD_Configure";
        case 0x14: return "JTAG_Sequence";
        case 0x15: return "JTAG_Configure";
        case 0x16: return "JTAG_IDCODE";
        case 0x17: return "SWO_Transport";
        case 0x18: return "SWO_Mode";
        case 0x19: return "SWO_Baudrate";
        case 0x1A: return "SWO_Control";
        case 0x1B: return "SWO_Status";
        case 0x1C: return "SWO_ExtendedStatus";
        case 0x1D: return "SWO_Data";
        case 0x7E: return "QueueCommands";
        case 0x7F: return "ExecuteCommands";
        default: return "Unknown";
    }
}

static void log_dap_packet(const char *direction, const uint8_t *data, uint32_t len)
{
#ifdef DEBUG
    uint32_t i;
    uint32_t preview = len > DEBUG_DATA_PREVIEW_SIZE ?
                       DEBUG_DATA_PREVIEW_SIZE : len;
    uint8_t command = len ? data[0] : 0xFFU;
    printf("[DAP %s] cmd=0x%02X(%s) len=%lu:", direction, command,
           dap_command_name(command), (unsigned long)len);
    for (i = 0; i < preview; i++) printf(" %02X", data[i]);
    if(preview != len) printf(" ...");
    printf("\r\n");
#else
    (void)direction; (void)data; (void)len;
#endif
}

static void dap_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes) {
    (void)busid; (void)ep; (void)nbytes;
    activity_led_pulse();
    dap_request_size[dap_req_index_in] = (uint16_t)nbytes;
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    if (dap_request[dap_req_index_in][0] == ID_DAP_TransferAbort) {
        DAP_TransferAbort = 1U;
    } else
#endif
    {
        dap_req_index_in++;
        if (dap_req_index_in == DAP_PACKET_COUNT) dap_req_index_in = 0U;
        dap_req_count_in++;
    }
    if ((uint16_t)(dap_req_count_in - dap_req_count_out) != DAP_PACKET_COUNT) {
        usbd_ep_start_read(0, DAP_OUT_EP, dap_request[dap_req_index_in], DAP_PACKET_SIZE);
    } else {
        dap_req_idle = 1U;
    }
}
static void dap_in_cb(uint8_t busid,uint8_t ep,uint32_t nbytes) {
    (void)busid;(void)ep;(void)nbytes;

    /* The buffer remains owned by USB until this completion callback. */
    dap_resp_index_out++;
    if (dap_resp_index_out == DAP_PACKET_COUNT) dap_resp_index_out = 0U;
    dap_resp_count_out++;

    if (dap_resp_count_in != dap_resp_count_out) {
        usbd_ep_start_write(0, DAP_IN_EP,
                            dap_response[dap_resp_index_out],
                            dap_response_size[dap_resp_index_out]);
    } else {
        dap_resp_idle = 1U;
    }
}
static void cdc_out_cb(uint8_t busid,uint8_t ep,uint32_t nbytes) {
    uint32_t written;
    (void)busid;
    activity_led_pulse();
    written = chry_ringbuffer_write(&cdc_uart_tx_rb,cdc_out_data,nbytes);
    cdc_usb_out_dropped += nbytes - written;
    usbd_ep_start_read(0,ep,cdc_out_data,USB_MPS);
}
static void cdc_in_cb(uint8_t busid,uint8_t ep,uint32_t nbytes) {
    (void)busid;(void)ep;
    PRINT("[USB CDC IN DONE] %lu byte\n", (unsigned long)nbytes);
    cdc_tx_busy=0;
}
static struct usbd_endpoint dap_out={DAP_OUT_EP,dap_out_cb},dap_in={DAP_IN_EP,dap_in_cb};
static struct usbd_endpoint cdc0_out={CDC0_OUT_EP,cdc_out_cb},cdc0_in={CDC0_IN_EP,cdc_in_cb};
static struct usbd_interface dap_intf,cdc0_ctrl,cdc0_data;

static void dap_serial_init(void) {
    static const char hex[] = "0123456789ABCDEF";
    __attribute__((aligned(4))) uint8_t unique_id[8];
    uint32_t i;

    GET_UNIQUE_ID(unique_id);
    for (i = 0; i < sizeof(unique_id); i++) {
        ch592_dap_serial[i * 2U] = hex[unique_id[i] >> 4];
        ch592_dap_serial[i * 2U + 1U] = hex[unique_id[i] & 0x0FU];
    }
    ch592_dap_serial[16] = '\0';
}

void usbd_event_handler(uint8_t busid,uint8_t event) {
    (void)busid;
    if(event==USBD_EVENT_CONFIGURED) {
        dap_req_index_in = dap_req_index_out = 0U;
        dap_req_count_in = dap_req_count_out = 0U;
        dap_resp_index_in = dap_resp_index_out = 0U;
        dap_resp_count_in = dap_resp_count_out = 0U;
        dap_req_idle = 0U;
        dap_resp_idle = 1U;
        usbd_ep_start_read(0,DAP_OUT_EP,dap_request[0],DAP_PACKET_SIZE);
        usbd_ep_start_read(0,CDC0_OUT_EP,cdc_out_data,USB_MPS);
    }
}
void usbd_cdc_acm_set_line_coding(uint8_t busid,uint8_t intf,struct cdc_line_coding *c) {
    (void)busid; (void)intf; line_coding=*c;
    if(c->dwDTERate) {
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
        UART0_BaudRateCfg(c->dwDTERate);
#else
        cdc_config_pending[0] = (uint8_t)c->dwDTERate;
        cdc_config_pending[1] = (uint8_t)(c->dwDTERate >> 8);
        cdc_config_pending[2] = (uint8_t)(c->dwDTERate >> 16);
        cdc_config_pending[3] = (uint8_t)(c->dwDTERate >> 24);
        cdc_config_pending[4] = c->bCharFormat;
        cdc_config_pending[5] = c->bParityType;
        cdc_config_pending[6] = c->bDataBits;
        cdc_config_pending_valid = 1U;
#endif
        activity_led_pulse();
    }
}
void usbd_cdc_acm_get_line_coding(uint8_t busid,uint8_t intf,struct cdc_line_coding *c) {
    (void)busid; (void)intf; *c=line_coding;
}
void ch592_dap_init(void) {
    dap_serial_init();
    chry_ringbuffer_init(&cdc_uart_tx_rb,cdc_uart_tx_pool,sizeof(cdc_uart_tx_pool));
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    DAP_Setup();
#endif
    usbd_desc_register(0,&dap_descriptor);
    usbd_add_interface(0,&dap_intf); usbd_add_endpoint(0,&dap_out); usbd_add_endpoint(0,&dap_in);
    usbd_add_interface(0,usbd_cdc_acm_init_intf(0,&cdc0_ctrl)); usbd_add_interface(0,usbd_cdc_acm_init_intf(0,&cdc0_data));
    usbd_add_endpoint(0,&cdc0_out); usbd_add_endpoint(0,&cdc0_in);
    usbd_initialize(0,0x40008000,usbd_event_handler);
}
static void cdc_process_usb_out(void)
{
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    uint8_t wireless_data[USB_MPS];
    uint8_t wireless_length;
    uint8_t config[7];
    uint8_t config_length;

    if(!wired_usb_active())
    {
        if(WirelessDAP_TakeCdcData(wireless_data, &wireless_length))
        {
            log_uart_data("RF->TX", wireless_data, wireless_length);
            UART0_SendString(wireless_data, wireless_length);
        }
        if(WirelessDAP_TakeCdcConfig(config, &config_length) &&
           (config_length == sizeof(config)))
        {
            uint32_t baud = (uint32_t)config[0] |
                            ((uint32_t)config[1] << 8) |
                            ((uint32_t)config[2] << 16) |
                            ((uint32_t)config[3] << 24);
            if(baud != 0U)
            {
                UART0_BaudRateCfg(baud);
                PRINT("[RF CDC CFG] %lu baud, data=%u parity=%u stop=%u\n",
                      (unsigned long)baud, config[6], config[5], config[4]);
            }
        }
    }
#endif

    if(cdc_usb_out_dropped != 0U)
    {
        uint32_t dropped = cdc_usb_out_dropped;
        cdc_usb_out_dropped = 0U;
        PRINT("[USB CDC OUT OVERFLOW] dropped=%lu\n",
              (unsigned long)dropped);
    }

    if(cdc_usb_out_pending_length == 0U)
    {
        cdc_usb_out_pending_length = (uint8_t)chry_ringbuffer_read(
            &cdc_uart_tx_rb, cdc_usb_out_pending, sizeof(cdc_usb_out_pending));
        if(cdc_usb_out_pending_length != 0U)
        {
            PRINT("[USB CDC OUT] %u byte\n", cdc_usb_out_pending_length);
        }
    }
    if(cdc_usb_out_pending_length == 0U)
    {
        return;
    }

#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    log_uart_data("TX", cdc_usb_out_pending, cdc_usb_out_pending_length);
    UART0_SendString(cdc_usb_out_pending, cdc_usb_out_pending_length);
    cdc_usb_out_pending_length = 0U;
#else
    if(WirelessDAP_SubmitCdcData(cdc_usb_out_pending,
                                 cdc_usb_out_pending_length))
    {
        log_uart_data("USB->RF", cdc_usb_out_pending,
                      cdc_usb_out_pending_length);
        cdc_usb_out_pending_length = 0U;
    }
#endif
}

static void cdc_process_input(void)
{
    uint32_t length;

    if(cdc_tx_busy &&
       ((int32_t)(SYS_GetSysTickCnt() - cdc_tx_deadline) >= 0))
    {
        PRINT("[USB CDC IN RECOVER] completion timeout\n");
        cdc_tx_busy = 0U;
    }

#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    if(wired_usb_active())
    {
        if(cdc_tx_busy)
        {
            return;
        }
        length = chry_ringbuffer_read(&cb_uart0, cdc_in_data, USB_MPS);
        if(length != 0U)
        {
            log_uart_data("RX", cdc_in_data, length);
            activity_led_pulse();
            cdc_tx_busy = 1U;
            cdc_tx_deadline = SYS_GetSysTickCnt() + CDC_USB_IN_TIMEOUT_TICKS;
            usbd_ep_start_write(0, CDC0_IN_EP, cdc_in_data, length);
        }
        return;
    }

    if(cdc_uart_pending_length == 0U)
    {
        uint32_t available = chry_ringbuffer_get_used(&cb_uart0);
        uint32_t now;

        if(available == 0U)
        {
            cdc_uart_observed_length = 0U;
            return;
        }

        now = SYS_GetSysTickCnt();
        if((available < sizeof(cdc_uart_pending)) &&
           (available != cdc_uart_observed_length))
        {
            /* UART interrupt often exposes a burst as 1 + N bytes. Wait for
             * a short quiet period so one serial burst becomes one RF packet. */
            cdc_uart_observed_length = available;
            cdc_uart_quiet_deadline = now + CDC_UART_COALESCE_TICKS;
            return;
        }
        if((available < sizeof(cdc_uart_pending)) &&
           ((int32_t)(now - cdc_uart_quiet_deadline) < 0))
        {
            return;
        }

        cdc_uart_pending_length = (uint8_t)chry_ringbuffer_read(
            &cb_uart0, cdc_uart_pending, sizeof(cdc_uart_pending));
        cdc_uart_observed_length = 0U;
        if(cdc_uart_pending_length != 0U)
        {
            log_uart_data("RX PENDING", cdc_uart_pending,
                          cdc_uart_pending_length);
        }
    }
    if((cdc_uart_pending_length != 0U) &&
       WirelessDAP_SubmitCdcData(cdc_uart_pending, cdc_uart_pending_length))
    {
        log_uart_data("RX->RF", cdc_uart_pending, cdc_uart_pending_length);
        cdc_uart_pending_length = 0U;
    }
#else
    uint8_t wireless_length;

    if(cdc_config_pending_valid &&
       WirelessDAP_SubmitCdcConfig(cdc_config_pending,
                                   sizeof(cdc_config_pending)))
    {
        cdc_config_pending_valid = 0U;
    }
    if(cdc_tx_busy || !usb_device_is_configured(0))
    {
        return;
    }
    if(WirelessDAP_TakeCdcData(cdc_in_data, &wireless_length))
    {
        int usb_status;

        length = wireless_length;
        log_uart_data("RF->USB", cdc_in_data, length);
        cdc_tx_busy = 1U;
        cdc_tx_deadline = SYS_GetSysTickCnt() + CDC_USB_IN_TIMEOUT_TICKS;
        usb_status = usbd_ep_start_write(0, CDC0_IN_EP, cdc_in_data, length);
        PRINT("[USB CDC IN START] len=%lu status=%d\n",
              (unsigned long)length, usb_status);
        if(usb_status < 0)
        {
            cdc_tx_busy = 0U;
        }
    }
#endif
}
static void dap_process(void) {
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    while ((dap_req_count_in != dap_req_count_out) &&
           ((uint16_t)(dap_resp_count_in - dap_resp_count_out) < DAP_PACKET_COUNT)) {

        log_dap_packet("OUT", dap_request[dap_req_index_out],
                       dap_request_size[dap_req_index_out]);
        dap_response_size[dap_resp_index_in] = (uint16_t)DAP_ExecuteCommand(
            dap_request[dap_req_index_out], dap_response[dap_resp_index_in]);
        log_dap_packet("IN", dap_response[dap_resp_index_in],
                       dap_response_size[dap_resp_index_in]);

        dap_req_index_out++;
        if (dap_req_index_out == DAP_PACKET_COUNT) dap_req_index_out = 0U;
        dap_req_count_out++;

        if (dap_req_idle &&
            ((uint16_t)(dap_req_count_in - dap_req_count_out) != DAP_PACKET_COUNT)) {
            dap_req_idle = 0U;
            usbd_ep_start_read(0, DAP_OUT_EP, dap_request[dap_req_index_in], DAP_PACKET_SIZE);
        }

        dap_resp_index_in++;
        if (dap_resp_index_in == DAP_PACKET_COUNT) dap_resp_index_in = 0U;
        dap_resp_count_in++;

        if (dap_resp_idle && (dap_resp_count_in != dap_resp_count_out)) {
            dap_resp_idle = 0U;
            usbd_ep_start_write(0, DAP_IN_EP,
                                dap_response[dap_resp_index_out],
                                dap_response_size[dap_resp_index_out]);
        }
    }
#else
    uint8_t wireless_length;

    if((dap_req_count_in != dap_req_count_out) &&
       WirelessDAP_SubmitRequest(dap_request[dap_req_index_out],
                                 (uint8_t)dap_request_size[dap_req_index_out]))
    {
        dap_req_index_out++;
        if(dap_req_index_out == DAP_PACKET_COUNT) dap_req_index_out = 0U;
        dap_req_count_out++;
        if(dap_req_idle &&
           ((uint16_t)(dap_req_count_in - dap_req_count_out) != DAP_PACKET_COUNT))
        {
            dap_req_idle = 0U;
            usbd_ep_start_read(0, DAP_OUT_EP, dap_request[dap_req_index_in],
                               DAP_PACKET_SIZE);
        }
    }

    if(((uint16_t)(dap_resp_count_in - dap_resp_count_out) < DAP_PACKET_COUNT) &&
       WirelessDAP_TakeResponse(dap_response[dap_resp_index_in],
                                &wireless_length))
    {
        dap_response_size[dap_resp_index_in] = wireless_length;
        dap_resp_index_in++;
        if(dap_resp_index_in == DAP_PACKET_COUNT) dap_resp_index_in = 0U;
        dap_resp_count_in++;
        if(dap_resp_idle)
        {
            dap_resp_idle = 0U;
            usbd_ep_start_write(0, DAP_IN_EP,
                                dap_response[dap_resp_index_out],
                                dap_response_size[dap_resp_index_out]);
        }
    }
#endif
}

void ch592_dap_poll(void) {
    uint8_t wireless_request[WIRELESS_DAP_PACKET_SIZE];
    uint8_t wireless_response[WIRELESS_DAP_PACKET_SIZE];
    uint8_t wireless_length;

    dap_process();
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    WirelessDAP_SetWiredActive(wired_usb_active());
    if(!wired_usb_active() &&
       WirelessDAP_TakeRequest(wireless_request, &wireless_length))
    {
        wireless_length = (uint8_t)DAP_ExecuteCommand(wireless_request,
                                                      wireless_response);
        WirelessDAP_SubmitResponse(wireless_response, wireless_length);
    }
#else
    (void)wireless_request;
    (void)wireless_response;
    (void)wireless_length;
#endif
    cdc_process_input();
    cdc_process_usb_out();
}
