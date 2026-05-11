// #ifndef WS2812_H
// #define WS2812_H

// #include "main.h"
// #include "tim.h" // 包含定时器头文件
// // #include "BspPwm.h"
// #include "dwt.h"

// // --- 用户配置 ---
// #define LED_NUM 1 // 你要控制的LED数量
// #define USE_FREERTOS 1 // 如果使用FreeRTOS，请置1

// // --- 驱动内部定义 ---
// #define PWM_DATA_SIZE (24 * LED_NUM) // 每个LED需要24个bit
// #define RESET_PULSE_LEN 50          // 复位信号需要的低电平PWM个数
// #define WS2812_DATA_LEN (PWM_DATA_SIZE + RESET_PULSE_LEN)

// /**
//  * @brief 初始化WS2812驱动
//  * @note 初始化PWM，设置800kHz频率
//  */
// void ws2812_init(void);

// /**
//  * @brief 设置单个LED的颜色
//  * @param led_id LED的编号，从0开始
//  * @param r Red (0-255)
//  * @param g Green (0-255)
//  * @param b Blue (0-255)
//  */
// void ws2812_set_color(uint16_t led_id, uint8_t r, uint8_t g, uint8_t b);

// /**
//  * @brief 将颜色数据发送到LED灯带
//  * @note 软件方式发送，会阻塞约1ms
//  */
// void ws2812_show(void);

// void ws2812_test_raw(uint8_t g, uint8_t r, uint8_t b);

// #endif // WS2812_H
