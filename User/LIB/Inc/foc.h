#ifndef FOC_H
#define FOC_H

#include "main.h"
#include "arm_math.h" // ARM CMSIS-DSP数学库
#include "mt6701.h"
#include "foc_adc.h"
#include "pwm.h"
#include "dwt.h"
#include "vofa.h"



#define TS                       1.0f                           // 控制周期（归一化到1.0）
#define N_BASE                   3.0f                           // 3S电池
#define BATVEL                   (4.0f * N_BASE)                // 电池电压
#define INVBATVEL                (1.0f / BATVEL)                // 电池电压的倒数
#define SQRT3                    1.73205080756887729352f        // 定义 根号3 常量


typedef enum {
    FOC_OK      =  0,
    FOC_ERR     = -1,          // 通用错误
    FOC_EINVAL  = -2,          // 参数非法
    FOC_EBUSY   = -3,          // 设备忙
    FOC_EOVERFLOW = -4,        // 溢出
    FOC_ETIMEDOUT = -5,        // 超时
} foc_err_t;

typedef enum {
    FOC_STATE_STOP = 0,
    FOC_STATE_OPEN_LOOP,
    FOC_STATE_CLOSED_LOOP,
    FOC_STATE_FAULT,
} motor_state_t;

typedef enum {
    SVPWM_SECTOR_1 = 3,
    SVPWM_SECTOR_2 = 1,
    SVPWM_SECTOR_3 = 5,
    SVPWM_SECTOR_4 = 4,
    SVPWM_SECTOR_5 = 6,
    SVPWM_SECTOR_6 = 2,
}svpwm_sector_t;

typedef struct foc_sensor_ops {
    foc_err_t (*init)(void);
    foc_err_t (*read)(float *angle);
} foc_sensor_ops_t;

typedef struct foc_current_ops {
    foc_err_t (*init)(void);
    foc_err_t (*sample)(float *ia, float *ib, float *ic);
} foc_current_ops_t;

typedef struct foc_svpwm_ops {
    foc_err_t (*init)(void);
    foc_err_t (*set)(float U_alpha, float U_beta,float Ua, float Ub, float Uc);
} foc_svpwm_ops_t;

typedef struct foc_motor {
    /* --- 底层硬件抽象 (通过 ops 注入) --- */
    const foc_sensor_ops_t  *s_ops;
    const foc_current_ops_t *c_ops;
    const foc_svpwm_ops_t   *svpwm;      // PWM 输出


    /* --- 运行状态 --- */
    float   electrical_angle;           // 电角度 (rad)
    float   angle_prev;                 // 上一帧角度 (用于转速计算)
    float   velocity;                   // 当前转速 (机械 rad/s)
    float   ia, ib, ic;                 // 三相电流瞬时值
    float   vd_ref, vq_ref;             // dq 参考电压
    float   v_alpha, v_beta;            // αβ 参考电压
    float   Ua, Ub, Uc;                 // abc 参考电压
    float   id_ref, iq_ref;             // dq 参考电流

    uint8_t pole_pairs;                 // 极对数 (电角度 = 机械角度 × pole_pairs)
    uint8_t state;                      // STOP / OPEN_LOOP / CLOSED_LOOP / FAULT
}foc_motor_t;

extern const foc_sensor_ops_t mt6701_foc_ops;
extern const foc_current_ops_t adc_foc_current_ops;
extern const foc_svpwm_ops_t svpwm_foc_ops;
extern foc_motor_t motor;


extern foc_err_t foc_init(void);
extern void motor_control(void);



#endif /* FOC_H */