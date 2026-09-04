#include "flash_save.h"

#include <stddef.h>
#include <string.h>

/* BLE SNV 默认从 DataFlash 0x7000 开始，本模块使用首个 256 字节页。 */
#define FLASH_SAVE_ADDR       0x0000U
#define FLASH_SAVE_AREA_SIZE  EEPROM_MIN_ER_SIZE
#define FLASH_SAVE_MAGIC      0x57444150UL /* "WDAP" */
#define FLASH_SAVE_VERSION    1U

flash_save_config_t g_flash_save_config;

static uint32_t FlashSave_Crc32(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;
    size_t i;

    while(length--)
    {
        crc ^= *bytes++;
        for(i = 0; i < 8U; i++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

static void FlashSave_LoadDefaults(void)
{
    memset(&g_flash_save_config, 0, sizeof(g_flash_save_config));
    g_flash_save_config.channel = FLASH_SAVE_PAIR_CHANNEL;
}

static bool FlashSave_DataIsValid(const flash_save_config_t *config)
{
    if((config->magic != FLASH_SAVE_MAGIC) ||
       (config->version != FLASH_SAVE_VERSION) ||
       (config->paired != 1U) ||
       (config->channel < FLASH_SAVE_MIN_WORK_CHANNEL) ||
       (config->channel > FLASH_SAVE_MAX_WORK_CHANNEL) ||
       (config->pair_id == 0U))
    {
        return false;
    }

    return FlashSave_Crc32(config, offsetof(flash_save_config_t, crc32)) ==
           config->crc32;
}

flash_save_status_t FlashSave_Init(void)
{
    if(EEPROM_READ(FLASH_SAVE_ADDR, &g_flash_save_config,
                   sizeof(g_flash_save_config)) != 0U)
    {
        FlashSave_LoadDefaults();
        return FLASH_SAVE_STATUS_READ_FAILED;
    }

    if(!FlashSave_DataIsValid(&g_flash_save_config))
    {
        FlashSave_LoadDefaults();
        return FLASH_SAVE_STATUS_DEFAULTS_LOADED;
    }
    return FLASH_SAVE_STATUS_OK;
}

flash_save_status_t FlashSave_Save(void)
{
    flash_save_config_t save_config;
    flash_save_config_t verify_config;

    if((g_flash_save_config.paired != 1U) ||
       (g_flash_save_config.channel < FLASH_SAVE_MIN_WORK_CHANNEL) ||
       (g_flash_save_config.channel > FLASH_SAVE_MAX_WORK_CHANNEL) ||
       (g_flash_save_config.pair_id == 0U))
    {
        return FLASH_SAVE_STATUS_INVALID_DATA;
    }
    if(sizeof(g_flash_save_config) > FLASH_SAVE_AREA_SIZE)
    {
        return FLASH_SAVE_STATUS_OUT_OF_RANGE;
    }

    save_config = g_flash_save_config;
    save_config.magic = FLASH_SAVE_MAGIC;
    save_config.version = FLASH_SAVE_VERSION;
    save_config.crc32 = FlashSave_Crc32(&save_config,
                                       offsetof(flash_save_config_t, crc32));

    if(EEPROM_ERASE(FLASH_SAVE_ADDR, FLASH_SAVE_AREA_SIZE) != 0U)
    {
        return FLASH_SAVE_STATUS_ERASE_FAILED;
    }
    if(EEPROM_WRITE(FLASH_SAVE_ADDR, &save_config, sizeof(save_config)) != 0U)
    {
        return FLASH_SAVE_STATUS_WRITE_FAILED;
    }
    if(EEPROM_READ(FLASH_SAVE_ADDR, &verify_config, sizeof(verify_config)) != 0U)
    {
        return FLASH_SAVE_STATUS_READ_FAILED;
    }
    if((memcmp(&verify_config, &save_config, sizeof(save_config)) != 0) ||
       !FlashSave_DataIsValid(&verify_config))
    {
        return FLASH_SAVE_STATUS_VERIFY_FAILED;
    }

    g_flash_save_config = verify_config;
    return FLASH_SAVE_STATUS_OK;
}

flash_save_status_t FlashSave_Reset(void)
{
    if(EEPROM_ERASE(FLASH_SAVE_ADDR, FLASH_SAVE_AREA_SIZE) != 0U)
    {
        return FLASH_SAVE_STATUS_ERASE_FAILED;
    }
    FlashSave_LoadDefaults();
    return FLASH_SAVE_STATUS_OK;
}

bool FlashSave_IsPaired(void)
{
    return FlashSave_DataIsValid(&g_flash_save_config);
}
