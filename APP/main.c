#include "dap_main.h"
#include "CH59x_common.h"
#include "main.h"
#include "uart.h"
#include "activity_led.h"
#include "hal_time.h"

/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */
int main(void)
{
#if(defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif
    HSECFG_Capacitance(HSECap_20p);
    SetSysClock(CLK_SOURCE_PLL_60MHz);
	HAL_TimeInit();
#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif
#ifdef DEBUG
    Uart1_Init();
#endif
	Uart0_Init();
	activity_led_init();
	ch592_dap_init();
	DBG("\n************************************\n");
	DBG("[data:%s][time:%s]\n", __DATE__, __TIME__);
	DBG("************************************\n");
    while(1)
    {
        ch592_dap_poll();
		activity_led_poll();
    }
}
