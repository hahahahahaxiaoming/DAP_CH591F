#ifndef __UART_H__
#define __UART_H__

#include "chry_ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- 宏控制开关 ---
#define USE_UART0   1  // 为1时启用UART0
#define USE_UART1   1  // 为1时启用UART1,LOG
#define USE_UART2   1  // 为1时启用UART2

// 定义各个串口缓冲区大小（单位：字节）
// 建议：尽量使用 2 的 N 次方 (如 64, 128, 256, 512)
#define UART0_RX_BUF_SIZE   1024
#define UART1_RX_BUF_SIZE   128
#define UART2_RX_BUF_SIZE   1024

#if USE_UART0
extern chry_ringbuffer_t cb_uart0;
void Uart0_Init(void);
#endif

#if USE_UART1
extern chry_ringbuffer_t cb_uart1;
void Uart1_Init(void);
#endif

#if USE_UART2
extern chry_ringbuffer_t cb_uart2;
void Uart2_Init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __UART_H__ */
