/**
 ******************************************************************************
 * @file    vofa.c
 * @brief   VOFA+ FireWater 协议通信模块
 * @author  Liu_xiyang
 * @date    2026-05-07
 ******************************************************************************
 * @attention
 *
 * 本模块实现 VOFA+ 上位机的 FireWater 协议，支持命名模式和 CSV 模式，
 *
 * 特性:
 *   - 通道注册模式，帧长和数据类型由用户自定义
 *   - 双缓冲乒乓切换，DMA 忙时自动丢帧防止阻塞
 *   - 发送限频，避免占用全部串口带宽
 *   - 支持 float / double / int8_t ~ int32_t / uint8_t ~ uint32_t
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "vofa.h"

/** @addtogroup BSP
  * @{
  */

/** @defgroup VOFA
  * @brief  VOFA+ 通信驱动
  * @{
  */

/* Private define ------------------------------------------------------------*/

/** @brief DMA 忙状态检测
  * @note  直接读取 DMA 句柄 State，无需依赖回调或中断标志
  */
#define DMA_BUSY  (vofa_uart->hdmatx->State == HAL_DMA_STATE_READY ? 0 : 1)

/* Private typedef -----------------------------------------------------------*/

/** @brief 格式化函数指针类型
  * @param buf  输出缓冲区
  * @param sz   缓冲区剩余大小
  * @param name 通道名（CSV 模式可传 NULL）
  * @param ptr  指向变量地址的 void 指针
  * @retval     写入的字符数（不含 \0），失败返回负值
  */
typedef int (*fmt_fn)(char *buf, int sz, const char *name, const void *ptr);

/* Private variables ---------------------------------------------------------*/

/** @brief 绑定的 UART 句柄，由 VOFA_Init() 设置 */
static UART_HandleTypeDef *vofa_uart = NULL;

/** @brief 双缓冲发送区 */
static char     tx_buf[2][VOFA_TX_BUF_SIZE];

/** @brief 当前可写入的 buffer 索引 (0 或 1) */
static uint8_t  cur_buf  = 0;

/** @brief 上次发送完成时的 HAL_GetTick() 值 */
static uint32_t last_tx  = 0;

/** @brief 两次发送之间的最小间隔 (ms)，0 = 不限频 */
static uint32_t min_ms   = 0;

/* Private function prototypes -----------------------------------------------*/

/* -------------------------------------------------------------------------- */
/*                      格式化函数 (每类型一个)                                  */
/* -------------------------------------------------------------------------- */

/**
  * @brief  将 float 变量格式化为 FireWater 字符串
  * @param  buf  : 输出缓冲区
  * @param  sz   : 缓冲区剩余字节数
  * @param  name : 变量名
  * @param  ptr  : 指向 float 变量的指针
  * @retval snprintf 返回值
  */
static int fmt_float(char *buf, int sz, const char *name, const void *ptr)
{
    return snprintf(buf, sz,
#if VOFA_NAMED_MODE
        "%s:%.4f", name, (double)*(float *)ptr
#else
        "%.4f", (double)*(float *)ptr
#endif
    );
}

/**
  * @brief  将 double 变量格式化为 FireWater 字符串
  * @param  buf  : 输出缓冲区
  * @param  sz   : 缓冲区剩余字节数
  * @param  name : 变量名
  * @param  ptr  : 指向 double 变量的指针
  * @retval snprintf 返回值
  */
static int fmt_double(char *buf, int sz, const char *name, const void *ptr)
{
    return snprintf(buf, sz,
#if VOFA_NAMED_MODE
        "%s:%.6f", name, *(double *)ptr
#else
        "%.6f", *(double *)ptr
#endif
    );
}

/**
  * @brief  将 int8_t 变量格式化为 FireWater 字符串
  * @param  buf  : 输出缓冲区
  * @param  sz   : 缓冲区剩余字节数
  * @param  name : 变量名
  * @param  ptr  : 指向 int8_t 变量的指针
  * @retval snprintf 返回值
  */
static int fmt_i8(char *buf, int sz, const char *name, const void *ptr)
{
    return snprintf(buf, sz,
#if VOFA_NAMED_MODE
        "%s:%d", name, (int)*(int8_t *)ptr
#else
        "%d", (int)*(int8_t *)ptr
#endif
    );
}

/**
  * @brief  将 int16_t 变量格式化为 FireWater 字符串
  * @param  buf  : 输出缓冲区
  * @param  sz   : 缓冲区剩余字节数
  * @param  name : 变量名
  * @param  ptr  : 指向 int16_t 变量的指针
  * @retval snprintf 返回值
  */
static int fmt_i16(char *buf, int sz, const char *name, const void *ptr)
{
    return snprintf(buf, sz,
#if VOFA_NAMED_MODE
        "%s:%d", name, (int)*(int16_t *)ptr
#else
        "%d", (int)*(int16_t *)ptr
#endif
    );
}

/**
  * @brief  将 int32_t 变量格式化为 FireWater 字符串
  * @param  buf  : 输出缓冲区
  * @param  sz   : 缓冲区剩余字节数
  * @param  name : 变量名
  * @param  ptr  : 指向 int32_t 变量的指针
  * @retval snprintf 返回值
  */
static int fmt_i32(char *buf, int sz, const char *name, const void *ptr)
{
    return snprintf(buf, sz,
#if VOFA_NAMED_MODE
        "%s:%ld", name, (long)*(int32_t *)ptr
#else
        "%ld", (long)*(int32_t *)ptr
#endif
    );
}

/**
  * @brief  将 uint8_t 变量格式化为 FireWater 字符串
  * @param  buf  : 输出缓冲区
  * @param  sz   : 缓冲区剩余字节数
  * @param  name : 变量名
  * @param  ptr  : 指向 uint8_t 变量的指针
  * @retval snprintf 返回值
  */
static int fmt_u8(char *buf, int sz, const char *name, const void *ptr)
{
    return snprintf(buf, sz,
#if VOFA_NAMED_MODE
        "%s:%u", name, (unsigned)*(uint8_t *)ptr
#else
        "%u", (unsigned)*(uint8_t *)ptr
#endif
    );
}

/**
  * @brief  将 uint16_t 变量格式化为 FireWater 字符串
  * @param  buf  : 输出缓冲区
  * @param  sz   : 缓冲区剩余字节数
  * @param  name : 变量名
  * @param  ptr  : 指向 uint16_t 变量的指针
  * @retval snprintf 返回值
  */
static int fmt_u16(char *buf, int sz, const char *name, const void *ptr)
{
    return snprintf(buf, sz,
#if VOFA_NAMED_MODE
        "%s:%u", name, (unsigned)*(uint16_t *)ptr
#else
        "%u", (unsigned)*(uint16_t *)ptr
#endif
    );
}

/**
  * @brief  将 uint32_t 变量格式化为 FireWater 字符串
  * @param  buf  : 输出缓冲区
  * @param  sz   : 缓冲区剩余字节数
  * @param  name : 变量名
  * @param  ptr  : 指向 uint32_t 变量的指针
  * @retval snprintf 返回值
  */
static int fmt_u32(char *buf, int sz, const char *name, const void *ptr)
{
    return snprintf(buf, sz,
#if VOFA_NAMED_MODE
        "%s:%lu", name, (unsigned long)*(uint32_t *)ptr
#else
        "%lu", (unsigned long)*(uint32_t *)ptr
#endif
    );
}

/* -------------------------------------------------------------------------- */
/*                          格式化函数查找表                                    */
/* -------------------------------------------------------------------------- */

/** @brief 类型枚举 → 格式化函数 映射表，编译期确定 */
static const fmt_fn fmt_table[VOFA_TYPE_COUNT] = {
    [VOFA_FLOAT]  = fmt_float,
    [VOFA_DOUBLE] = fmt_double,
    [VOFA_I8]     = fmt_i8,
    [VOFA_I16]    = fmt_i16,
    [VOFA_I32]    = fmt_i32,
    [VOFA_U8]     = fmt_u8,
    [VOFA_U16]    = fmt_u16,
    [VOFA_U32]    = fmt_u32,
};

/* -------------------------------------------------------------------------- */
/*                             内部辅助函数                                     */
/* -------------------------------------------------------------------------- */

/**
  * @brief  启动 DMA 发送
  * @param  data : 待发送数据指针
  * @param  len  : 发送字节数
  * @note   调用 HAL_UART_Transmit_DMA 启动传输，非阻塞立即返回
  */
static void vofa_tx(const char *data, int len)
{
    HAL_UART_Transmit_DMA(vofa_uart, (uint8_t *)data, len);
}

/* -------------------------------------------------------------------------- */
/*                              公开 API                                       */
/* -------------------------------------------------------------------------- */

/**
  * @brief  初始化 VOFA 通信模块
  * @param  huart : 指向 CubeMX 生成的 UART 句柄 (如 &huart1)
  * @note   调用本函数前须确保 USART 已在 main.c 中完成 MX_USARTx_UART_Init()
  * @retval None
  */
void VOFA_Init(UART_HandleTypeDef *huart)
{
    vofa_uart = huart;
    cur_buf   = 0;
    last_tx   = 0;
    min_ms    = 0;
    memset(tx_buf, 0, sizeof(tx_buf));
}

/**
  * @brief  按通道描述符数组发送一帧 FireWater 数据
  * @param  chs   : 用户定义的通道描述符数组
  * @param  count : 数组元素个数
  * @note
  *   - 本函数非阻塞，若 DMA 正在发送上一帧则直接丢弃本帧
  *   - 受 VOFA_SetMinInterval() 限频控制，过快的调用会被跳过
  *   - 帧格式: name1:val1,name2:val2,...\\n   (命名模式)
  *              val1,val2,...\\n              (CSV 模式)
  *   - 若单帧超出 VOFA_TX_BUF_SIZE 则截断丢弃后续通道
  * @retval None
  */
void VOFA_Send(VOFA_Channel *chs, int count)
{
    if (!vofa_uart || !chs || count <= 0) return;

    /* 非阻塞：DMA 忙则丢帧 */
    if (DMA_BUSY) return;

    /* 限频检查 */
    uint32_t now = HAL_GetTick();
    if (min_ms && now - last_tx < min_ms) return;
    last_tx = now;

    /* 遍历通道拼帧 */
    char *p   = tx_buf[cur_buf];
    int  rem  = VOFA_TX_BUF_SIZE;
    int  len  = 0;
    int  ok   = 0;

    for (int i = 0; i < count; i++) {
        fmt_fn fn = fmt_table[chs[i].type];
        ok = fn(p, rem, chs[i].name, chs[i].ptr);

        if (ok < 0 || ok >= rem) break;     /* 格式化失败或缓冲区满，截断 */

        p   += ok;
        rem -= ok;

        /* 通道间逗号 */
        if (i < count - 1 && rem > 1) {
            *p++ = ',';
            rem--;
        }
    }

    /* 帧尾换行 */
    if (rem > 1) {
        *p++ = '\n';
    }
    len = p - tx_buf[cur_buf];

    /* 启动 DMA 发送，切换乒乓缓冲 */
    vofa_tx(tx_buf[cur_buf], len);
    cur_buf ^= 1;
}

/**
  * @brief  printf 风格调试发送
  * @param  fmt : 格式化字符串 (同 printf)
  * @param  ... : 可变参数列表
  * @note
  *   - 内部自动追加 '\\n'，用户无需手动添加
  *   - 超出 VOFA_TX_BUF_SIZE 的内容会被截断，末尾补 '\\n'
  *   - DMA 忙时直接丢弃
  * @retval None
  */
void VOFA_Print(const char *fmt, ...)
{
    if (!vofa_uart || !fmt) return;

    /* DMA 忙检查 */
    if (DMA_BUSY) return;

    /* 格式化 */
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(tx_buf[cur_buf], VOFA_TX_BUF_SIZE, fmt, args);
    va_end(args);

    /* 截断保护: 保证末尾有 \n\0 */
    if (len < 0) len = 0;
    if (len >= VOFA_TX_BUF_SIZE) {
        len = VOFA_TX_BUF_SIZE - 2;
        tx_buf[cur_buf][len++] = '\n';
    } else {
        tx_buf[cur_buf][len++] = '\n';
    }
    tx_buf[cur_buf][len] = '\0';

    /* 启动 DMA 发送，切换乒乓缓冲 */
    vofa_tx(tx_buf[cur_buf], len);
    cur_buf ^= 1;
}

/**
  * @brief  设置两次发送之间的最小间隔 (限频)
  * @param  ms : 最小间隔 (毫秒)，传 0 取消限频
  * @note   示例: VOFA_SetMinInterval(10) 限制最多 100Hz 发送
  * @retval None
  */
void VOFA_SetMinInterval(uint32_t ms)
{
    min_ms = ms;
}

/**
  * @}
  */

/**
  * @}
  */
