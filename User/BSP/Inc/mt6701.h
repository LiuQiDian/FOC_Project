/*
 * @Author: Liu_xiyang 3230643253@qq.com
 * @Date: 2026-05-09 15:34:13
 * @LastEditors: Liu_xiyang 3230643253@qq.com
 * @LastEditTime: 2026-05-09 16:30:00
 * @FilePath: \FOC_Project\User\BSP\Inc\mt6701.h
 * @Description: MT6701 磁编码器驱动 — SPI DMA 模式
 *
 * MT6701 是 MPS 公司的 14-bit 磁性角度传感器，通过 SPI 接口输出：
 *   - 14-bit 绝对角度 (0 ~ 16383)
 *   - 4-bit 磁场状态 (指示磁铁安装距离是否合适)
 *
 * 数据帧 (4 字节，MSB 先出):
 *   Byte0: PA[13:6]   — 角度高 8 位
 *   Byte1: PA[5:0]    — 角度低 6 位
 *          ST[1:0]     — 状态低 2 位
 *   Byte2: ST[3:2]    — 状态高 2 位
 *   Byte3: 保留
 *
 * 本驱动使用 DMA 非阻塞读取，流程:
 *   1. MT6701_DMA_Init()  — 阻塞读一帧 + 启动预取 DMA
 *   2. angleRead()         — 取 DMA 缓冲区数据 + 触发下一帧 DMA
 *      每次调用返回的是上一次 DMA 完成的数据，延迟约 3µs
 */

#ifndef __MT6701_H
#define __MT6701_H

#include "main.h"
#include "spi.h"
#include <math.h>   /* fabsf */

/* ===================== 硬件常量 ===================== */

#define MT6701_SPI              hspi1       // CubeMX 生成的 SPI 句柄
#define MT6701_CS_PORT          GPIOC       // CS 引脚所在 GPIO 端口
#define MT6701_CS_PIN           4U          // CS 引脚号 (PC4)

/* BSRR 快速 GPIO 控制 (比 HAL 更快，~2 个时钟周期) */
#define MT6701_CS_HIGH()        (MT6701_CS_PORT->BSRR = (1U << MT6701_CS_PIN))
#define MT6701_CS_LOW()         (MT6701_CS_PORT->BSRR = (1U << (MT6701_CS_PIN + 16)))

/* 编码器参数 */
#define MT6701_RESOLUTION       16384UL     // 14-bit 分辨率
#define MT6701_TWO_PI           6.28318530718f

/* 磁场状态 (ST[1:0]) */
#define MT6701_FIELD_STRONG     0x00        // 磁场过强 → 磁铁太近
#define MT6701_FIELD_WEAK       0x01        // 磁场过弱 → 磁铁太远
#define MT6701_FIELD_GOOD       0x02        // 磁场正常
#define MT6701_FIELD_INVALID    0x03        // 无效数据

/* ===================== 设备句柄 ===================== */

/**
  * @brief  MT6701 设备描述符
  * @note   保存多圈跟踪状态，由 GetAngle() 维护
  */
typedef struct {
    SPI_HandleTypeDef *hspi;            // SPI 句柄
    float              angle_prev;      // 上一帧角度 (rad)，用于多圈计算
    float              full_rotations;  // 累计整圈偏移量 (rad)
} MT6701_Handle;

/* ===================== API ===================== */

/**
  * @brief  初始化 MT6701 DMA 模式
  * @note   阻塞读一帧填充缓冲区，然后启动第一次 DMA 预取。
  *         必须在 FreeRTOS 调度器启动后调用 (依赖 DMA 中断)。
  */
void MT6701_DMA_Init(void);

/**
  * @brief  读取当前角度 (rad)
  * @retval 角度值 0.0f ~ 2π，弧度制
  * @note   每次调用:
  *           1. 取走 DMA 缓冲区中的上一帧数据
  *           2. 立即触发下一帧 DMA
  *         延迟 ~3µs，适合在控制环 (10-50kHz) 中调用
  */
float angleRead(void);

/**
  * @brief  跨圈连续角度 (rad) — 支持多圈累计
  * @param  rawAngle : 当前帧的弧度角度 (来自 angleRead())
  * @retval 累积角度 (可能 > 2π 或 < 0)，每圈累积 2π
  * @note   通过比较相邻帧角度差判断是否跨圈 (阈值 0.8×2π)。
  *         适用于需要绝对位置跟踪的场景 (如电机轴累计转数)。
  */
float GetAngle(float rawAngle);

/**
  * @brief  完整读取: 原始值 + 角度(度) + 磁场状态
  * @param  angle_raw    : [out] 原始 14-bit 角度 (0~16383)，可为 NULL
  * @param  angle        : [out] 角度值 0°~360°，可为 NULL
  * @param  field_status : [out] 磁场状态 (MT6701_FIELD_*)，可为 NULL
  * @note   不走 DMA 握手链，独立触发一次 SPI 阻塞读取。
  *         适合诊断/校准场景，高频场景请用 angleRead()。
  */
void MT6701_read(uint16_t *angle_raw, float *angle, uint8_t *field_status);

#endif /* __MT6701_H */
