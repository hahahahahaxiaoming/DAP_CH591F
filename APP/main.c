#include "CONFIG.h"
#include "HAL.h"
#include "dap_main.h"
#include "wireless_dap.h"
#include "main.h"
#include "uart.h"
#include "activity_led.h"

/*********************************************************************
 * GLOBAL TYPEDEFS
 */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

/*********************************************************************
 * @fn      Main_Circulation
 *
 * @brief   主循环
 *
 * @return  none
 */
__HIGH_CODE
__attribute__((noinline))
void Main_Circulation()
{
    while(1)
    {
        ch592_dap_poll();
		activity_led_poll();
        TMOS_SystemProcess();
    }
}


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
#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif
#ifdef DEBUG
    Uart1_Init();
#endif
	DBG("\n************************************\n");
	DBG("[data:%s][time:%s]\n", __DATE__, __TIME__);
    DBG("%s\n", VER_LIB);
	DBG("************************************\n");

    CH59x_BLEInit();
    HAL_Init();
    RF_RoleInit();
	activity_led_init();

#if FIRMWARE_ROLE == FIRMWARE_ROLE_DAPLINK
	Uart0_Init();
    DBG("[BOOT] role=DAPLink\n");
#else
    DBG("[BOOT] role=USB Dongle\n");
#endif

	ch592_dap_init();
    WirelessDAP_Init();
    
    Main_Circulation();
}
