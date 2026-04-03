/*
 *   串口数据包格式：
 *   +-------------+-------------+------------------------------------------------------------------------------+-------------+-------------+
 *   |    HEAD0    |    HEAD1    |                                  (BODY)                                      |    TAIL0    |    TAIL0    |
 *   |    0xAA     |    0x55     |                                  (BODY)                                      |    0x0D     |    0x07     |
 *   +-------------+-------------+------------------------------------------------------------------------------+-------------+-------------+
 *                               +------------------+------------------+-------------------+--------------------+
 *                               |    Package ID    |  Operation Code  |  Operation size   |   Operation Data   |
 *                               |      uint32      |      uint8       |      uint 16      |    unsigned char   |
 *                               +------------------+------------------+-------------------+--------------------+
 *                                                                                             128 Bytes (Max)
 */

#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "NowMode.h"

#define UART_PORT UART_NUM_1
#define TXD_PIN 4
#define RXD_PIN 5
#define BUF_SIZE 1024

const char *TAG = "UART";

// 状态机读取状态
typedef enum
{
        HEAD0,
        HEAD1,
        BODY,
        TAIL0,
        TAIL1
} FCMReadStatus;
typedef enum
{
        HEAD0_TAG = 0xAA,
        HEAD1_TAG = 0x55,
        TAIL0_TAG = 0x0D,
        TAIL1_TAG = 0x07
} FCMTag;

#pragma pack(push, 4) // 紧凑 1字节对齐

typedef struct
{
        uint32_t packageID;
        uint8_t OperationCode;
        uint16_t OperationSize;
        uint8_t OperationData;
} BodyDef_t;

#pragma pack(pop) // 恢复默认

void F1CControlUartListen(void *pvParameters)
{
        uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
        uint8_t *bodyBuffer = (uint8_t *)malloc();
        if (data == nullptr)
        {
                ESP_LOGE(TAG, "Malloc Failed");
        }
        FCMReadStatus status = HEAD0;
        while (1)
        {
                int len = uart_read_bytes(UART_PORT, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
                if (len < 1)
                {
                        // delay 100ms
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        continue;
                }

                // FCM
                for (uint8_t i = 0; i < BUF_SIZE; i++)
                {
                        switch (status)
                        {
                        case HEAD0:
                                if (data[i] == HEAD0_TAG)
                                        status = HEAD1;
                                break;
                        case HEAD1:
                                if (data[i] == HEAD1_TAG)
                                        status = BODY;
                                break;
                        case BODY:

                                break;
                        }
                }
        }
        return;
}

void StartUart(void)
{
        const int uart_buffer_size = (1024 * 2);
        QueueHandle_t uart_queue;
        ESP_ERROR_CHECK(uart_driver_install(UART_PORT, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));

        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
            .rx_flow_ctrl_thresh = 122,
        };
        uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        // Configure UART parameters
        ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
        xTaskCreate(F1CControlUartListen, "F1CControlUartListen", 2048, NULL, 4, NULL);
        return;
}
