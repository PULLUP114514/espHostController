/*
 *   串口数据帧格式：
 *   +--------------+--------------+------------------------------------------------------------------------------+-------------+-------------+--------------+--------------+
 *   |    HEAD 0    |    HEAD 1    |                                  (BODY)                                      |    CRC 0    |    CRC 1    |    TAIL 0    |    TAIL 1    |
 *   |     0xAA     |     0x55     |                                  (BODY)                                      |     CRC     |     CRC     |     0x0D     |     0x07     |
 *   +--------------+--------------+------------------------------------------------------------------------------+-------------+-------------+--------------+--------------+
 *                                 +------------------+------------------+-------------------+--------------------+
 *                                 |    Package ID    |  Operation Code  |  Operation size   |   Operation Data   |
 *                                 |      uint 32     |      uint 8      |      uint 16      |    unsigned char   |
 *                                 +------------------+------------------+-------------------+--------------------+
 *                                     Not yet used                                |             512 Bytes (Max)
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
#include "freertos/portmacro.h"

#define UART_PORT UART_NUM_1
#define TXD_PIN 4
#define RXD_PIN 5
#define BUF_SIZE 1024
#define BODY_HEADER_SIZE (sizeof(bodyPackage.packageID) +     \
                          sizeof(bodyPackage.operationCode) + \
                          sizeof(bodyPackage.operationSize))
const char *TAG = "UART";
int8_t UartMessageProcesser(BodyDef_t *bodyMessage);
void UartSender(const uint8_t operationCode, const uint16_t operationSize, const void *operationData);

void printHex(const uint8_t *data, uint32_t size)
{
        if (data == NULL || size == 0)
                return;

        for (uint32_t i = 0; i < size; i++)
        {
                printf("%02X ", data[i]);
                if ((i + 1) % 16 == 0)
                {
                        printf("\n");
                }
        }
        if (size % 16 != 0)
        {
                printf("\n");
        }
}
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
        uint16_t bodyOperationDataCount = 0; // 指向当前 operationData 的字节
        uint16_t crcCode = 0;
        if (data == nullptr)
        {
                ESP_LOGE(TAG, "Malloc Failed. At pos: %s %d", __FILENAME__, __LINE__);
                return;
        }
        FCMReadStatus status = HEAD0;
        uint8_t i = 0;
        int len = 0;
        while (1)
        {
                len = uart_read_bytes(UART_PORT, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
                if (len < 1)
                {
                        // delay 100ms
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        continue;
                }
                // FSM
                for (i = 0; i < len; i++)
                {

                        switch (status)
                        {
                        case HEAD0:
                                if (data[i] == HEAD0_TAG)
                                {
                                        // clean
                                        memset(&bodyPackage, 0, sizeof(BodyDef_t));
                                        bodyOperationDataCount = 0;
                                        status = HEAD1;
                                }
                                break;
                        case HEAD1:
                                if (data[i] == HEAD1_TAG)
                                {
                                        status = BODY_ID0;
                                }
                                else if (data[i] == HEAD0_TAG)
                                {
                                        status = HEAD1; // 重同步5
                                }
                                else
                                {
                                        status = HEAD0;
                                }
                                break;
                        case BODY_ID0:
                                ((uint8_t *)&bodyPackage)[0] = data[i];
                                status = BODY_ID1;
                                break;
                        case BODY_ID1:
                                ((uint8_t *)&bodyPackage)[1] = data[i];
                                status = BODY_ID2;
                                break;
                        case BODY_ID2:
                                ((uint8_t *)&bodyPackage)[2] = data[i];
                                status = BODY_ID3;
                                break;
                        case BODY_ID3:
                                ((uint8_t *)&bodyPackage)[3] = data[i];
                                status = BODY_CODE;
                                break;
                        case BODY_CODE:
                                ((uint8_t *)&bodyPackage)[4] = data[i];
                                status = BODY_SIZE0;
                                break;
                        case BODY_SIZE0:
                                ((uint8_t *)&bodyPackage)[5] = data[i];
                                status = BODY_SIZE1;
                                break;
                        case BODY_SIZE1:
                                ((uint8_t *)&bodyPackage)[6] = data[i];
                                if (bodyPackage.operationSize == 0)
                                {
                                        status = CRC0;
                                }
                                else if (bodyPackage.operationSize > MAX_OPERATIONDATA_SIZE)
                                {
                                        status = HEAD0;
                                }
                                else
                                {
                                        bodyOperationDataCount = 0;
                                        status = BODY_PAYLOAD;
                                }
                                break;
                        case BODY_PAYLOAD:
                                if (7 + bodyOperationDataCount > MAX_OPERATIONDATA_SIZE - 1)
                                {
                                        status = HEAD0;
                                }
                                ((uint8_t *)&bodyPackage)[7 + bodyOperationDataCount] = data[i];
                                bodyOperationDataCount++;
                                if (bodyOperationDataCount >= bodyPackage.operationSize)
                                {
                                        status = CRC0;
                                }
                                break;
                        case CRC0:
                                crcCode = ((uint16_t)data[i]);
                                status = CRC1;
                                break;
                        case CRC1:
                                crcCode |= ((uint16_t)data[i] << 8);
                                // Check CRC
                                uint16_t crcCalc = CRC16Check(((uint8_t *)&bodyPackage), BODY_HEADER_SIZE + bodyPackage.operationSize);
                                if (crcCalc == crcCode)
                                {
                                        status = TAIL0;
                                }
                                else
                                {
                                        ESP_LOGE(TAG, "CRC FAILED: excetped is : %02X, but got: %02X", crcCalc, crcCode);
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
                                        UartMessageProcesser(&bodyPackage);
                                        memset(&bodyPackage, 0, sizeof(BodyDef_t));
                                }
                                status = HEAD0;
                                break;
                        }
                }
                memset(data, 0, BUF_SIZE);
        }
        return;
}

int8_t UartMessageProcesser(BodyDef_t *bodyMessage)
{
        if (bodyMessage == NULL)
        {
                return -1;
        }
        UartSender(ACK, 0, NULL);
        switch (bodyMessage->operationCode)
        {
        case IMG_DATAHEAD:
                memcpy(&selfImgSize, bodyMessage->operationData + sizeof(uint8_t), sizeof(selfImgSize));

                // 尝试释放旧的
                if (selfImgPtr == NULL)
                {
                        free(selfImgPtr);
                        selfImgPtr = NULL;
                }

                // 重新获取新的
                selfImgPtr = (uint8_t *)heap_caps_malloc(selfImgSize, MALLOC_CAP_SPIRAM);
                if (selfImgPtr == NULL)
                {
                        ESP_LOGE(TAG, "malloc selfImgPtr failed, size: %d. At pos: %s %d", selfImgSize, __FILENAME__, __LINE__); // 操 是不是没开SPIRAM
                        return -1;
                }
                else
                {
                        ESP_LOGI(TAG, "malloc selfImgPtr success, size: %d", selfImgSize);
                }
                break;
        case IMG_DATABODY:
                uint32_t currentOffset = 0;
                memcpy(&currentOffset, bodyMessage->operationData, sizeof(uint32_t));
                if (bodyMessage->operationSize < sizeof(uint32_t))
                {
                        ESP_LOGE(TAG, "Valid Position. At pos: %s %d", __FILENAME__, __LINE__);
                        return -1;
                }
                // 我会一直监视你的
                if (currentOffset + bodyMessage->operationSize - sizeof(uint32_t) > selfImgSize)
                {
                        ESP_LOGE(TAG, "Valid Offset. At pos: %s %d", __FILENAME__, __LINE__);
                        return -1;
                }
                memcpy(selfImgPtr + currentOffset,
                       bodyMessage->operationData + sizeof(uint32_t),
                       bodyMessage->operationSize - sizeof(uint32_t));
                break;
        case IMG_DATATAIL:
                // printHex(selfImgPtr, selfImgSize);
                break;
        }

        return 0;
}

uint32_t uartSenderPackageID = 0;
void UartSender(const uint8_t operationCode, const uint16_t operationSize, const void *operationData)
{
        uint32_t frameSize = operationSize + 6 + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t);
        uint8_t *fullFrame = malloc(frameSize);
        if (fullFrame == NULL)
        {
                ESP_LOGI(TAG, "malloc fullFrame failed. At pos: %s %d", __FILENAME__, __LINE__);
                return;
        }

        if (operationSize > MAX_OPERATIONDATA_SIZE)
        {
                ESP_LOGE(TAG, "operationSize TOO LARGE. At pos: %s %d", __FILENAME__, __LINE__);
                return;
        }

        fullFrame[0] = 0xAA;
        fullFrame[1] = 0xAA;

        // 主！体！                     此处不拷贝packageID
        fullFrame[2] = 0;
        fullFrame[3] = 0;

        memcpy(fullFrame + 2 + sizeof(uint32_t), &operationCode, sizeof(uint8_t));
        memcpy(fullFrame + 2 + sizeof(uint32_t) + sizeof(uint8_t), &operationSize, sizeof(uint16_t));
        if (operationSize > 0)
        {
                memcpy(fullFrame + 2 + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t), operationData, operationSize);
        }
        uint16_t crc = CRC16Check(fullFrame + 2, frameSize - 6);
        int crcOffset = operationSize + 2 + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t);

        fullFrame[crcOffset] = (uint8_t)(crc & 0xFF);   // 低位
        fullFrame[crcOffset + 1] = (uint8_t)(crc >> 8); // 高位

        fullFrame[crcOffset + 2] = 0x0D;
        fullFrame[crcOffset + 3] = 0x07;
        uart_write_bytes(UART_PORT, fullFrame, frameSize);
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
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 122,
        };
        uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        // Configure UART parameters
        ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
        xTaskCreatePinnedToCore(
            F1CControlUartListener,
            "F1CControlUartListener",
            2048,
            NULL,
            1,
            NULL,
            0 // 绑定到 core0
        );

        return;
}
