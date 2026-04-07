#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "EspnowConfig.h"
#include "TypeDef.h"

#define TAG "ESP_DISCOVER"

#define CHANNEL 1
#define SEND_INTERVAL 1000
#define MAX_PEERS 20

static void SendImg(void *pvParameters);

/// @brief 线程安全的ACK标志位
volatile bool ACKFlag = false;
static uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 设备列表
static uint8_t discovered_macs[MAX_PEERS][6];
static int discovered_count = 0;

/* ===== 判断是否存在 ===== */
static bool MacExists(uint8_t *mac)
{
        for (int i = 0; i < discovered_count; i++)
        {
                if (memcmp(discovered_macs[i], mac, 6) == 0)
                        return true;
        }
        return false;
}

/* ===== 添加设备 ===== */
static bool AddMac(uint8_t *mac)
{
        if (discovered_count >= MAX_PEERS)
                return false;

        if (!MacExists(mac))
        {
                memcpy(discovered_macs[discovered_count], mac, 6);
                discovered_count++;
                esp_now_peer_info_t peer;
                memcpy(peer.peer_addr, mac, 6);
                peer.channel = CHANNEL;      // 当前信道
                peer.ifidx = ESPNOW_WIFI_IF; // STA
                peer.encrypt = false;
                esp_now_add_peer(&peer);
                ESP_LOGI(TAG, "发现新设备: " MACSTR, MAC2STR(mac));
                return true;
        }

        return false;
}

/* ===== WiFi 初始化 ===== */
static void InitWifi(void)
{
        esp_netif_init();
        esp_event_loop_create_default();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(80)); // 最大功率 20dBm
        esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
}

void NowPrintHex(const uint8_t *data, uint32_t size)
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

/* ===== 接收回调 ===== */
static void ReceiveCallBack(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
        uint8_t *srcMac = info->src_addr;
        BodyDef_t *pkt = (BodyDef_t *)data;
        BodyDef_t outpkt = {0};
        outpkt.packageID = 0;
        if (srcMac == NULL || data == NULL || len <= 0)
        {
                return;
        }

        ESP_LOGI(TAG, "Get package OperationCode: %d", pkt->operationCode);

        if (IS_BROADCAST_ADDR(info->des_addr))
        {
                /* 回发广播（让对方发现自己） */
                ESP_LOGI(TAG, "收到广播来自: " MACSTR, MAC2STR(srcMac));
                outpkt.operationCode = NOW_DETECT;
                esp_now_send(srcMac, (uint8_t *)&outpkt, sizeof(outpkt));
        }
        else
        {
                // ESP_LOGI(TAG, "收到单播来自: " MACSTR, MAC2STR(srcMac));
        }

        switch (pkt->operationCode)
        {
        case 2:
                ESP_LOGI(TAG, "Get normal data: %s", (char *)(pkt->operationData));
                outpkt.operationCode = 3;
                // ack Package
                esp_now_send(srcMac, (uint8_t *)&outpkt, sizeof(outpkt));
                break;
        case 3:
                ESP_LOGI(TAG, "ACK");
                ACKFlag = true;
                break;
        case SEND_IMG:
                ESP_LOGI(TAG, "GET img data");
                // NowPrintHex((pkt->operationData), pkt->operationSize);
        }

        bool newMac = AddMac(srcMac);
        if (newMac)
        {
                // SendImg(srcMac);
                xTaskCreate(
                    SendImg,
                    "SendImgTask",
                    4096,
                    srcMac,
                    5,
                    NULL);
        }
}

/* ===== 发送扩列图 ===== */
static void SendImg(void *pvParameters)
{
        uint8_t *mac = (uint8_t *)pvParameters;
        BodyDef_t pkt = {0};

        // esp_now_send(mac, (uint8_t *)&pkt, 64);
        pkt.operationCode = SEND_IMG;
        uint32_t currentOffset = 0;
        while (selfImgPtr == NULL)
        {
                vTaskDelay(100);
        }
        while (currentOffset < selfImgSize)
        {

                ESP_LOGI(TAG, "SENDING");
                // 计算本次发送长度（处理最后一包）
                uint16_t chunkSize = selfImgSize - currentOffset;
                if (chunkSize > MAX_OPERATIONDATA_SIZE)
                        chunkSize = MAX_OPERATIONDATA_SIZE;

                pkt.operationSize = chunkSize;

                // 拷贝数据到 operationData
                memcpy(
                    (uint8_t *)&pkt + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t),
                    selfImgPtr + currentOffset,
                    chunkSize);

                // 发送：注意长度 = 头 + 数据
                uint32_t sendSize =
                    sizeof(uint32_t) + // packageID
                    sizeof(uint8_t) +  // operationCode
                    sizeof(uint16_t) + // operationSize
                    chunkSize;         // 实际数据

                esp_now_send(mac, (uint8_t *)&pkt, sendSize);

                currentOffset += chunkSize;

                // 可选：防止发太快（ESP-NOW建议）
                vTaskDelay(pdMS_TO_TICKS(5));
        }

        vTaskDelete(NULL);
}

// if (ACKFlag == true)
// {
//         ACKFlag = false;
//         break;
// }
// else
// {
//         vTaskDelay(SEND_INTERVAL / portTICK_PERIOD_MS);
// }

/* ===== 发送回调 ===== */
static void SendCallBack(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
        if (info)
        {
                ESP_LOGD(TAG, "发送到 " MACSTR " 状态:%d",
                         MAC2STR(info->des_addr), status);
        }
}

/* ===== ESP-NOW 初始化 ===== */
static void InitEspNow(void)
{
        esp_now_init();

        esp_now_register_recv_cb(ReceiveCallBack);
        esp_now_register_send_cb(SendCallBack);

        /* 添加广播 peer */
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, broadcastMac, 6);
        peer.channel = CHANNEL;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;

        esp_now_add_peer(&peer);
}

/* ===== 广播任务 ===== */
static void BroadcastTask(void *arg)
{
        while (1)
        {
                BodyDef_t pkt = {0};
                pkt.operationCode = 0;
                esp_err_t err = esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
                if (err != ESP_OK)
                {
                        ESP_LOGE(TAG, "广播发送失败: %s", esp_err_to_name(err));
                }

                vTaskDelay(SEND_INTERVAL / portTICK_PERIOD_MS);
        }
}

/* ===== 主入口 ===== */
void StartEspNow(void)
{

        /* NVS */
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
        {
                nvs_flash_erase();
                nvs_flash_init();
        }

        InitWifi();
        InitEspNow();

        xTaskCreate(BroadcastTask, "BroadcastTask", 2048, NULL, 4, NULL);
}

int8_t NowMessageProcesser(BodyDef_t *bodyMessage)
{
        if (bodyMessage == NULL)
        {
                return -1;
        }
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