#include "CONFIG.h"
#include "wireless_dap.h"
#include "RF_PHY.h"
#include "activity_led.h"
#include "flash_save.h"
#include <stddef.h>
#include <string.h>

#define WDAP_MAGIC                  0x50414457UL /* "WDAP" */
#define WDAP_VERSION                1U
#define WDAP_PAIR_RSSI_MIN_DBM      (-50)
#define WDAP_PAIR_PERIOD_MS         100U
#define WDAP_CONFIRM_TIMEOUT_MS     500U
#define WDAP_DAP_TIMEOUT_MS         100U
#define WDAP_DAP_MAX_RETRIES        5U

#define WDAP_EVT_SEND               0x0001
#define WDAP_EVT_START_RX           0x0002
#define WDAP_EVT_RX_READY           0x0004
#define WDAP_EVT_TX_DONE            0x0008
#define WDAP_EVT_PAIR_TICK          0x0010
#define WDAP_EVT_TIMEOUT            0x0020
#define WDAP_EVT_LED                0x0040

typedef enum
{
    WDAP_PACKET_PAIR_REQUEST = 1,
    WDAP_PACKET_PAIR_ACK,
    WDAP_PACKET_PAIR_CONFIRM,
    WDAP_PACKET_PAIR_CONFIRM_ACK,
    WDAP_PACKET_DAP_REQUEST,
    WDAP_PACKET_DAP_RESPONSE
} wireless_packet_type_t;

typedef struct
{
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t channel;
    uint8_t length;
    uint32_t sequence;
    uint64_t pair_id;
    uint8_t payload[WIRELESS_DAP_PACKET_SIZE];
    uint32_t checksum;
} __attribute__((packed)) wireless_packet_t;

typedef enum
{
    WDAP_STATE_IDLE,
    WDAP_STATE_PAIR_LISTEN,
    WDAP_STATE_PAIR_ACK_TX,
    WDAP_STATE_WAIT_PAIR_ACK,
    WDAP_STATE_WAIT_CONFIRM,
    WDAP_STATE_WAIT_CONFIRM_ACK,
    WDAP_STATE_CONFIRM_ACK_TX,
    WDAP_STATE_PAIRED_RX,
    WDAP_STATE_DAP_RESPONSE_TX,
    WDAP_STATE_WAIT_DAP_RESPONSE,
    WDAP_STATE_RECONFIRM_TX
} wireless_state_t;

static tmosTaskID wireless_task_id;
static wireless_state_t wireless_state;
static wireless_packet_t tx_packet;
static wireless_packet_t rx_packet;
static volatile uint8_t rx_ready;
static volatile int8_t received_rssi;
static uint8_t request_data[WIRELESS_DAP_PACKET_SIZE];
static uint8_t response_data[WIRELESS_DAP_PACKET_SIZE];
static volatile uint8_t request_ready;
static volatile uint8_t response_ready;
static uint8_t request_length;
static uint8_t response_length;
static uint8_t proposed_channel;
static uint64_t proposed_pair_id;
static uint32_t pairing_nonce;
static uint32_t dap_sequence;
static uint32_t last_request_sequence;
static uint8_t last_response[WIRELESS_DAP_PACKET_SIZE];
static uint8_t last_response_length;
static uint8_t retry_count;
static bool wired_active;

static uint32_t WirelessChecksum(const wireless_packet_t *packet)
{
    const uint8_t *bytes = (const uint8_t *)packet;
    uint32_t hash = 2166136261UL;
    uint32_t index;

    for(index = 0; index < offsetof(wireless_packet_t, checksum); index++)
    {
        hash = (hash ^ bytes[index]) * 16777619UL;
    }
    return hash;
}

static void WirelessBuildPacket(wireless_packet_t *packet, uint8_t type,
                                uint32_t sequence, uint64_t pair_id,
                                uint8_t channel, const uint8_t *payload,
                                uint8_t length)
{
    memset(packet, 0, sizeof(*packet));
    packet->magic = WDAP_MAGIC;
    packet->version = WDAP_VERSION;
    packet->type = type;
    packet->channel = channel;
    packet->length = length;
    packet->sequence = sequence;
    packet->pair_id = pair_id;
    if((payload != NULL) && (length != 0U))
    {
        memcpy(packet->payload, payload, length);
    }
    packet->checksum = WirelessChecksum(packet);
}

static bool WirelessPacketValid(const wireless_packet_t *packet)
{
    return (packet->magic == WDAP_MAGIC) &&
           (packet->version == WDAP_VERSION) &&
           (packet->length <= WIRELESS_DAP_PACKET_SIZE) &&
           (packet->checksum == WirelessChecksum(packet));
}

static uint32_t WirelessRandom32(void)
{
    static uint32_t random_state;
    __attribute__((aligned(4))) uint8_t unique_id[8];
    uint8_t index;

    if(random_state == 0U)
    {
        GET_UNIQUE_ID(unique_id);
        random_state = SYS_GetSysTickCnt() ^ 0x9E3779B9UL;
        for(index = 0; index < sizeof(unique_id); index++)
        {
            random_state = (random_state * 33U) ^ unique_id[index];
        }
        if(random_state == 0U)
        {
            random_state = 1U;
        }
    }

    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void WirelessStartReceive(void)
{
    if(RF_PHY_StartReceive() != SUCCESS)
    {
        tmos_start_task(wireless_task_id, WDAP_EVT_START_RX,
                        MS1_TO_SYSTEM_TIME(2));
    }
}

static void WirelessSendCurrent(void)
{
    if(RF_PHY_Send(&tx_packet, sizeof(tx_packet)) != SUCCESS)
    {
        tmos_start_task(wireless_task_id, WDAP_EVT_SEND,
                        MS1_TO_SYSTEM_TIME(2));
    }
}

static void WirelessSavePairing(void)
{
    g_flash_save_config.paired = 1U;
    g_flash_save_config.channel = proposed_channel;
    g_flash_save_config.pair_id = proposed_pair_id;

    if(FlashSave_Save() == FLASH_SAVE_STATUS_OK)
    {
        activity_led_off();
        PRINT("[PAIR] complete: channel=%u id=%08x%08x\n",
              proposed_channel, (uint32_t)(proposed_pair_id >> 32),
              (uint32_t)proposed_pair_id);
    }
    else
    {
        PRINT("[PAIR] flash save failed\n");
    }
}

static void WirelessRadioCallback(rf_phy_event_t event, int8_t rssi,
                                  const uint8_t *data, uint8_t length)
{
    if((event == RF_PHY_EVENT_RX_DATA) &&
       (length == sizeof(wireless_packet_t)))
    {
        memcpy(&rx_packet, data, sizeof(rx_packet));
        received_rssi = rssi;
        rx_ready = 1U;
        tmos_set_event(wireless_task_id, WDAP_EVT_RX_READY);
    }
    else if(event == RF_PHY_EVENT_TX_DONE)
    {
        tmos_set_event(wireless_task_id, WDAP_EVT_TX_DONE);
    }
    else if(event == RF_PHY_EVENT_TX_FAILED)
    {
        tmos_start_task(wireless_task_id, WDAP_EVT_SEND,
                        MS1_TO_SYSTEM_TIME(2));
    }
}

static void WirelessReturnToPairChannel(void)
{
    RF_PHY_SetChannel(RF_PHY_PAIR_CHANNEL);
    wireless_state = WDAP_STATE_PAIR_LISTEN;
    proposed_pair_id = 0U;
    proposed_channel = 0U;
    tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
}

static void WirelessHandlePairRequest(const wireless_packet_t *packet,
                                      int8_t rssi)
{
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    if((wireless_state != WDAP_STATE_PAIR_LISTEN) ||
       (packet->type != WDAP_PACKET_PAIR_REQUEST))
    {
        return;
    }
    if(rssi <= WDAP_PAIR_RSSI_MIN_DBM)
    {
        PRINT("[PAIR] ignored, RSSI=%d dBm\n", rssi);
        tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
        return;
    }
    if((packet->pair_id == 0U) || (packet->channel < 1U) ||
       (packet->channel > RF_PHY_MAX_CHANNEL))
    {
        tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
        return;
    }

    pairing_nonce = packet->sequence;
    proposed_pair_id = packet->pair_id;
    proposed_channel = packet->channel;
    WirelessBuildPacket(&tx_packet, WDAP_PACKET_PAIR_ACK, pairing_nonce,
                        proposed_pair_id, proposed_channel, NULL, 0);
    wireless_state = WDAP_STATE_PAIR_ACK_TX;
    PRINT("[PAIR] accepted, RSSI=%d dBm, channel=%u\n", rssi,
          proposed_channel);
    tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
#else
    (void)packet;
    (void)rssi;
#endif
}

static void WirelessHandleReceived(void)
{
    wireless_packet_t packet;
    int8_t rssi;
    uint32_t irq_status;

    if(rx_ready == 0U)
    {
        return;
    }
    SYS_DisableAllIrq(&irq_status);
    memcpy(&packet, &rx_packet, sizeof(packet));
    rssi = received_rssi;
    rx_ready = 0U;
    SYS_RecoverIrq(irq_status);

    if(!WirelessPacketValid(&packet))
    {
        tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
        return;
    }

    if(!FlashSave_IsPaired())
    {
#if FIRMWARE_ROLE == FIRMWARE_ROLE_USB_DONGLE
        if((wireless_state == WDAP_STATE_WAIT_PAIR_ACK) &&
           (packet.type == WDAP_PACKET_PAIR_ACK) &&
           (packet.sequence == pairing_nonce) &&
           (packet.pair_id == proposed_pair_id) &&
           (packet.channel == proposed_channel))
        {
            RF_PHY_SetChannel(proposed_channel);
            WirelessBuildPacket(&tx_packet, WDAP_PACKET_PAIR_CONFIRM,
                                pairing_nonce, proposed_pair_id,
                                proposed_channel, NULL, 0);
            wireless_state = WDAP_STATE_WAIT_CONFIRM_ACK;
            tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
            return;
        }
        if((wireless_state == WDAP_STATE_WAIT_CONFIRM_ACK) &&
           (packet.type == WDAP_PACKET_PAIR_CONFIRM_ACK) &&
           (packet.sequence == pairing_nonce) &&
           (packet.pair_id == proposed_pair_id))
        {
            tmos_stop_task(wireless_task_id, WDAP_EVT_PAIR_TICK);
            WirelessSavePairing();
            wireless_state = WDAP_STATE_IDLE;
            return;
        }
#else
        WirelessHandlePairRequest(&packet, rssi);
        if((wireless_state == WDAP_STATE_WAIT_CONFIRM) &&
           (packet.type == WDAP_PACKET_PAIR_CONFIRM) &&
           (packet.sequence == pairing_nonce) &&
           (packet.pair_id == proposed_pair_id) &&
           (packet.channel == proposed_channel))
        {
            tmos_stop_task(wireless_task_id, WDAP_EVT_TIMEOUT);
            WirelessBuildPacket(&tx_packet, WDAP_PACKET_PAIR_CONFIRM_ACK,
                                pairing_nonce, proposed_pair_id,
                                proposed_channel, NULL, 0);
            wireless_state = WDAP_STATE_CONFIRM_ACK_TX;
            tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
            return;
        }
#endif
        return;
    }

#if FIRMWARE_ROLE == FIRMWARE_ROLE_USB_DONGLE
    if((wireless_state == WDAP_STATE_WAIT_DAP_RESPONSE) &&
       (packet.type == WDAP_PACKET_DAP_RESPONSE) &&
       (packet.pair_id == g_flash_save_config.pair_id) &&
       (packet.sequence == dap_sequence))
    {
        memcpy(response_data, packet.payload, packet.length);
        response_length = packet.length;
        response_ready = 1U;
        wireless_state = WDAP_STATE_IDLE;
        tmos_stop_task(wireless_task_id, WDAP_EVT_TIMEOUT);
        activity_led_pulse();
        return;
    }
#else
    if((packet.type == WDAP_PACKET_PAIR_CONFIRM) &&
       (packet.pair_id == g_flash_save_config.pair_id))
    {
        WirelessBuildPacket(&tx_packet, WDAP_PACKET_PAIR_CONFIRM_ACK,
                            packet.sequence, packet.pair_id,
                            g_flash_save_config.channel, NULL, 0);
        wireless_state = WDAP_STATE_RECONFIRM_TX;
        tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
        return;
    }
    if((packet.type == WDAP_PACKET_DAP_REQUEST) &&
       (packet.pair_id == g_flash_save_config.pair_id))
    {
        if(wired_active)
        {
            tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
            return;
        }
        if((packet.sequence == last_request_sequence) &&
           (last_response_length != 0U))
        {
            WirelessBuildPacket(&tx_packet, WDAP_PACKET_DAP_RESPONSE,
                                packet.sequence, packet.pair_id, 0,
                                last_response, last_response_length);
            wireless_state = WDAP_STATE_DAP_RESPONSE_TX;
            tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
            return;
        }
        if(request_ready == 0U)
        {
            memcpy(request_data, packet.payload, packet.length);
            request_length = packet.length;
            last_request_sequence = packet.sequence;
            request_ready = 1U;
            activity_led_pulse();
            return;
        }
    }
#endif
    tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
}

static tmosEvents WirelessProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if(events & SYS_EVENT_MSG)
    {
        uint8_t *message = tmos_msg_receive(task_id);
        if(message != NULL)
        {
            tmos_msg_deallocate(message);
        }
        return events ^ SYS_EVENT_MSG;
    }
    if(events & WDAP_EVT_LED)
    {
        if(!FlashSave_IsPaired())
        {
            activity_led_pulse();
            tmos_start_task(wireless_task_id, WDAP_EVT_LED,
                            MS1_TO_SYSTEM_TIME(200));
        }
        return events ^ WDAP_EVT_LED;
    }
    if(events & WDAP_EVT_RX_READY)
    {
        WirelessHandleReceived();
        return events ^ WDAP_EVT_RX_READY;
    }
    if(events & WDAP_EVT_SEND)
    {
        WirelessSendCurrent();
        return events ^ WDAP_EVT_SEND;
    }
    if(events & WDAP_EVT_START_RX)
    {
        WirelessStartReceive();
        return events ^ WDAP_EVT_START_RX;
    }
    if(events & WDAP_EVT_TX_DONE)
    {
#if FIRMWARE_ROLE == FIRMWARE_ROLE_USB_DONGLE
        if((wireless_state == WDAP_STATE_WAIT_PAIR_ACK) ||
           (wireless_state == WDAP_STATE_WAIT_CONFIRM_ACK) ||
           (wireless_state == WDAP_STATE_WAIT_DAP_RESPONSE))
        {
            tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
        }
#else
        if(wireless_state == WDAP_STATE_PAIR_ACK_TX)
        {
            RF_PHY_SetChannel(proposed_channel);
            wireless_state = WDAP_STATE_WAIT_CONFIRM;
            tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
            tmos_start_task(wireless_task_id, WDAP_EVT_TIMEOUT,
                            MS1_TO_SYSTEM_TIME(WDAP_CONFIRM_TIMEOUT_MS));
        }
        else if(wireless_state == WDAP_STATE_CONFIRM_ACK_TX)
        {
            WirelessSavePairing();
            wireless_state = WDAP_STATE_PAIRED_RX;
            tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
        }
        else if((wireless_state == WDAP_STATE_DAP_RESPONSE_TX) ||
                (wireless_state == WDAP_STATE_RECONFIRM_TX))
        {
            wireless_state = WDAP_STATE_PAIRED_RX;
            tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
        }
#endif
        return events ^ WDAP_EVT_TX_DONE;
    }
    if(events & WDAP_EVT_PAIR_TICK)
    {
#if FIRMWARE_ROLE == FIRMWARE_ROLE_USB_DONGLE
        if(!FlashSave_IsPaired())
        {
            if(wireless_state == WDAP_STATE_WAIT_CONFIRM_ACK)
            {
                RF_PHY_SetChannel(proposed_channel);
                WirelessBuildPacket(&tx_packet, WDAP_PACKET_PAIR_CONFIRM,
                                    pairing_nonce, proposed_pair_id,
                                    proposed_channel, NULL, 0);
            }
            else
            {
                RF_PHY_SetChannel(RF_PHY_PAIR_CHANNEL);
                pairing_nonce = WirelessRandom32();
                proposed_channel = (uint8_t)(1U + (WirelessRandom32() % 39U));
                proposed_pair_id = ((uint64_t)WirelessRandom32() << 32) |
                                   WirelessRandom32();
                if(proposed_pair_id == 0U)
                {
                    proposed_pair_id = 1U;
                }
                WirelessBuildPacket(&tx_packet, WDAP_PACKET_PAIR_REQUEST,
                                    pairing_nonce, proposed_pair_id,
                                    proposed_channel, NULL, 0);
                wireless_state = WDAP_STATE_WAIT_PAIR_ACK;
            }
            tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
            tmos_start_task(wireless_task_id, WDAP_EVT_PAIR_TICK,
                            MS1_TO_SYSTEM_TIME(WDAP_PAIR_PERIOD_MS));
        }
#endif
        return events ^ WDAP_EVT_PAIR_TICK;
    }
    if(events & WDAP_EVT_TIMEOUT)
    {
#if FIRMWARE_ROLE == FIRMWARE_ROLE_USB_DONGLE
        if(wireless_state == WDAP_STATE_WAIT_DAP_RESPONSE)
        {
            if(retry_count++ < WDAP_DAP_MAX_RETRIES)
            {
                tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
                tmos_start_task(wireless_task_id, WDAP_EVT_TIMEOUT,
                                MS1_TO_SYSTEM_TIME(WDAP_DAP_TIMEOUT_MS));
            }
            else
            {
                response_data[0] = tx_packet.payload[0];
                response_data[1] = 0xFFU;
                response_length = 2U;
                response_ready = 1U;
                wireless_state = WDAP_STATE_IDLE;
                PRINT("[RF] DAP response timeout\n");
            }
        }
#else
        if(wireless_state == WDAP_STATE_WAIT_CONFIRM)
        {
            WirelessReturnToPairChannel();
        }
#endif
        return events ^ WDAP_EVT_TIMEOUT;
    }
    return 0;
}

void WirelessDAP_Init(void)
{
    FlashSave_Init();
    wireless_task_id = TMOS_ProcessEventRegister(WirelessProcessEvent);
    if(RF_PHY_Init(FlashSave_IsPaired() ? g_flash_save_config.channel :
                   RF_PHY_PAIR_CHANNEL, WirelessRadioCallback) != SUCCESS)
    {
        PRINT("[RF] initialization failed\n");
        while(1);
    }

    if(FlashSave_IsPaired())
    {
        proposed_channel = g_flash_save_config.channel;
        proposed_pair_id = g_flash_save_config.pair_id;
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
        wireless_state = WDAP_STATE_PAIRED_RX;
        tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
#else
        wireless_state = WDAP_STATE_IDLE;
#endif
        activity_led_off();
        PRINT("[PAIR] restored: channel=%u id=%08x%08x\n",
              proposed_channel, (uint32_t)(proposed_pair_id >> 32),
              (uint32_t)proposed_pair_id);
    }
    else
    {
        tmos_set_event(wireless_task_id, WDAP_EVT_LED);
#if FIRMWARE_ROLE == FIRMWARE_ROLE_USB_DONGLE
        wireless_state = WDAP_STATE_IDLE;
        tmos_set_event(wireless_task_id, WDAP_EVT_PAIR_TICK);
        PRINT("[PAIR] USB Dongle searching on channel 0\n");
#else
        wireless_state = WDAP_STATE_PAIR_LISTEN;
        tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
        PRINT("[PAIR] DAPLink listening on channel 0\n");
#endif
    }
}

void WirelessDAP_SetWiredActive(bool active)
{
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    if(active && !wired_active && request_ready)
    {
        request_ready = 0U;
        wireless_state = WDAP_STATE_PAIRED_RX;
        tmos_set_event(wireless_task_id, WDAP_EVT_START_RX);
    }
    wired_active = active;
#else
    (void)active;
#endif
}

bool WirelessDAP_IsPaired(void)
{
    return FlashSave_IsPaired();
}

bool WirelessDAP_SubmitRequest(const uint8_t *data, uint8_t length)
{
#if FIRMWARE_ROLE == FIRMWARE_ROLE_USB_DONGLE
    if(!FlashSave_IsPaired() || (wireless_state != WDAP_STATE_IDLE) ||
       response_ready || (data == NULL) || (length == 0U) ||
       (length > WIRELESS_DAP_PACKET_SIZE))
    {
        return false;
    }
    dap_sequence++;
    if(dap_sequence == 0U)
    {
        dap_sequence = 1U;
    }
    WirelessBuildPacket(&tx_packet, WDAP_PACKET_DAP_REQUEST, dap_sequence,
                        g_flash_save_config.pair_id, 0, data, length);
    retry_count = 0U;
    wireless_state = WDAP_STATE_WAIT_DAP_RESPONSE;
    tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
    tmos_start_task(wireless_task_id, WDAP_EVT_TIMEOUT,
                    MS1_TO_SYSTEM_TIME(WDAP_DAP_TIMEOUT_MS));
    return true;
#else
    (void)data;
    (void)length;
    return false;
#endif
}

bool WirelessDAP_TakeResponse(uint8_t *data, uint8_t *length)
{
#if FIRMWARE_ROLE == FIRMWARE_ROLE_USB_DONGLE
    uint32_t irq_status;
    if(!response_ready || (data == NULL) || (length == NULL))
    {
        return false;
    }
    SYS_DisableAllIrq(&irq_status);
    memcpy(data, response_data, response_length);
    *length = response_length;
    response_ready = 0U;
    SYS_RecoverIrq(irq_status);
    return true;
#else
    (void)data;
    (void)length;
    return false;
#endif
}

bool WirelessDAP_TakeRequest(uint8_t *data, uint8_t *length)
{
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    uint32_t irq_status;
    if(!request_ready || (data == NULL) || (length == NULL))
    {
        return false;
    }
    SYS_DisableAllIrq(&irq_status);
    memcpy(data, request_data, request_length);
    *length = request_length;
    request_ready = 0U;
    SYS_RecoverIrq(irq_status);
    return true;
#else
    (void)data;
    (void)length;
    return false;
#endif
}

bool WirelessDAP_SubmitResponse(const uint8_t *data, uint8_t length)
{
#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
    if(!FlashSave_IsPaired() || (data == NULL) || (length == 0U) ||
       (length > WIRELESS_DAP_PACKET_SIZE))
    {
        return false;
    }
    memcpy(last_response, data, length);
    last_response_length = length;
    WirelessBuildPacket(&tx_packet, WDAP_PACKET_DAP_RESPONSE,
                        last_request_sequence, g_flash_save_config.pair_id,
                        0, data, length);
    wireless_state = WDAP_STATE_DAP_RESPONSE_TX;
    tmos_set_event(wireless_task_id, WDAP_EVT_SEND);
    return true;
#else
    (void)data;
    (void)length;
    return false;
#endif
}
