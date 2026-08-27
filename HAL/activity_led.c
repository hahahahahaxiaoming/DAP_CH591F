#include "CH59x_common.h"
#include "activity_led.h"
#include "hal_time.h"

#define ACTIVITY_LED_PIN       GPIO_Pin_12
#define ACTIVITY_LED_TICKS     (60000000U / 20U) /* 50 ms at 60 MHz */
#define ACTIVITY_LED_ACTIVE_LOW 1

#if ACTIVITY_LED_ACTIVE_LOW
#define ACTIVITY_LED_ON()      GPIOB_ResetBits(ACTIVITY_LED_PIN)
#define ACTIVITY_LED_OFF()     GPIOB_SetBits(ACTIVITY_LED_PIN)
#else
#define ACTIVITY_LED_ON()      GPIOB_SetBits(ACTIVITY_LED_PIN)
#define ACTIVITY_LED_OFF()     GPIOB_ResetBits(ACTIVITY_LED_PIN)
#endif

static volatile uint32_t led_off_time;
static volatile uint8_t led_active;

void activity_led_init(void)
{
    ACTIVITY_LED_OFF();
    GPIOB_ModeCfg(ACTIVITY_LED_PIN, GPIO_ModeOut_PP_5mA);

    led_active = 0;
}

void activity_led_pulse(void)
{
    ACTIVITY_LED_ON();
    led_off_time = HAL_TimeNow() + ACTIVITY_LED_TICKS;
    led_active = 1;
}

void activity_led_poll(void)
{
    if (led_active && (int32_t)(HAL_TimeNow() - led_off_time) >= 0) {
        ACTIVITY_LED_OFF();
        led_active = 0;
    }
}
