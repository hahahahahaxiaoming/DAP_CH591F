#include "CH59x_common.h"
#include "hal_time.h"

void HAL_TimeInit(void)
{
    /* SysTick_Config also enables its IRQ. This project only needs a free-running
     * counter, so disable the interrupt immediately after starting the timer. */
    (void)SysTick_Config(SysTick_LOAD_RELOAD_Msk);
    SysTick->CTLR &= ~(SysTick_CTLR_STIE | SysTick_CTLR_SWIE);
    PFIC_DisableIRQ(SysTick_IRQn);
}

uint32_t HAL_TimeNow(void)
{
    return SYS_GetSysTickCnt();
}
