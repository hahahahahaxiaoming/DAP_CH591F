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

#endif
