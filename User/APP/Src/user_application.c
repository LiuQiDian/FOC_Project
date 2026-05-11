/*
 * @Author: Liu_xiyang 3230643253@qq.com
 * @Date: 2026-05-08 19:25:47
 * @LastEditors: Liu_xiyang 3230643253@qq.com
 * @LastEditTime: 2026-05-09 17:06:07
 * @FilePath: \FOC_Project\User\APP\Src\user_application.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
  ******************************************************************************
  * @file           : user_application.c
  * @brief          : 用户应用代码
  ******************************************************************************
  * @attention
  *
  * 本文件由 STM32CubeIDE 自动生成，包含用户应用的初始化和任务函数。
  * 用户可以在此文件中添加自己的代码，或创建新的源文件进行开发。
  *
  * 推荐的用户代码结构：
  *   - 初始化函数：用于设置外设、变量等
  *   - 任务函数：FreeRTOS 任务的实现
  *   - 中断处理函数：如果需要，可以在此文件中添加中断服务程序
  *
  * 注意事项：
  *   - 避免修改自动生成的代码块（如 MX_FREERTOS_Init），以免影响代码生成器的功能。
  *   - 使用 FreeRTOS API 时，请确保正确处理任务优先级和资源共享。
  *  - 可以在 user_application.h 中声明需要在多个文件中使用的函数和变量。
  *  ******************************************************************************
  */
 
 #include "user_application.h"


/* ===================== 用户主任务 ===================== */

 void user_app(void)
 {
    VOFA_Init(VOFA_UART);
    MT6701_DMA_Init();

    while (1)
    {
        float angle = angleRead();
        VOFA_Print("Angle: %.2f\n", angle);
        osDelay(100);
    }
 }