#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// 此枚举在esp-now和UART中通用
typedef enum
{
        NOW_DETECT = 1,
        IMG_DATAHEAD = 2,
        IMG_DATABODY = 3,
        IMG_DATATAIL = 4,
        REQ_IMG = 5,             // 此功能仅在UART中使用
        CONTOL_BRIGHTNESS = 128, // Operation Data 为一个int 代表目标屏幕亮度
        CONTOL_EXIT = 5,         // 退出Drm_App
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