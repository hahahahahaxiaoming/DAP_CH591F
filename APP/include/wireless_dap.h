#ifndef WIRELESS_DAP_H
#define WIRELESS_DAP_H

#include <stdbool.h>
#include <stdint.h>
#include "firmware_config.h"

#define WIRELESS_DAP_PACKET_SIZE 64U

void WirelessDAP_Init(void);
void WirelessDAP_SetWiredActive(bool active);
bool WirelessDAP_IsPaired(void);

/* USB Dongle 端：USB 请求送入无线，取回无线响应。 */
bool WirelessDAP_SubmitRequest(const uint8_t *data, uint8_t length);
bool WirelessDAP_TakeResponse(uint8_t *data, uint8_t *length);

/* DAPLink 端：取出无线请求，执行后送回响应。 */
bool WirelessDAP_TakeRequest(uint8_t *data, uint8_t *length);
bool WirelessDAP_SubmitResponse(const uint8_t *data, uint8_t length);

/* 双向无线串口：Dongle 提交的数据由 DAPLink 取出，反向亦然。 */
bool WirelessDAP_SubmitCdcData(const uint8_t *data, uint8_t length);
bool WirelessDAP_TakeCdcData(uint8_t *data, uint8_t *length);

/* Dongle 把 CDC line coding（7 字节）同步给 DAPLink。 */
bool WirelessDAP_SubmitCdcConfig(const uint8_t *data, uint8_t length);
bool WirelessDAP_TakeCdcConfig(uint8_t *data, uint8_t *length);

#endif
