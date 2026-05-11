/*
 * @Author: Liu_xiyang 3230643253@qq.com
 * @Date: 2026-05-10 21:22:56
 * @LastEditors: Liu_xiyang 3230643253@qq.com
 * @LastEditTime: 2026-05-10 22:33:13
 * @FilePath: \FOC_Project\User\BSP\Inc\foc_adc.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
 * @file    adc.h
 * @brief   ADC 电流采样模块
 * @details 通过 ADC 注入通道采集三相电流 (Ia/Ib/Ic) 及母线电压 (Vbus)，
 *          提供偏移校准与实时采样功能，用于 FOC 控制中的电流环反馈。
 */

#ifndef ADC_H
#define ADC_H

#include "main.h"
#include "adc.h"
#include "cmsis_os.h"

static const float FAC_CURRENT_ADC          = (3.3f / 4096.0f) * 4;           // 电流ADC转换系数
static const float FAC_VOLTAGE_ADC          = (3.3f / 4096.0f) * 11;          // 电压ADC转换系数

/** @brief ADC 电流采样数据结构 */
typedef struct {
    float currentAOffset;   /**< A 相电流零点偏移 (ADC 原始值) */
    float currentBOffset;   /**< B 相电流零点偏移 (ADC 原始值) */
    float currentCOffset;   /**< C 相电流零点偏移 (ADC 原始值) */
    float currentVBUS;      /**< 母线电压偏移 */

    float Ia;               /**< A 相实际电流 (A) */
    float Ib;               /**< B 相实际电流 (A) */
    float Ic;               /**< C 相实际电流 (A) */
    float Vbus;             /**< 母线实际电压 (V) */
} adc_current_t;

/** @brief 全局 ADC 采样数据实例 */
extern adc_current_t adc_data;

/**
 * @brief  ADC 初始化并自动校准零点偏移
 * @param  hadc     ADC 句柄指针
 * @param  adc_data 数据存储结构体指针
 */
void ADC_Init(ADC_HandleTypeDef* hadc, adc_current_t* adc_data);

/**
 * @brief  读取 ADC 注入通道并计算实际电流与母线电压
 * @details 从四个注入通道读取原始 ADC 值，减去零点偏移后乘以标定系数，
 *          得到 A/B/C 三相实际电流及母线电压。
 * @param  hadc     ADC 句柄指针
 * @param  adc_data 结果写入的目标结构体
 */
void UpdateCurrent(ADC_HandleTypeDef* hadc, adc_current_t* adc_data);

#endif /* ADC_H */