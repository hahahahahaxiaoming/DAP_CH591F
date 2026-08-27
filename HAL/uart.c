#include "CH59x_common.h"
#include "uart.h"
#include "CH59x_uart.h"

/* ===================================================================== */
/* UART0                                  */
/* ===================================================================== */
#if USE_UART0

static uint8_t uart0_buf[UART0_RX_BUF_SIZE];
chry_ringbuffer_t cb_uart0;

// UART0 初始化 (PB7/PB4)
void Uart0_Init(void)
{
    GPIOPinRemap(DISABLE, RB_PIN_UART0);
    GPIOB_SetBits(bTXD0);
    GPIOB_ModeCfg(bTXD0, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(bRXD0, GPIO_ModeIN_PU); 
    UART0_DefInit();

    UART0_ByteTrigCfg(UART_1BYTE_TRIG);
    UART0_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART0_IRQn);

    chry_ringbuffer_init(&cb_uart0, uart0_buf, UART0_RX_BUF_SIZE);
}

// UART0 中断处理
__INTERRUPT __HIGH_CODE void UART0_IRQHandler(void)
{
    uint8_t i;
    uint8_t tmp_buf[8]; 
    
    switch(UART0_GetITFlag())
    {
        case UART_II_LINE_STAT:
            UART0_GetLinSTA();
            break;
            
        case UART_II_RECV_RDY:
            i = 0;
            while(R8_UART0_RFC && i < sizeof(tmp_buf)) {
                tmp_buf[i++] = UART0_RecvByte();
            }
            if(i) chry_ringbuffer_write(&cb_uart0, tmp_buf, i);
            break;
            
        case UART_II_RECV_TOUT:
            while(R8_UART0_RFC) 
            {
                i = 0;
                while(R8_UART0_RFC && i < sizeof(tmp_buf)) { 
                    tmp_buf[i++] = UART0_RecvByte();
                }
                if (i > 0) {
                    chry_ringbuffer_write(&cb_uart0, tmp_buf, i);
                }
            }
            break;
            
        default: break;
    }
}
#endif

/* ===================================================================== */
/* UART1                                  */
/* ===================================================================== */
#if USE_UART1

static uint8_t uart1_buf[UART1_RX_BUF_SIZE];
chry_ringbuffer_t cb_uart1;

// UART1 初始化 (PA9/PA8)
void Uart1_Init(void)
{
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    GPIOA_ModeCfg(bRXD1, GPIO_ModeIN_PU);
    UART1_DefInit();

    UART1_ByteTrigCfg(UART_1BYTE_TRIG);
    UART1_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART1_IRQn);
    
    chry_ringbuffer_init(&cb_uart1, uart1_buf, UART1_RX_BUF_SIZE);
}

// UART1 中断处理
__INTERRUPT __HIGH_CODE void UART1_IRQHandler(void)
{
    uint8_t i;
    uint8_t tmp_buf[8];

    switch(UART1_GetITFlag())
    {
        case UART_II_LINE_STAT:
            UART1_GetLinSTA();
            break;
        case UART_II_RECV_RDY:
            i = 0;
            while(R8_UART1_RFC && i < sizeof(tmp_buf)) {
                tmp_buf[i++] = UART1_RecvByte();
            }
            if(i) chry_ringbuffer_write(&cb_uart1, tmp_buf, i);
            break;
        case UART_II_RECV_TOUT:
            while(R8_UART1_RFC) 
            {
                i = 0;
                // 【修复Bug】：这里原来复制成了 R8_UART0_RFC，现已修正为 R8_UART1_RFC
                while(R8_UART1_RFC && i < sizeof(tmp_buf)) { 
                    tmp_buf[i++] = UART1_RecvByte();
                }
                if (i > 0) {
                    chry_ringbuffer_write(&cb_uart1, tmp_buf, i);
                }
            }
            break;
        default: break;
    }
}
#endif

/* ===================================================================== */
/* UART2                                  */
/* ===================================================================== */
#if USE_UART2

static uint8_t uart2_buf[UART2_RX_BUF_SIZE];
chry_ringbuffer_t cb_uart2;

// UART2 初始化 (通常默认为 PB23/PB22)
void Uart2_Init(void)
{
    GPIOPinRemap(ENABLE, RB_PIN_UART2);
    GPIOB_SetBits(bTXD2);
    GPIOB_ModeCfg(bTXD2, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(bRXD2, GPIO_ModeIN_PU);
    UART2_DefInit();

    UART2_ByteTrigCfg(UART_1BYTE_TRIG);
    UART2_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART2_IRQn);
    
    chry_ringbuffer_init(&cb_uart2, uart2_buf, UART2_RX_BUF_SIZE);
}

// UART2 中断处理
__INTERRUPT __HIGH_CODE void UART2_IRQHandler(void)
{
    uint8_t i;
    uint8_t tmp_buf[8];

    switch(UART2_GetITFlag())
    {
        case UART_II_LINE_STAT:
            UART2_GetLinSTA();
            break;
        case UART_II_RECV_RDY:
            i = 0;
            while(R8_UART2_RFC && i < sizeof(tmp_buf)) {
                tmp_buf[i++] = UART2_RecvByte();
            }
            if(i) chry_ringbuffer_write(&cb_uart2, tmp_buf, i);
            break;
        case UART_II_RECV_TOUT:
            while(R8_UART2_RFC) 
            {
                i = 0;
                while(R8_UART2_RFC && i < sizeof(tmp_buf)) { 
                    tmp_buf[i++] = UART2_RecvByte();
                }
                if (i > 0) {
                    chry_ringbuffer_write(&cb_uart2, tmp_buf, i);
                }
            }
            break;
        default: break;
    }
}
#endif

