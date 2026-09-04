#ifndef CH592_DAP_MAIN_H
#define CH592_DAP_MAIN_H

#define DAP_IN_EP       0x81
#define DAP_OUT_EP      0x01
#define CDC0_INT_EP     0x82
#define CDC0_IN_EP      0x83
#define CDC0_OUT_EP     0x03
#define CDC1_IN_EP      0x86
#define CDC1_OUT_EP     0x07
#define CDC1_INT_EP     0x87

void ch592_dap_init(void);
void ch592_dap_poll(void);

#endif
