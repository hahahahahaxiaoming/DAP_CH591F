#ifndef CH592_USB_CONFIG_H
#define CH592_USB_CONFIG_H
#include <stdint.h>
#include <stdio.h>
#define CONFIG_USBDEV_ADVANCE_DESC
#define CONFIG_USBDEV_EP_NUM 8
#define CONFIG_USBDEV_MAX_BUS 1
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 256
#define CONFIG_USB_ALIGN_SIZE 4
#define CONFIG_USB_PRINTF(...) ((void)0)
#define CONFIG_USB_DBG_LEVEL (-1)
#define USB_NOCACHE_RAM_SECTION
#define __ALIGN_BEGIN __attribute__((aligned(4)))
#define __ALIGN_END
#endif
