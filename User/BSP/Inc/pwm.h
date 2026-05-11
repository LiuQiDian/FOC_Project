#ifndef __PWM_H__
#define __PWM_H__

#include "main.h"
#include "tim.h"
#include <stdint.h>

/* ===================== 硬件常量 ===================== */

#define PWM_TIM1_CLOCK_HZ      168000000UL
#define PWM_TIM1_PERIOD        4200U          // ARR = 4200-1 → 40kHz 中心对齐
#define PWM_TIM1_DEADTIME_NS   0U             // 默认死区 0ns，可重配

/* ===================== 错误码 ===================== */

typedef enum {
    PWM_OK = 0,
    PWM_ERR_NULL_HANDLE,
    PWM_ERR_INVALID_CHANNEL,
    PWM_ERR_DUTY_OVERFLOW,
    PWM_ERR_NOT_STARTED,
} PWM_Status;

/* ===================== PWM 句柄 ===================== */

typedef struct {
    TIM_HandleTypeDef *htim;          // HAL 定时器句柄
    uint32_t           period;        // ARR 值
    uint32_t           max_pulse;     // 安全上限 (clamp 到 95% 防止上下管直通)
    uint8_t            started;       // 输出使能标志
} PWM_Handle;

/* ===================== API ===================== */

/**
  * @brief  绑定 HAL 定时器句柄并初始化 PWM 模块
  * @param  pwm  : PWM 句柄指针
  * @param  htim : CubeMX 生成的定时器句柄 (如 &htim1)
  * @retval PWM_Status
  */
PWM_Status PWM_Init(PWM_Handle *pwm, TIM_HandleTypeDef *htim);

/**
  * @brief  启动所有 PWM 通道输出
  * @param  pwm : PWM 句柄指针
  * @retval PWM_Status
  */
PWM_Status PWM_Start(PWM_Handle *pwm);

/**
  * @brief  停止所有 PWM 通道输出 (输出强制低)
  * @param  pwm : PWM 句柄指针
  * @retval PWM_Status
  */
PWM_Status PWM_Stop(PWM_Handle *pwm);

/**
  * @brief  紧急制动 — 立即封波 (BKIN 软件触发)
  * @param  pwm : PWM 句柄指针
  * @note   比 PWM_Stop 更快，走硬件刹车路径
  * @retval PWM_Status
  */
PWM_Status PWM_EmergencyBrake(PWM_Handle *pwm);

/**
  * @brief  设置单通道占空比 (百分比)
  * @param  pwm     : PWM 句柄指针
  * @param  channel : 定时器通道号 (TIM_CHANNEL_1 ~ TIM_CHANNEL_4)
  * @param  percent : 占空比 0.0f ~ 1.0f
  * @note   自动 clamp 到 [0, max_pulse/period]
  * @retval PWM_Status
  */
PWM_Status PWM_SetDutyPercent(PWM_Handle *pwm, uint32_t channel, float percent);

/**
  * @brief  设置单通道占空比 (比较值)
  * @param  pwm     : PWM 句柄指针
  * @param  channel : 定时器通道号
  * @param  pulse   : CCR 比较值
  * @note   自动 clamp 到 [0, max_pulse]
  * @retval PWM_Status
  */
PWM_Status PWM_SetCompare(PWM_Handle *pwm, uint32_t channel, uint32_t pulse);

/**
  * @brief  三相 PWM 同步更新 (FOC 控制主力接口)
  * @param  pwm  : PWM 句柄指针
  * @param  cmp1 : CH1 比较值
  * @param  cmp2 : CH2 比较值
  * @param  cmp3 : CH3 比较值
  * @note   三相比较值在同一个函数写入，减少寄存器访问间隔
  * @retval PWM_Status
  */
PWM_Status PWM_SetThreePhase(PWM_Handle *pwm, uint32_t cmp1, uint32_t cmp2, uint32_t cmp3);

/**
  * @brief  设置死区时间
  * @param  pwm : PWM 句柄指针
  * @param  ns  : 死区时间 (纳秒)
  * @note   受限于定时器时钟分辨率，实际值可能被量化
  * @retval PWM_Status
  */
PWM_Status PWM_SetDeadtime(PWM_Handle *pwm, uint32_t ns);

/**
  * @brief  设置安全占空比上限
  * @param  pwm      : PWM 句柄指针
  * @param  fraction : 最大占空比 (0.0f ~ 1.0f, 默认 0.95f)
  * @retval PWM_Status
  */
PWM_Status PWM_SetMaxDuty(PWM_Handle *pwm, float fraction);

/**
  * @brief  读取当前通道比较值
  * @param  pwm     : PWM 句柄指针
  * @param  channel : 定时器通道号
  * @retval 当前 CCR 值
  */
uint32_t PWM_GetCompare(PWM_Handle *pwm, uint32_t channel);

#endif
