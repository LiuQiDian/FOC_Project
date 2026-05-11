#ifndef FOC_H
#define FOC_H

#include "main.h"
#include "mt6701.h"
#include "foc_adc.h"

typedef enum {
    FOC_OK      =  0,
    FOC_ERR     = -1,          // 通用错误
    FOC_EINVAL  = -2,          // 参数非法
    FOC_EBUSY   = -3,          // 设备忙
    FOC_EOVERFLOW = -4,        // 溢出
    FOC_ETIMEDOUT = -5,        // 超时
} foc_err_t;

typedef struct foc_sensor_ops {
    foc_err_t (*init)(void);
    foc_err_t (*read)(float *angle);
} foc_sensor_ops_t;

typedef struct foc_current_ops {
    foc_err_t (*init)(void);
    foc_err_t (*sample)(float *ia, float *ib, float *ic);
} foc_current_ops_t;

typedef struct foc_motor {
    /* --- 底层硬件抽象 (通过 ops 注入) --- */
    const foc_sensor_ops_t  *s_ops;
    const foc_current_ops_t *c_ops;
    struct foc_svpwm        *svpwm;      // PWM 输出


    /* --- 运行状态 --- */
    float   electrical_angle;           // 电角度 (rad)
    float   angle_prev;                 // 上一帧角度 (用于转速计算)
    float   velocity;                   // 当前转速 (机械 rad/s)
    float   ia, ib, ic;                 // 三相电流瞬时值
    float   vd_ref, vq_ref;             // dq 参考电压
    float   id_ref, iq_ref;             // dq 参考电流 (速度环输出 or 直接给定)

    uint8_t pole_pairs;                 // 极对数 (电角度 = 机械角度 × pole_pairs)
    uint8_t state;                      // STOP / OPEN_LOOP / CLOSED_LOOP / FAULT
}foc_motor_t;

#endif /* FOC_H */