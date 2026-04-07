#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "TypeDef.h"
#include "NowMode.h"
#include "F1CUart.h"

#define TAG "MAIN"
uint8_t *selfImgPtr = NULL;
uint32_t selfImgSize = 0;
uint32_t otherImgSize = 0;
uint8_t *otherImgPtr = NULL;
void check_psram(void)
{
    if (esp_psram_is_initialized())
    {
        ESP_LOGI(TAG, "PSRAM initialized");

        size_t size = esp_psram_get_size();
        ESP_LOGI(TAG, "PSRAM size: %d bytes", size);

        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "Free PSRAM: %d bytes", free_psram);
    }
    else
    {
        ESP_LOGE(TAG, "PSRAM NOT initialized");
    }
    return;
}
void app_main(void)
{

    esp_task_wdt_deinit();
    check_psram();
    StartUart();
    StartEspNow();
}
