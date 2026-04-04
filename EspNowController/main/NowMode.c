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

/// 线程安全的ACK标志位
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

/* ===== 接收回调 ===== */
static void ReceiveCallBack(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
        uint8_t *srcMac = info->src_addr;
        packageBody_t *pkt = (packageBody_t *)data;
        packageBody_t outpkt = {0};
        outpkt.packageId = 0;
        if (srcMac == NULL || data == NULL || len <= 0)
        {
                return;
        }
        if (IS_BROADCAST_ADDR(info->des_addr))
        {
                /* 回发广播（让对方发现自己） */
                ESP_LOGI(TAG, "收到广播来自: " MACSTR, MAC2STR(srcMac));
                outpkt.type = NOW_DETECT;
                memcpy(outpkt.payload, "HELLO", 6);
                esp_now_send(srcMac, (uint8_t *)&outpkt, sizeof(outpkt));
        }
        else
        {
                ESP_LOGI(TAG, "收到单播来自: " MACSTR, MAC2STR(srcMac));
        }

        switch (pkt->type)
        {
        case 2:
                ESP_LOGI(TAG, "收到普通数据：%s", (char *)(pkt->payload));
                outpkt.type = 3;
                esp_now_send(srcMac, (uint8_t *)&outpkt, sizeof(outpkt));
                break;
        case 3:
                ESP_LOGI(TAG, "ACK");
                ACKFlag = true;
                break;
        }

        bool newMac = AddMac(srcMac);
        if (newMac)
        {
                xTaskCreate(
                    SendImg,       // 任务函数
                    "SendImgTask", // 名字
                    4096,          // 栈大小
                    srcMac,        // 参数（传指针）
                    5,             // 优先级
                    NULL           // 任务句柄
                );
        }
}

/* ===== 发送扩列图 ===== */
static void SendImg(void *pvParameters)
{

        uint8_t *mac = (uint8_t *)pvParameters;
        while (true)
        {
                packageBody_t pkt = {0};
                pkt.type = 2;
                pkt.packageId = 0;
                memcpy(pkt.payload, "GETGETGET12313123123", 21);
                esp_now_send(mac, (uint8_t *)&pkt, sizeof(pkt));
                if (ACKFlag == true)
                {
                        ACKFlag = false;
                        break;
                }
                else
                {
                        vTaskDelay(SEND_INTERVAL / portTICK_PERIOD_MS);
                }
        }
        vTaskDelete(NULL);
}

/* ===== 发送回调（可选） ===== */
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
                packageBody_t pkt = {0};
                pkt.type = 0;
                memcpy(pkt.payload, "DISCOVER", 9);

                esp_err_t err = esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));

                if (err != ESP_OK)
                {
                        ESP_LOGE(TAG, "广播发送失败");
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