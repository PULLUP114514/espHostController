#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// 此枚举在esp-now和UART中通用
typedef enum
{
        NOW_DETECT = 1,          // NULL                                                                        探测包
        IMG_DATAHEAD = 2,        // uint8 + uint32      (数据类型 0 -> 图片 1 -> 文本) (图片大小 < 512K)
        IMG_DATABODY = 3,        // #DYNAMIC <= 512                                                             图片主体
        IMG_DATATAIL = 4,        // NULL                                                                        功能待定
        REQ_IMG = 5,             // NULL                                                                        此功能仅在UART中使用 由ESP向F1C请求图片
        SEND_IMG = 6,            // uint32              图片大小
        CONTOL_BRIGHTNESS = 128, // int32               目标屏幕亮度
        CONTOL_EXIT = 129,       // NULL                                                                        退出Drm_App
        ACK = 256                // NULL                                                                        通用ACK
} OPERATION_CODE_ENUM;

typedef struct
{
        uint8_t type; // 0=广播  1=单播
        uint32_t packageId;
        uint8_t payload[64];
} packageBody_t;

// 状态机读取状态
typedef enum
{
        HEAD0,
        HEAD1,
        BODY,
        CRC0,
        CRC1,
        TAIL0,
        TAIL1,
        SUCCESS
} FCMReadStatus;
typedef enum
{
        HEAD0_TAG = 0xAA,
        HEAD1_TAG = 0x55,
        TAIL0_TAG = 0x0D,
        TAIL1_TAG = 0x07
} FCMTag;

#pragma pack(push, 1) // 紧凑 1字节对齐

typedef struct
{
        uint32_t packageID;
        uint8_t OperationCode;
        uint16_t OperationSize;
        uint8_t OperationData[512];
} BodyDef_t;

#pragma pack(pop) // 恢复默认

#endif