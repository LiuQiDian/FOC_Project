/**
 ******************************************************************************
 * @file    pwm.c
 * @brief   PWM 驱动封装 — 面向 FOC 电机控制
 * @author  Liu_xiyang
 * @date    2026-05-08
 ******************************************************************************
 * @attention
 *
 * 封装 TIM1 三相 PWM 输出，提供安全钳位、死区配置、紧急制动。
 * 所有占空比写入函数自动 clamp 到安全范围，防止上下管直通。
 *
 ******************************************************************************
 */

#include "pwm.h"

/* ===================== 内部辅助 ===================== */

/** @brief 按通道号获取对应的 HAL CCR 寄存器指针 */
static __IO uint32_t *get_ccr_ptr(TIM_HandleTypeDef *htim, uint32_t channel)
{
    switch (channel) {
        case TIM_CHANNEL_1: return &htim->Instance->CCR1;
        case TIM_CHANNEL_2: return &htim->Instance->CCR2;
        case TIM_CHANNEL_3: return &htim->Instance->CCR3;
        case TIM_CHANNEL_4: return &htim->Instance->CCR4;
        default:            return NULL;
    }
}

/** @brief 钳位比较值到安全范围 */
static inline uint32_t clamp_pulse(uint32_t val, uint32_t max)
{
    return val > max ? max : val;
}

/* ===================== API 实现 ===================== */

/**
  * @brief  绑定 HAL 定时器句柄并初始化 PWM 模块
  * @param  pwm  : PWM 句柄指针
  * @param  htim : CubeMX 生成的定时器句柄
  * @retval PWM_Status
  */
PWM_Status PWM_Init(PWM_Handle *pwm, TIM_HandleTypeDef *htim)
{
    if (!pwm || !htim) return PWM_ERR_NULL_HANDLE;

    pwm->htim      = htim;
    pwm->period    = htim->Init.Period + 1;
    pwm->max_pulse = (uint32_t)(pwm->period * 0.95f);
    pwm->started   = 0;

    return PWM_OK;
}

/**
  * @brief  启动所有 PWM 通道输出
  * @param  pwm : PWM 句柄指针
  * @retval PWM_Status
  */
PWM_Status PWM_Start(PWM_Handle *pwm)
{
    if (!pwm || !pwm->htim) return PWM_ERR_NULL_HANDLE;

    /* 清零所有比较值，确保启动时输出为低 */
    HAL_TIM_PWM_Start(pwm->htim, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(pwm->htim, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(pwm->htim, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(pwm->htim, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(pwm->htim, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(pwm->htim, TIM_CHANNEL_3);

    pwm->started = 1;
    return PWM_OK;
}

/**
  * @brief  停止所有 PWM 通道输出 (输出强制低)
  * @param  pwm : PWM 句柄指针
  * @retval PWM_Status
  */
PWM_Status PWM_Stop(PWM_Handle *pwm)
{
    if (!pwm || !pwm->htim) return PWM_ERR_NULL_HANDLE;

    /* 先清零比较值再关通道，避免停在非零电平 */
    PWM_SetThreePhase(pwm, 0, 0, 0);

    HAL_TIMEx_PWMN_Stop(pwm->htim, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(pwm->htim, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(pwm->htim, TIM_CHANNEL_3);
    HAL_TIM_PWM_Stop(pwm->htim, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(pwm->htim, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(pwm->htim, TIM_CHANNEL_3);

    pwm->started = 0;
    return PWM_OK;
}

/**
  * @brief  紧急制动 — 软件触发刹车
  * @param  pwm : PWM 句柄指针
  * @note   直接置位 MOE 关闭，硬件级响应，不依赖软件循环
  * @retval PWM_Status
  */
PWM_Status PWM_EmergencyBrake(PWM_Handle *pwm)
{
    if (!pwm || !pwm->htim) return PWM_ERR_NULL_HANDLE;

    /*
     * 软件刹车 (STM32F4):
     *   1. 清零 MOE → 关闭所有互补输出
     *   2. 强制各通道输出为空闲态 (Idle State)
     *   比直接关通道更快，硬件响应
     */
    pwm->htim->Instance->BDTR &= ~TIM_BDTR_MOE;

    pwm->started = 0;
    return PWM_OK;
}

/**
  * @brief  设置单通道占空比 (0.0 ~ 1.0)
  * @param  pwm     : PWM 句柄指针
  * @param  channel : 定时器通道号
  * @param  percent : 占空比百分比
  * @retval PWM_Status
  */
PWM_Status PWM_SetDutyPercent(PWM_Handle *pwm, uint32_t channel, float percent)
{
    if (!pwm || !pwm->htim) return PWM_ERR_NULL_HANDLE;

    __IO uint32_t *ccr = get_ccr_ptr(pwm->htim, channel);
    if (!ccr) return PWM_ERR_INVALID_CHANNEL;

    uint32_t pulse = (uint32_t)(percent * (float)pwm->period);
    *ccr = clamp_pulse(pulse, pwm->max_pulse);

    return PWM_OK;
}

/**
  * @brief  设置单通道比较值 (CCR)
  * @param  pwm     : PWM 句柄指针
  * @param  channel : 定时器通道号
  * @param  pulse   : 比较值
  * @retval PWM_Status
  */
PWM_Status PWM_SetCompare(PWM_Handle *pwm, uint32_t channel, uint32_t pulse)
{
    if (!pwm || !pwm->htim) return PWM_ERR_NULL_HANDLE;

    __IO uint32_t *ccr = get_ccr_ptr(pwm->htim, channel);
    if (!ccr) return PWM_ERR_INVALID_CHANNEL;

    if (pulse > pwm->period) return PWM_ERR_DUTY_OVERFLOW;

    *ccr = clamp_pulse(pulse, pwm->max_pulse);
    return PWM_OK;
}

/**
  * @brief  三相 PWM 同步更新 (FOC 主力接口)
  * @param  pwm  : PWM 句柄指针
  * @param  cmp1 : CH1 比较值
  * @param  cmp2 : CH2 比较值
  * @param  cmp3 : CH3 比较值
  * @note   连续写三个 CCR，最小化更新间隔
  * @retval PWM_Status
  */
PWM_Status PWM_SetThreePhase(PWM_Handle *pwm, uint32_t cmp1, uint32_t cmp2, uint32_t cmp3)
{
    if (!pwm || !pwm->htim) return PWM_ERR_NULL_HANDLE;

    TIM_TypeDef *tim = pwm->htim->Instance;
    uint32_t max = pwm->max_pulse;

    tim->CCR1 = clamp_pulse(cmp1, max);
    tim->CCR2 = clamp_pulse(cmp2, max);
    tim->CCR3 = clamp_pulse(cmp3, max);

    return PWM_OK;
}

/**
  * @brief  设置死区时间
  * @param  pwm : PWM 句柄指针
  * @param  ns  : 死区时间 (纳秒)
  * @note   168MHz 主频 → 死区分辨率 1/168MHz ≈ 5.95ns
  * @retval PWM_Status
  */
PWM_Status PWM_SetDeadtime(PWM_Handle *pwm, uint32_t ns)
{
    if (!pwm || !pwm->htim) return PWM_ERR_NULL_HANDLE;

    /*
     * 死区计算:
     *   DTG[7:5] 决定死区发生器时钟分频 (tDTS):
     *     0xx → tDTS
     *     10x → 2×tDTS
     *     110 → 8×tDTS
     *     111 → 16×tDTS
     *   这里使用最简单的 0xx 分频 (tDTS = 1/168MHz ≈ 5.95ns)
     */
    uint32_t tDTS_ns = 1000000000UL / PWM_TIM1_CLOCK_HZ;     // ≈ 5.95ns
    uint32_t dtg     = ns / tDTS_ns;                         // DTG[7:0]

    if (dtg > 127) dtg = 127;                                // 最大 127×5.95≈756ns

    uint32_t bdtr = pwm->htim->Instance->BDTR;
    bdtr &= ~TIM_BDTR_DTG;                                   // 清除 DTG[7:0]
    bdtr |= (dtg & 0x7F);                                    // 写入新死区值
    pwm->htim->Instance->BDTR = bdtr;

    return PWM_OK;
}

/**
  * @brief  设置安全占空比上限
  * @param  pwm      : PWM 句柄指针
  * @param  fraction : 最大占空比 (0.0f ~ 1.0f)
  * @retval PWM_Status
  */
PWM_Status PWM_SetMaxDuty(PWM_Handle *pwm, float fraction)
{
    if (!pwm || !pwm->htim) return PWM_ERR_NULL_HANDLE;
    if (fraction > 1.0f) fraction = 1.0f;
    if (fraction < 0.0f) fraction = 0.0f;

    pwm->max_pulse = (uint32_t)(fraction * (float)pwm->period);
    return PWM_OK;
}

/**
  * @brief  读取当前通道比较值
  * @param  pwm     : PWM 句柄指针
  * @param  channel : 定时器通道号
  * @retval 当前 CCR 值
  */
uint32_t PWM_GetCompare(PWM_Handle *pwm, uint32_t channel)
{
    if (!pwm || !pwm->htim) return 0;

    switch (channel) {
        case TIM_CHANNEL_1: return pwm->htim->Instance->CCR1;
        case TIM_CHANNEL_2: return pwm->htim->Instance->CCR2;
        case TIM_CHANNEL_3: return pwm->htim->Instance->CCR3;
        case TIM_CHANNEL_4: return pwm->htim->Instance->CCR4;
        default:            return 0;
    }
}
