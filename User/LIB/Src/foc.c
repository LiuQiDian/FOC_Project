/*
 * @Author: Liu_xiyang 3230643253@qq.com
 * @Date: 2026-05-09 17:07:51
 * @LastEditors: Liu_xiyang 3230643253@qq.com
 * @LastEditTime: 2026-05-10 22:31:56
 * @FilePath: \FOC_Project\User\LIB\Src\foc.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "foc.h"


//   关键设计原则：一个 struct foc_motor 代表一个电机轴，所有状态都在其中，不依赖全局变量。

//   5. API 生命周期

//   /* 分配/初始化/解构 — Linux 风格两级构造 */
//   struct foc_motor *foc_motor_alloc(void);
//   foc_err_t foc_motor_init(struct foc_motor *m, const struct foc_config *cfg);
//   void     foc_motor_exit(struct foc_motor *m);    // 释放内部资源
//   void     foc_motor_free(struct foc_motor *m);     // 释放结构体本身

//   /* 运行时 */
//   foc_err_t foc_motor_start(struct foc_motor *m);
//   foc_err_t foc_motor_stop(struct foc_motor *m);

//   /* 控制环 — 放在定时器中断或 FreeRTOS 高优先级任务中调用 */
//   foc_err_t foc_current_loop(struct foc_motor *m);  // 电流环 ISR 本体
//   foc_err_t foc_speed_loop(struct foc_motor *m);    // 速度环 (通常在外层任务中)

//   /* 参数调节 */
//   void foc_id_ref_set(struct foc_motor *m, float id_ref);
//   void foc_iq_ref_set(struct foc_motor *m, float iq_ref);

//   foc_motor_alloc / foc_motor_free 对应 Linux 内核里 kzalloc / kfree 的惯用法；foc_motor_init / foc_motor_exit 对应 open /
//   release。两级构造函数可以分离内存分配和设备初始化。


//   7. 变换模块：纯函数

//   Clark/Park/SVPWM 没有内部状态，最适合写成无副作用的纯函数：

//   /* foc_transform.h */

//   /* Clark: 三相电流 → αβ */
//   void foc_clarke(float ia, float ib, float ic,
//                   float *i_alpha, float *i_beta);

//   /* Park: αβ + 电角度 → dq */
//   void foc_park(float i_alpha, float i_beta, float theta,
//                 float *i_d, float *i_q);

//   /* 反 Park: dq + 电角度 → αβ */
//   void foc_inv_park(float v_d, float v_q, float theta,
//                     float *v_alpha, float *v_beta);

//   /* SVPWM: αβ 参考电压 → 三相比较值 */
//   foc_err_t foc_svpwm_calc(float v_alpha, float v_beta, float vdc,
//                            uint32_t period, struct foc_pwm_duty *out);




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
    // 电角度校准
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

