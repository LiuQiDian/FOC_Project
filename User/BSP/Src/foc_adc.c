/*
 * @Author: Liu_xiyang 3230643253@qq.com
 * @Date: 2026-05-10 21:22:54
 * @LastEditors: Liu_xiyang 3230643253@qq.com
 * @LastEditTime: 2026-05-12 19:51:04
 * @FilePath: \FOC_Project\User\BSP\Src\foc_adc.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
 * @file    adc.c
 * @brief   ADC 电流采样模块实现
 */

#include "foc_adc.h"

/** @brief 全局 ADC 采样数据 */
adc_current_t adc_data;

/**
 * @brief  更新三相电流零点偏移
 * @details 对三个注入通道各采样 2000 次，计算算术平均值作为偏移量。
 *          每次采样间隔 1ms，总计耗时约 2s。
 * @param  hadc      ADC 句柄指针
 * @param  adc_data  偏移值写入的目标结构体
 */
static void UpdateCurrentOffsets(ADC_HandleTypeDef* hadc, adc_current_t* adc_data)
{
    long long tempAOffset = 0;
    long long tempBOffset = 0;
    long long tempCOffset = 0;
    for (int i = 0; i < 2000; i++)
    {
      HAL_ADCEx_InjectedPollForConversion(hadc, 1); // 等待转换完成
      tempAOffset += HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
      tempBOffset += HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);
      tempCOffset += HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
      vTaskDelay(pdMS_TO_TICKS(1)); // 延时 1 毫秒
    }
    adc_data->currentAOffset = (float)tempAOffset / 2000.0f;
    adc_data->currentBOffset = (float)tempBOffset / 2000.0f;
    adc_data->currentCOffset = (float)tempCOffset / 2000.0f;
}

/**
 * @brief  ADC 初始化并自动校准零点偏移
 * @details 启动注入通道 → 计算电流偏移 → 停止注入通道。
 *          上电时调用一次即可完成初始化与校准。
 * @param  hadc     ADC 句柄指针
 * @param  adc_data 偏移值写入的目标结构体
 */
void ADC_Init(ADC_HandleTypeDef* hadc, adc_current_t* adc_data)
{
    HAL_ADCEx_InjectedStart(hadc); // 只启动ADC，不启动中断
    UpdateCurrentOffsets(hadc, adc_data); // 计算电流偏移
    HAL_ADCEx_InjectedStop(hadc);
}

/**
 * @brief  读取 ADC 注入通道并计算实际电流与母线电压
 * @details 从四个注入通道读取原始 ADC 值，减去零点偏移后乘以标定系数
 *          (FAC_CURRENT_ADC / FAC_VOLTAGE_ADC)，得到实际物理量。
 * @param  hadc     ADC 句柄指针
 * @param  adc_data 结果写入的目标结构体
 */
void UpdateCurrent(ADC_HandleTypeDef* hadc, adc_current_t* adc_data)
{
  float adcValueIa = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1); // 获取 IA 电流
  float adcValueIb = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);
  float adcValueIc = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
  float adcValueVbus = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_4); // 获取 VBUS 电压

  adc_data->Ia = (float)(adcValueIa - adc_data->currentAOffset) * FAC_CURRENT_ADC;
  adc_data->Ib = (float)(adcValueIb - adc_data->currentBOffset) * FAC_CURRENT_ADC;
  adc_data->Ic = (float)(adcValueIc - adc_data->currentCOffset) * FAC_CURRENT_ADC;
  adc_data->Vbus = (float)adcValueVbus * FAC_VOLTAGE_ADC; // 根据 ADC 分辨率和参考电压计算 VBUS
}

