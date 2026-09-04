#ifndef RF_PHY_H
#define RF_PHY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RF_PHY_PAIR_CHANNEL       0U
#define RF_PHY_MIN_CHANNEL        0U
#define RF_PHY_MAX_CHANNEL        39U
#define RF_PHY_MAX_PAYLOAD        251U

typedef enum
{
    RF_PHY_EVENT_TX_DONE = 0,
    RF_PHY_EVENT_TX_FAILED,
    RF_PHY_EVENT_RX_DATA
} rf_phy_event_t;

/* 回调运行在射频中断中：只能复制数据或投递 TMOS 事件。 */
typedef void (*rf_phy_callback_t)(rf_phy_event_t event,
                                  int8_t rssi,
                                  const uint8_t *data,
                                  uint8_t length);

uint8_t RF_PHY_Init(uint8_t channel, rf_phy_callback_t callback);
uint8_t RF_PHY_SetChannel(uint8_t channel);
uint8_t RF_PHY_Send(const void *data, uint8_t length);
uint8_t RF_PHY_StartReceive(void);
void RF_PHY_Stop(void);
uint8_t RF_PHY_GetChannel(void);

#ifdef __cplusplus
}
#endif

#endif
