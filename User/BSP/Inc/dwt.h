/*
 * @Author: Liu_xiyang 3230643253@qq.com
 * @Date: 2026-05-07 20:07:32
 * @LastEditors: Liu_xiyang 3230643253@qq.com
 * @LastEditTime: 2026-05-07 20:07:43
 * @FilePath: \FOC_Project\User\BSP\Inc\dwt.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
 ******************************************************************************
 * @file	bsp_dwt.h
 * @version V1.1.0
 * @brief   DWT (Data Watchpoint and Trace) 周期计数器驱动头文件
 ******************************************************************************
 * @attention
 *
 ******************************************************************************
 */
#ifndef _BSP_DWT_H
#define _BSP_DWT_H

#include "main.h"
#include "stdint.h"

/**
 * @brief DWT 时间结构体，用于存储分解后的时间
 */
typedef struct
{
    uint32_t s;     /*!< 秒 */
    uint16_t ms;    /*!< 毫秒 */
    uint16_t us;    /*!< 微秒 */
} DWT_Time_t;

/**
 * @brief  初始化DWT外设
 * @param  CPU_Freq_mHz: CPU核心频率，单位MHz
 */
void DWT_Init(uint32_t CPU_Freq_mHz);

/**
 * @brief  获取两次调用之间的时间差 (float, 单位: 秒)
 * @param  cnt_last: 指向一个uint32_t变量的指针，用于存储上一次的计数值
 * @retval float: 时间差，单位秒
 */
float DWT_GetDeltaT(uint32_t *cnt_last);

/**
 * @brief  获取两次调用之间的时间差 (double, 单位: 秒)
 * @param  cnt_last: 指向一个uint32_t变量的指针，用于存储上一次的计数值
 * @retval double: 时间差，单位秒 (双精度)
 */
double DWT_GetDeltaT64(uint32_t *cnt_last);

/**
 * @brief  获取从DWT初始化开始经过的时间 (float, 单位: 秒)
 * @retval float: 总时间，单位秒
 */
float DWT_GetTimeline_s(void);

/**
 * @brief  获取从DWT初始化开始经过的时间 (float, 单位: 毫秒)
 * @retval float: 总时间，单位毫秒
 */
float DWT_GetTimeline_ms(void);

/**
 * @brief  获取从DWT初始化开始经过的时间 (uint64_t, 单位: 微秒)
 * @retval uint64_t: 总时间，单位微秒
 */
uint64_t DWT_GetTimeline_us(void);

/**
 * @brief  使用DWT进行忙等待延时 (单位: 秒)
 * @param  Delay: 需要延时的时间，单位秒
 * @attention 这是一个阻塞式延时，会100%占用CPU。在RTOS中应避免使用。
 */
void DWT_Delay(float Delay);

/**
 * @brief  更新全局的系统时间结构体 SysTime
 */
void DWT_SysTimeUpdate(void);

extern DWT_Time_t SysTime; // 外部可访问的系统时间

#endif /* BSP_DWT_H_ */
