#ifndef FLASH_SAVE_H
#define FLASH_SAVE_H

#include "CH59x_common.h"
#include <stdbool.h>

#define FLASH_SAVE_PAIR_CHANNEL       0U
#define FLASH_SAVE_MIN_WORK_CHANNEL   1U
#define FLASH_SAVE_MAX_WORK_CHANNEL   39U

typedef enum
{
    FLASH_SAVE_STATUS_OK = 0,
    FLASH_SAVE_STATUS_DEFAULTS_LOADED,
    FLASH_SAVE_STATUS_READ_FAILED,
    FLASH_SAVE_STATUS_INVALID_DATA,
    FLASH_SAVE_STATUS_ERASE_FAILED,
    FLASH_SAVE_STATUS_WRITE_FAILED,
    FLASH_SAVE_STATUS_VERIFY_FAILED,
    FLASH_SAVE_STATUS_OUT_OF_RANGE
} flash_save_status_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint8_t  paired;
    uint8_t  channel;
    uint64_t pair_id;
    uint32_t crc32;
} __attribute__((aligned(4))) flash_save_config_t;

extern flash_save_config_t g_flash_save_config;

flash_save_status_t FlashSave_Init(void);
flash_save_status_t FlashSave_Save(void);
flash_save_status_t FlashSave_Reset(void);
bool FlashSave_IsPaired(void);

#endif
