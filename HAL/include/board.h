#ifndef BOARD_H
#define BOARD_H

#include "CH59x_common.h"

/*
 * 本固件的固定功能引脚及串口使用情况：
 *
 *   USB D-  ：PB10（已使用，由 CH592 硬件固定）
 *   USB D+  ：PB11（已使用，由 CH592 硬件固定）
 *
 *   UART0 RX：PB4 （已由 USB CDC0 使用）
 *   UART0 TX：PB7 （已由 USB CDC0 使用）
 *   UART1 RX：PA8 （仅定义 DEBUG 时使用）
 *   UART1 TX：PA9 （仅定义 DEBUG 时使用，作为固件日志口）
 *   UART2 RX：PB22（USE_UART2=0，已禁用）
 *   UART2 TX：PB23（USE_UART2=0，已禁用）
 *   UART3 RX：PA4 （本固件未编译、未使用）
 *   UART3 TX：PA5 （本固件未编译、未使用）
 *
 * UART0 始终初始化；UART1 仅在 DEBUG 构建中初始化；UART2 已通过
 * USE_UART2 关闭；UART3 未使用。UART2 和 UART3 运行时不占用相应引脚。
 * 修改下方引脚配置时，请避开上述“已使用”的引脚。
 */

/* 用户可配置的 GPIO：端口宏必须填写 A 或 B，不要填写 GPIOA 或 GPIOB。 */
#define DAP_NRESET_PORT         A
#define DAP_NRESET_PIN          GPIO_Pin_11

#define DAP_SWCLK_PORT          A
#define DAP_SWCLK_PIN           GPIO_Pin_12

#define DAP_SWDIO_PORT          A
#define DAP_SWDIO_PIN           GPIO_Pin_13

#define DAP_TDI_PORT            A
#define DAP_TDI_PIN             GPIO_Pin_14

#define DAP_TDO_PORT            A
#define DAP_TDO_PIN             GPIO_Pin_15

#define ACTIVITY_LED_PORT       B
#define ACTIVITY_LED_PIN        GPIO_Pin_23
#define ACTIVITY_LED_ACTIVE_HIGH 1  /* 1：高电平点亮；0：低电平点亮 */

/* GPIO 辅助宏：增加一层宏展开，使端口宏能先展开为 A 或 B。 */
#define BOARD_GPIO_REG_IMPL(port, reg)       R32_P##port##_##reg
#define BOARD_GPIO_REG(port, reg)            BOARD_GPIO_REG_IMPL(port, reg)
#define BOARD_GPIO_SET_IMPL(port, pin)        GPIO##port##_SetBits(pin)
#define BOARD_GPIO_SET(port, pin)             BOARD_GPIO_SET_IMPL(port, pin)
#define BOARD_GPIO_RESET_IMPL(port, pin)      GPIO##port##_ResetBits(pin)
#define BOARD_GPIO_RESET(port, pin)           BOARD_GPIO_RESET_IMPL(port, pin)
#define BOARD_GPIO_MODE_IMPL(port, pin, mode) GPIO##port##_ModeCfg(pin, mode)
#define BOARD_GPIO_MODE(port, pin, mode)      BOARD_GPIO_MODE_IMPL(port, pin, mode)

#endif
