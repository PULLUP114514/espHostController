/*
 *   串口数据帧格式：
 *   +--------------+--------------+------------------------------------------------------------------------------+-------------+-------------+--------------+--------------+
 *   |    HEAD 0    |    HEAD 1    |                                  (BODY)                                      |    CRC 0    |    CRC 1    |    TAIL 0    |    TAIL 1    |
 *   |     0xAA     |     0x55     |                                  (BODY)                                      |     CRC     |     CRC     |     0x0D     |     0x07     |
 *   +--------------+--------------+------------------------------------------------------------------------------+-------------+-------------+--------------+--------------+
 *                                 +------------------+------------------+-------------------+--------------------+
 *                                 |    Package ID    |  Operation Code  |  Operation size   |   Operation Data   |
 *                                 |      uint32      |      uint8       |      uint 16      |    unsigned char   |
 *                                 +------------------+------------------+-------------------+--------------------+
 *                                                                                 |             512 Bytes (Max)
 *                                 ^                                               |                    ^         ^
 *                                 |                                               +--------------------+         |
 *                                 +-------------------------------CRC Protected----------------------------------+
 * Operation Code 见枚举
 */

#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "NowMode.h"
#include "TypeDef.h"

#define UART_PORT UART_NUM_1
#define TXD_PIN 4
#define RXD_PIN 5
#define BUF_SIZE 1024
#define BODY_HEADER_SIZE (sizeof(bodyPackage.packageID) +     \
                          sizeof(bodyPackage.OperationCode) + \
                          sizeof(bodyPackage.OperationSize))
const char *TAG = "UART";
uint16_t CRC16Check(const uint8_t *data, uint16_t size)
{
        uint16_t crc = 0xFFFF;

        for (uint16_t i = 0; i < size; i++)
        {
                crc ^= data[i];

                for (uint8_t j = 0; j < 8; j++)
                {
                        if (crc & 0x0001)
                                crc = (crc >> 1) ^ 0xA001;
                        else
                                crc >>= 1;
                }
        }

        return crc;
}
void F1CControlUartListener(void *pvParameters)
{
        uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
        BodyDef_t bodyPackage = {0};
        uint32_t bodyCount = 0;              // 指向当前 Body 的字节
        uint16_t bodyOperationDataCount = 0; // 指向当前 OperationData 的字节
        uint16_t bodyOperationDataSize = 0;  // 指向 OperationData 的总字节
        uint16_t crcCode = 0;
        if (data == nullptr)
        {
                ESP_LOGE(TAG, "Malloc Failed");
                return;
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
                for (uint8_t i = 0; i < len; i++)
                {
                        switch (status)
                        {
                        case HEAD0:
                                // clean
                                bodyCount = 0;
                                memset(&bodyPackage, 0, sizeof(BodyDef_t));
                                bodyOperationDataSize = 0;
                                bodyOperationDataCount = 0;
                                if (data[i] == HEAD0_TAG)
                                        status = HEAD1;
                                break;
                        case HEAD1:
                                if (data[i] == HEAD1_TAG)
                                {
                                        status = BODY;
                                        bodyCount = 0;
                                        bodyOperationDataCount = 0;
                                }
                                else if (data[i] == HEAD0_TAG)
                                {
                                        status = HEAD1; // 重同步
                                }
                                else
                                {
                                        status = HEAD0;
                                }
                                break;
                        case BODY:
                                if (bodyCount >= sizeof(bodyPackage))
                                {
                                        status = HEAD0;
                                        break;
                                }
                                ((uint8_t *)&bodyPackage)[bodyCount] = data[i];
                                if (bodyCount == (sizeof(bodyPackage.packageID) + sizeof(bodyPackage.OperationCode) + sizeof(bodyPackage.OperationSize)))
                                {
                                        bodyOperationDataCount = 0;
                                        bodyOperationDataSize = bodyPackage.OperationSize;
                                        if (bodyOperationDataSize > 512)
                                        {
                                                status = HEAD0;
                                        }
                                }
                                if (bodyCount >= BODY_HEADER_SIZE)
                                {
                                        bodyOperationDataCount++;
                                }
                                bodyCount++; // 后++
                                if (bodyOperationDataCount >= bodyOperationDataSize)
                                {
                                        status = CRC0;
                                }
                                break;
                        case CRC0:
                                crcCode = ((uint16_t)data[i] << 8);
                                status = CRC1;
                                break;
                        case CRC1:
                                crcCode |= ((uint16_t)data[i]);
                                // Check CRC
                                if (CRC16Check(((uint8_t *)&bodyPackage), BODY_HEADER_SIZE + bodyOperationDataSize) == crcCode)
                                {
                                        status = TAIL0;
                                }
                                else
                                {
                                        status = HEAD0;
                                }

                                break;
                        case TAIL0:
                                if (data[i] == TAIL0_TAG)
                                {
                                        status = TAIL1;
                                }
                                else
                                {
                                        status = HEAD0;
                                }
                                break;
                        case TAIL1:
                                if (data[i] == TAIL1_TAG)
                                {
                                        status = SUCCESS;
                                }
                                else
                                {
                                        status = HEAD0;
                                }
                                break;
                        case SUCCESS:
                                status = HEAD0;
                                break;
                        }
                        if (status == SUCCESS)
                        {
                                break;
                        }
                }

                if (status != SUCCESS)
                {
                        continue;
                }
                // TODO
        }
        return;
}

void UartMessageProcesser(BodyDef_t *bodyMessage)
{
        if (bodyMessage == NULL)
        {
                return;
        }
        switch (bodyMessage->OperationCode)
        {
        case IMG_DATAHEAD:

                break;
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
        xTaskCreate(F1CControlUartListener, "F1CControlUartListener", 2048, NULL, 4, NULL);
        return;
}
