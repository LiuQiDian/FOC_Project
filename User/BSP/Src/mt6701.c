/**
 ******************************************************************************
 * @file    mt6701.c
 * @brief   MT6701 磁编码器驱动 — SPI DMA 模式
 * @author  Liu_xiyang
 * @date    2026-05-09
 ******************************************************************************
 * @attention
 *
 * 数据帧格式 (4 字节 SPI, MSB 先出, 时钟空闲高 CPOL=1 CPHA=1):
 *
 *   ┌─────────┬─────────┬─────────┬─────────┐
 *   │ Byte 0  │ Byte 1  │ Byte 2  │ Byte 3  │
 *   ├─────────┼─────────┼─────────┼─────────┤
 *   │ PA[13:6]│ PA[5:0] │ ST[3:2] │  保留   │
 *   │         │ ST[1:0] │         │         │
 *   └─────────┴─────────┴─────────┴─────────┘
 *
 *   PA[13:0] = 14-bit 绝对角度, 0~16383 → 0°~360°
 *   ST[3:0]  = 4-bit 磁场强度状态, 仅低 2 位有效
 *     - 00: 磁场过强 (磁铁太近)
 *     - 01: 磁场过弱 (磁铁太远)
 *     - 10: 正常
 *     - 11: 无效
 *
 * DMA 工作模式:
 *   本驱动采用 "提前预取" 策略 — 每次调用 angleRead() 时:
 *     1. 取走上一次 DMA 完成的数据 (已就绪在缓冲区)
 *     2. 立刻启动下一次 DMA
 *   这样在控制环中调用 angleRead() 时几乎是零等待 (DMA 早已完成),
 *   适合 FOC 电流环等高频场景。
 *
 *   首次使用时必须调用 MT6701_DMA_Init() 完成初始帧抓取和预取启动。
 *
 *   SPI 频率: 受限于 MT6701, 推荐 ≤ 10MHz (CubeMX 配置)
 *   DMA 传输时间: 4 字节 × 1/SPI_CLK × 8 ≈ 3.2µs @ 10MHz
 *
 ******************************************************************************
 */

#include "mt6701.h"

/* ===================== DMA 缓冲区 ===================== */

/* DMA 要求 SRAM 静态区 (非栈上) — 传输期间 CPU 不可写 */
static uint8_t dma_tx_buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};  // SPI 写 dummy 字节
static uint8_t dma_rx_buf[4] = {0};

/* ===================== 内部辅助 ===================== */

/**
  * @brief  启动下一次 DMA 传输 (CS 拉低 → DMA → 完成回调拉高 CS)
  * @note   非阻塞，DMA 硬件在后台完成传输，完成后 HAL_SPI_TxRxCpltCallback 释放 CS
  */
static void MT6701_TriggerDMA(void)
{
    MT6701_CS_LOW();
    HAL_SPI_TransmitReceive_DMA(&MT6701_SPI, dma_tx_buf, dma_rx_buf, 4);
}

/**
  * @brief  从 DMA 缓冲区拷贝一帧数据到用户 buffer
  * @param  pBuffer : 至少 4 字节的输出缓冲区
  */
static void MT6701_Read_RAW(uint8_t *pBuffer)
{
    pBuffer[0] = dma_rx_buf[0];
    pBuffer[1] = dma_rx_buf[1];
    pBuffer[2] = dma_rx_buf[2];
    pBuffer[3] = dma_rx_buf[3];
}

/* ===================== HAL 回调 ===================== */

/**
  * @brief  SPI DMA 传输完成回调 (由 DMA2_Stream0 IRQ → HAL_SPI_IRQHandler 触发)
  * @note   仅释放 CS，不拷贝数据 (数据已在 DMA 缓冲区中，angleRead 会取走)
  */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        MT6701_CS_HIGH();   // 一帧结束，释放 CS
    }
}

/* ===================== API 实现 ===================== */

/**
  * @brief  初始化 MT6701 DMA 模式
  * @note
  *   1. 首次用阻塞 SPI 读一帧 (确保缓冲区有有效数据)
  *   2. 启动第一个 DMA 预取 (后续 angleRead 调用会取走这帧并触发下一帧)
  *   调用时机: 在 FreeRTOS 任务启动后，进入控制循环之前调用一次即可。
  */
void MT6701_DMA_Init(void)
{
    uint8_t tx_buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    /* 阻塞读取第一帧 — 此时 DMA 尚未启动，用阻塞方式确保数据有效 */
    MT6701_CS_LOW();
    HAL_SPI_TransmitReceive(&MT6701_SPI, tx_buf, dma_rx_buf, 4, 100);
    MT6701_CS_HIGH();

    /* 启动 DMA 预取管道 — 下次 angleRead() 可以直接取数据 */
    MT6701_TriggerDMA();
}

/**
  * @brief  读取当前角度 (rad) — 高频控制环主力接口
  * @retval 角度值 0.0f ~ 2π (弧度制)
  *
  * @note   调用流程:
  *          angleRead()               DMA 后台
  *          ───────────               ────────
  *          1. 取 dma_rx_buf 数据      ←── DMA 上一帧已完成
  *          2. 触发 MT6701_TriggerDMA  ──→ DMA 开始新一帧传输
  *          3. 解析角度                 DMA 传输中 (~3µs)
  *          4. 返回                      DMA 完成, CS 被回调拉高
  *
  *          下一次 angleRead() 取到的就是这帧 DMA 完成的数据。
  */
float angleRead(void)
{
    uint8_t data[4];
    uint16_t angle_raw;

    MT6701_Read_RAW(data);     // 取出上一次 DMA 完成的帧
    MT6701_TriggerDMA();       // 立即预取下一帧 (非阻塞)

    /* 拼接 14-bit 角度: Byte0[7:0] + Byte1[7:2] */
    angle_raw  = (uint16_t)(data[1] >> 2);   // PA[5:0] ← 去掉 ST[1:0]
    angle_raw |= ((uint16_t)data[0] << 6);   // PA[13:6]

    return (float)angle_raw * (MT6701_TWO_PI / (float)MT6701_RESOLUTION);
}

/**
  * @brief  跨圈连续角度 — 支持多圈累计
  * @param  rawAngle : 当前帧弧度角度 (来自 angleRead())
  * @retval 累计弧度值，每圈递增/递减 2π
  *
  * @note   通过相邻帧角度差判断是否跨圈:
  *          - 当前帧比上一帧突然减小 > 0.8×2π → 正向跨圈 → 加 2π
  *          - 当前帧比上一帧突然增大 > 0.8×2π → 反向跨圈 → 减 2π
  *          阈值 0.8 防止噪声导致误判 (正常转速下相邻帧差 << π)
  */
float GetAngle(float rawAngle)
{
    static float angle_prev       = 0.0f;
    static float full_rotations   = 0.0f;

    float delta = rawAngle - angle_prev;

    /* 检测跨圈: 相邻帧角度差超过 0.8 圈则判定为跨圈 */
    if (fabsf(delta) > (0.8f * MT6701_TWO_PI)) {
        full_rotations += (delta > 0.0f) ? -MT6701_TWO_PI : MT6701_TWO_PI;
    }

    angle_prev = rawAngle;
    return full_rotations + rawAngle;
}

/**
  * @brief  完整读取: 原始值 + 角度(度) + 磁场状态 (独立阻塞 SPI)
  * @param  angle_raw    : [out] 14-bit 原始值 0~16383，可为 NULL
  * @param  angle        : [out] 角度 0°~360°，可为 NULL
  * @param  field_status : [out] 磁场状态 (MT6701_FIELD_*)，可为 NULL
  *
  * @note   独立触发一次阻塞 SPI 读取，不走 DMA 握手流水线。
  *         适合诊断/校准场景，高频场景请用 angleRead() + DMA 模式。
  */
void MT6701_read(uint16_t *angle_raw, float *angle, uint8_t *field_status)
{
    uint8_t data[4] = {0};
    uint8_t tx_buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint16_t raw;
    uint8_t  status;

    /* 阻塞 SPI 读取一帧 */
    MT6701_CS_LOW();
    HAL_SPI_TransmitReceive(&MT6701_SPI, tx_buf, data, 4, 100);
    MT6701_CS_HIGH();

    /* 解析 14-bit 角度: Byte0[7:0] = PA[13:6], Byte1[7:2] = PA[5:0] */
    raw  = (uint16_t)(data[1] >> 2);
    raw |= ((uint16_t)data[0] << 6);

    /* 解析 4-bit 状态: Byte2[7:6] = ST[3:2], Byte1[1:0] = ST[1:0] */
    status  = (data[2] >> 6);
    status |= (data[1] & 0x03) << 2;

    if (angle_raw != NULL) {
        *angle_raw = raw;
    }
    if (angle != NULL) {
        *angle = (float)raw * (360.0f / (float)MT6701_RESOLUTION);
    }
    if (field_status != NULL) {
        *field_status = status & 0x03;   // 仅低 2 位: 强/弱/正常/无效
    }
}
