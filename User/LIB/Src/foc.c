/*
 * @Author: Liu_xiyang 3230643253@qq.com
 * @Date: 2026-05-09 17:07:51
 * @LastEditors: Liu_xiyang 3230643253@qq.com
 * @LastEditTime: 2026-05-13 00:11:45
 * @FilePath: \FOC_Project\User\LIB\Src\foc.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#include "foc.h"
#include "mt6701.h"
#include "pwm.h"
#include "stm32f405xx.h"
#include "vofa.h"
#include <math.h>
#include <stdint.h>


static svpwm_sector_t sector; // 当前扇区，FOC 主体根据电角度计算后更新
static float zero_electrical_angle = 0.0f; // 电角度零点偏移，FOC 主体通过校准函数更新

static void update_motor_angle(void);
static void update_motor_current(void);
static foc_err_t calibrate_electronical_angle();
static foc_err_t set_voltage_phase(float Uq, float Ud, float electrical_angle);

foc_motor_t motor = {
    .s_ops = &mt6701_foc_ops,
    .c_ops = &adc_foc_current_ops,
    .svpwm = &svpwm_foc_ops,

    .pole_pairs = 11,
    .state = FOC_STATE_OPEN_LOOP,

    .electrical_angle = 0.0f,
    .angle_prev = 0.0f,
    .velocity = 0.0f,
    .ia = 0.0f,
    .ib = 0.0f,
    .ic = 0.0f,
    .Ua = 0.0f,
    .Ub = 0.0f,
    .Uc = 0.0f,
    .vd_ref = 0.0f,
    .vq_ref = 0.0f,
    .v_alpha = 0.0f,
    .v_beta = 0.0f,
    .id_ref = 0.0f,
    .iq_ref = 0.0f
};

foc_err_t foc_init(void)
{
    // 1. 初始化传感器接口
    if (mt6701_foc_ops.init() != FOC_OK) {
        return FOC_ERR;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待传感器稳定
    // 2. 初始化电流采样接口
    if (adc_foc_current_ops.init() != FOC_OK) {
        return FOC_ERR;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待 ADC 稳定
    // 3. 初始化 SVPWM 输出接口
    if (motor.svpwm->init() != FOC_OK) {
        return FOC_ERR;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待 SVPWM 稳定
    // DWT_Init(168); // 初始化 DWT 计时器，用于高精度控制环计时
    motor.electrical_angle = 3.14f;
    
    motor.iq_ref = 1.0f; // 设定一个固定的 q 轴电流参考值，代表目标转矩
    motor.id_ref = 0.0f; // d 轴电流参考值保持为零以最大化转矩输出
    return FOC_OK;
}

void motor_control(void)
{
    update_motor_angle(); // 更新电角度
    update_motor_current(); // 更新三相电流
    set_voltage_phase(motor.iq_ref, motor.id_ref, motor.electrical_angle); // 设置输出电压
}

uint32_t constrain(uint32_t val, uint32_t min_val, uint32_t max_val)
{
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

float normalize_angle(float angle)
{
    float a = fmod(angle, 2 * PI);
    return a >= 0 ? a : (a + 2 * PI);
}

float get_angle(float angle)
{
    return normalize_angle(motor.pole_pairs * angle + zero_electrical_angle);
}

static void update_motor_angle(void)
{
    motor.s_ops->read(&motor.electrical_angle);
    motor.electrical_angle = get_angle(motor.electrical_angle);
}

static void update_motor_current(void)
{
    motor.c_ops->sample(&motor.ia, &motor.ib, &motor.ic);
}

static foc_err_t calibrate_electronical_angle(void)
{
    set_voltage_phase(3.0f, 0.0f, 0.0f);

    vTaskDelay(pdMS_TO_TICKS(1000));

    float angleSum = 0;
    int samples = 100;
    for (int i=0; i<samples; i++)
    {
        angleSum += GetAngle(angleRead());
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    float mechanicalAngle_locked = angleSum / samples;
    zero_electrical_angle = normalize_angle(motor.pole_pairs * mechanicalAngle_locked);

    set_voltage_phase(0.0f, 0.0f, 0.0f);

    return FOC_OK;
}

static void inverse_park_transform(float U_d, float U_q, float theta, float *U_alpha, float *U_beta)
{
    *U_alpha = U_d * arm_cos_f32(theta) - U_q * arm_sin_f32(theta);
    *U_beta = U_d * arm_sin_f32(theta) + U_q * arm_cos_f32(theta);
}

static void inverse_clarke_transform(float U_alpha, float U_beta, float *Ua, float *Ub, float *Uc)
{
    *Ua = U_alpha;
    *Ub = (-U_alpha + SQRT3 * U_beta) * (1.0f / 2.0f);
    *Uc = (-U_alpha - SQRT3 * U_beta) * (1.0f / 2.0f);
}

static foc_err_t set_voltage_phase(float Uq, float Ud, float electrical_angle)
{
    inverse_park_transform(Ud, Uq, electrical_angle, &motor.v_alpha, &motor.v_beta);
    inverse_clarke_transform(motor.v_alpha, motor.v_beta, &motor.Ua, &motor.Ub, &motor.Uc);
    return motor.svpwm->set(motor.v_alpha, motor.v_beta, motor.Ua, motor.Ub, motor.Uc);
}

static foc_err_t svpwm_init(void)
{
    if(PWM_Init(&pwmd, pwmd.htim) != PWM_OK) {
        return FOC_ERR;
    }
    PWM_Start(&pwmd);
    return FOC_OK;
}

static foc_err_t svpwm_set(float U_alpha, float U_beta,float Ua, float Ub, float Uc)
{
    float ta = 0.0f;
    float tb = 0.0f;
    float tc = 0.0f;
    float k = (TS * SQRT3) * INVBATVEL;
    // VOFA_Print("%f,%f,%f\n", Ua, Ub, Uc);

    int a = (Ua > 0) ? 1 : 0;
    int b = (Ub > 0) ? 1 : 0;
    int c = (Uc > 0) ? 1 : 0;
    sector = (c << 2) | (b << 1) | a;

    switch (sector)
    {
    case SVPWM_SECTOR_1:
    {
        float t4 = k * Ub;
        float t6 = k * Ua;
        float t0 = (TS - t4 - t6) * 0.5f;

        ta       = t4 + t6 + t0;
        tb       = t6 + t0;
        tc       = t0;
    }
    break;

    case SVPWM_SECTOR_2:
    {
        float t6 = -k * Uc;
        float t2 = -k * Ub;
        float t0 = (TS - t2 - t6) * 0.5f;

        ta       = t6 + t0;      // a相占空比时间计算
        tb       = t2 + t6 + t0; // b相占空比时间计算
        tc       = t0;           // c相占空比时间为t0
    }
    break;

    case SVPWM_SECTOR_3:
    {
        float t2 = k * Ua;
        float t3 = k * Uc;
        float t0 = (TS - t2 - t3) * 0.5f;

        ta       = t0;           // a相占空比时间为t0
        tb       = t2 + t3 + t0; // b相占空比时间计算
        tc       = t3 + t0;      // c相占空比时间计算
    }
    break;

    case SVPWM_SECTOR_4:
    {
        float t1 = -k * Ua;
        float t3 = -k * Ub;
        float t0 = (TS - t1 - t3) * 0.5f;

        ta       = t0;           // a相占空比时间为t0
        tb       = t3 + t0;      // b相占空比时间计算
        tc       = t1 + t3 + t0; // c相占空比时间计算
    }
    break;

    case SVPWM_SECTOR_5:
    {
        float t1 = k * Uc;
        float t5 = k * Ub;
        float t0 = (TS - t1 - t5) * 0.5f;

        ta       = t5 + t0;      // a相占空比时间计算
        tb       = t0;           // b相占空比时间为t0
        tc       = t1 + t5 + t0; // c相占空比时间计算
    }
    break;

    case SVPWM_SECTOR_6:
    {
        float t4 = -k * Uc;
        float t5 = -k * Ua;
        float t0 = (TS - t4 - t5) * 0.5f;

        ta       = t4 + t5 + t0; // a相占空比时间计算
        tb       = t0;           // b相占空比时间为t0
        tc       = t5 + t0;      // c相占空比时间计算
    }
    break;

    default:
        break;
    }
    
    ta=(uint32_t)(ta * (float)pwmd.period);
    tb=(uint32_t)(tb * (float)pwmd.period);
    tc=(uint32_t)(tc * (float)pwmd.period);
    ta = constrain(ta, 0, pwmd.period);
    tb = constrain(tb, 0, pwmd.period);
    tc = constrain(tc, 0, pwmd.period);
    
    PWM_SetThreePhase(&pwmd, ta, tb, tc);
    return FOC_OK;
}

const foc_svpwm_ops_t svpwm_foc_ops = {
    .init = svpwm_init,
    .set = svpwm_set,
};

static foc_err_t mt6701_init(void)
{
    MT6701_DMA_Init();
    return FOC_OK;
}

static foc_err_t mt6701_read(float *angle)
{
    if (angle == NULL) {
        return FOC_EINVAL;
    }
    *angle = angleRead();
    return FOC_OK;
}

const foc_sensor_ops_t mt6701_foc_ops = {
    .init = mt6701_init,
    .read = mt6701_read,
};

static foc_err_t adc_init(void)
{
    ADC_Init(&hadc1, &adc_data);
    calibrate_electronical_angle(); // 校准电角度零点
    __HAL_ADC_ENABLE_IT(&hadc1, ADC_IT_JEOC);
    HAL_ADCEx_InjectedStart_IT(&hadc1);
    return FOC_OK;
}

static foc_err_t adc_read(float *ia, float *ib, float *ic)
{
    if (ia == NULL || ib == NULL || ic == NULL) {
        return FOC_EINVAL;
    }
    UpdateCurrent(&hadc1, &adc_data);
    *ia = adc_data.Ia;
    *ib = adc_data.Ib;
    *ic = adc_data.Ic;
    return FOC_OK;
}

const foc_current_ops_t adc_foc_current_ops = {
    .init = adc_init,
    .sample = adc_read,
};

