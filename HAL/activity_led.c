#include "CH59x_common.h"
#include "activity_led.h"
#include "board.h"

#define ACTIVITY_LED_TICKS     (60000000U / 20U) /* 50 ms at 60 MHz */

#if ACTIVITY_LED_ACTIVE_HIGH
#define ACTIVITY_LED_ON()      BOARD_GPIO_SET(ACTIVITY_LED_PORT, ACTIVITY_LED_PIN)
#define ACTIVITY_LED_OFF()     BOARD_GPIO_RESET(ACTIVITY_LED_PORT, ACTIVITY_LED_PIN)
#else
#define ACTIVITY_LED_ON()      BOARD_GPIO_RESET(ACTIVITY_LED_PORT, ACTIVITY_LED_PIN)
#define ACTIVITY_LED_OFF()     BOARD_GPIO_SET(ACTIVITY_LED_PORT, ACTIVITY_LED_PIN)
#endif

static volatile uint32_t led_off_time;
static volatile uint8_t led_active;

void activity_led_init(void)
{
    ACTIVITY_LED_OFF();
    BOARD_GPIO_MODE(ACTIVITY_LED_PORT, ACTIVITY_LED_PIN, GPIO_ModeOut_PP_5mA);

    led_active = 0;
}

void activity_led_pulse(void)
{
    ACTIVITY_LED_ON();
    led_off_time = SYS_GetSysTickCnt() + ACTIVITY_LED_TICKS;
    led_active = 1;
}

void activity_led_off(void)
{
    ACTIVITY_LED_OFF();
    led_active = 0;
}

void activity_led_poll(void)
{
    if (led_active && (int32_t)(SYS_GetSysTickCnt() - led_off_time) >= 0) {
        ACTIVITY_LED_OFF();
        led_active = 0;
    }
}
