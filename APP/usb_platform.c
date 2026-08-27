#include "CH59x_common.h"
void usb_dc_low_level_init(void) { PFIC_EnableIRQ(USB_IRQn); }
void usb_dc_low_level_deinit(void) { PFIC_DisableIRQ(USB_IRQn); }
