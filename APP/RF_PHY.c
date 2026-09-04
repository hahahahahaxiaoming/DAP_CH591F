#include "CONFIG.h"
#include "RF_PHY.h"

#define RF_PHY_ACCESS_ADDRESS    0x71764129UL
#define RF_PHY_CRC_INIT          0x555555UL

static rf_phy_callback_t rf_phy_callback;
static uint8_t rf_phy_channel;

/* CH59x BLE 库在射频中断中调用本函数。 */
static void RF_PHY_StatusCallback(uint8_t status, uint8_t receive_status,
                                  uint8_t *receive_buffer)
{
    if(rf_phy_callback == 0)
    {
        return;
    }

    switch(status)
    {
        case TX_MODE_TX_FINISH:
            rf_phy_callback(RF_PHY_EVENT_TX_DONE, 0, 0, 0);
            break;

        case TX_MODE_TX_FAIL:
            rf_phy_callback(RF_PHY_EVENT_TX_FAILED, 0, 0, 0);
            break;

        case RX_MODE_RX_DATA:
            /* BASIC RX receives one packet and then enters idle, including
             * packets rejected by CRC. Always notify the upper layer so it
             * can restart RX; only expose payload when CRC is correct. */
            if((receive_status == 0U) && (receive_buffer != 0))
            {
                rf_phy_callback(RF_PHY_EVENT_RX_DATA,
                                (int8_t)receive_buffer[0],
                                &receive_buffer[2],
                                receive_buffer[1]);
            }
            else
            {
                rf_phy_callback(RF_PHY_EVENT_RX_FAILED,
                                (int8_t)receive_status, 0, 0);
            }
            break;

        default:
            break;
    }
}

static uint8_t RF_PHY_Configure(uint8_t channel)
{
    rfConfig_t config;

    if(channel > RF_PHY_MAX_CHANNEL)
    {
        return INVALIDPARAMETER;
    }

    tmos_memset(&config, 0, sizeof(config));
    config.LLEMode = LLE_MODE_BASIC | LLE_MODE_PHY_2M;
    config.Channel = channel;
    config.accessAddress = RF_PHY_ACCESS_ADDRESS;
    config.CRCInit = RF_PHY_CRC_INIT;
    config.rfStatusCB = RF_PHY_StatusCallback;
    config.RxMaxlen = RF_PHY_MAX_PAYLOAD;
    config.TxMaxlen = RF_PHY_MAX_PAYLOAD;

    RF_Shut();
    if(RF_Config(&config) != SUCCESS)
    {
        return FAILURE;
    }

    rf_phy_channel = channel;
    return SUCCESS;
}

uint8_t RF_PHY_Init(uint8_t channel, rf_phy_callback_t callback)
{
    rf_phy_callback = callback;
    rf_phy_channel = RF_PHY_PAIR_CHANNEL;
    return RF_PHY_Configure(channel);
}

uint8_t RF_PHY_SetChannel(uint8_t channel)
{
    if(channel == rf_phy_channel)
    {
        RF_Shut();
        return SUCCESS;
    }
    return RF_PHY_Configure(channel);
}

uint8_t RF_PHY_Send(const void *data, uint8_t length)
{
    if((data == 0) || (length == 0U) || (length > RF_PHY_MAX_PAYLOAD))
    {
        return INVALIDPARAMETER;
    }

    RF_Shut();
    return RF_Tx((uint8_t *)data, length, 0xFF, 0xFF);
}

uint8_t RF_PHY_StartReceive(void)
{
    RF_Shut();
    return RF_Rx(0, 0, 0xFF, 0xFF);
}

void RF_PHY_Stop(void)
{
    RF_Shut();
}

uint8_t RF_PHY_GetChannel(void)
{
    return rf_phy_channel;
}
