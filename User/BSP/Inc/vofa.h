#ifndef __VOFA_H__
#define __VOFA_H__

#include "main.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

/* ===================== 用户配置 ===================== */

#define VOFA_TX_BUF_SIZE     256       // 单帧最大字节数
#define VOFA_NAMED_MODE      1         // 1=命名模式(id:val), 0=CSV模式(val,val)

/* ===================== 类型枚举 ===================== */

typedef enum {
    VOFA_FLOAT,
    VOFA_DOUBLE,
    VOFA_I8,
    VOFA_I16,
    VOFA_I32,
    VOFA_U8,
    VOFA_U16,
    VOFA_U32,
    VOFA_TYPE_COUNT
} VOFA_DType;

/* ===================== 通道描述符 ===================== */

typedef struct {
    const char *name;
    VOFA_DType  type;
    void       *ptr;
} VOFA_Channel;



void VOFA_Init(UART_HandleTypeDef *huart);
void VOFA_Send(VOFA_Channel *chs, int count);
void VOFA_Print(const char *fmt, ...);
void VOFA_SetMinInterval(uint32_t ms);

#endif
