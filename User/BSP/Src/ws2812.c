// /*
//  * @Author: Liu_xiyang 3230643253@qq.com
//  * @Date: 2026-05-08 20:45:33
//  * @LastEditors: Liu_xiyang 3230643253@qq.com
//  * @LastEditTime: 2026-05-08 20:47:43
//  * @FilePath: \FOC_Project\User\BSP\Src\ws2812.c
//  * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
//  */

// #include "ws2812.h"


// // WS2812 时序参数
// // WS2812需要800kHz的PWM频率 (1.25μs周期)
// // 0码: 高电平0.4μs, 低电平0.85μs (占空比约32%)
// // 1码: 高电平0.8μs, 低电平0.45μs (占空比约64%)
// #define WS2812_PWM_FREQ 800000  // 800kHz

// // PWM数据缓冲区
// static uint16_t pwm_buf[WS2812_DATA_LEN] = {0};

// // PWM对象
// static Pwm pwm(DEVICE_PWM_3); // 使用PWM设备3 (TIM3)

// // 用于存储CODE_0和CODE_1的值 (根据实际ARR动态计算)
// static uint32_t CODE_0 = 0;
// static uint32_t CODE_1 = 0;

// /**
//  * @brief WS2812初始化函数
//  * @note 初始化PWM,设置频率和占空比参数
//  */
// void ws2812_init(void)
// {
//     // 初始化PWM,设置800kHz频率
//     auto initResult = pwm.Init(WS2812_PWM_FREQ);
//     if (!initResult.ok())
//     {
//         // 初始化失败处理
//         return;
//     }

//     // 获取ARR值以计算占空比
//     auto arrResult = pwm.GetARR();
//     if (!arrResult.ok())
//     {
//         return;
//     }
//     uint32_t arr = arrResult.value;

//     // 使用 (ARR+1) 计算占空比，接近 1/3 与 2/3，更稳健贴合 WS2812 公差
//     uint32_t period = arr + 1U;
//     uint32_t c0 = (period * 1U) / 3U; // 约 33%
//     uint32_t c1 = (period * 2U) / 3U; // 约 66%
//     // 夹取到有效范围 [1, ARR]
//     CODE_0 = (c0 == 0U) ? 1U : (c0 > arr ? arr : c0);
//     CODE_1 = (c1 == 0U) ? 1U : (c1 > arr ? arr : c1);

//     // 初始化PWM缓冲区为全0 (复位信号)
//     for (int i = 0; i < WS2812_DATA_LEN; i++)
//     {
//         pwm_buf[i] = 0;
//     }

//     // 启动PWM通道1
//     pwm.Start(Pwm::CHANNEL_1);
// }

// /**
//  * @brief 设置单个LED的颜色
//  * @param led_id LED编号 (0~LED_NUM-1)
//  * @param r 红色值 (0~255)
//  * @param g 绿色值 (0~255)
//  * @param b 蓝色值 (0~255)
//  * @note WS2812的颜色顺序是GRB
//  */
// void ws2812_set_color(uint16_t led_id, uint8_t r, uint8_t g, uint8_t b)
// {
//     if (led_id >= LED_NUM)
//     {
//         return; // 超出范围
//     }

//     // WS2812的颜色顺序是GRB
//     uint32_t color = (g << 16) | (r << 8) | b;
//     uint16_t *p = &pwm_buf[led_id * 24];

//     // 将24位颜色数据转换为PWM占空比
//     for (int i = 23; i >= 0; i--)
//     {
//         if ((color >> i) & 1)
//         {
//             *p = CODE_1; // 1码
//         }
//         else
//         {
//             *p = CODE_0; // 0码
//         }
//         p++;
//     }
// }



// // 精确发送：在每个 800kHz 周期边界更新 CCR，确保 WS2812 位时序稳定
// static void ws2812_show_precise()
// {
//     // 使用封装的 Pwm 类启动通道（若已经启动，pwm.Start 会快速返回）
//     pwm.Start(Pwm::CHANNEL_1);

//     // 为降低抖动，可临时屏蔽中断
//     uint32_t primask = __get_PRIMASK();
//     __disable_irq();

//     // // 对齐到周期边界后再开始写入，确保首位完整周期
//     pwm.WaitUpdate();

//     // 逐位发送：每次在新的更新周期开始前写入 CCR
//     for (uint16_t i = 0; i < WS2812_DATA_LEN; ++i)
//     {
//         // 通过 Pwm 类设置占空比，保持抽象统一
//         pwm.SetDutyTicks(Pwm::CHANNEL_1, pwm_buf[i]);
//         pwm.WaitUpdate();
//     }

//     // 发送复位：保持低电平 > 50us（约 40 个 1.25us 周期，保守取 80 个）
//     pwm.SetDutyTicks(Pwm::CHANNEL_1, 0);
//     for (int k = 0; k < 100; ++k)
//         pwm.WaitUpdate();

//     if (primask == 0) __enable_irq();
// }

// /**
//  * @brief 将颜色数据发送到LED灯带 (不使用DMA，软件方式)
//  * @note 通过逐位设置PWM占空比并延时来发送数据
//  */
// void ws2812_show(void)
// {
//     // 使用精确时序的发送函数，确保每位严格 1.25us 周期
//     ws2812_show_precise();
// }


// // // 等待 TIM3 更新事件，保证在新周期边界更新 CCR
// // static inline void ws2812_wait_update()
// // {
// //     while (__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE) == RESET) { }
// //     __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
// // }

