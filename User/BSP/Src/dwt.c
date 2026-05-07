/**
 ******************************************************************************
 * @file	bsp_dwt.c
 * @version V1.1.0
 * @brief   DWT (Data Watchpoint and Trace) 周期计数器驱动
 * @note    DWT计数器是Cortex-M内核中的一个32位寄存器(CYCCNT)，
 *          每个CPU时钟周期加一，提供最高精度的时间测量。
 *          常用于高精度延时和时间戳获取。
 ******************************************************************************
 * @attention
 *
 ******************************************************************************
 */
#include "dwt.h"

DWT_Time_t SysTime;                                     // 全局时间结构体，存储s, ms, us
static uint32_t CPU_FREQ_Hz, CPU_FREQ_Hz_ms, CPU_FREQ_Hz_us; // CPU频率，分别以Hz, ms, us为单位
static uint32_t CYCCNT_RountCount;                      // 32位CYCCNT寄存器的溢出次数
static uint32_t CYCCNT_LAST;                            // 上一次读取的CYCCNT值，用于判断溢出
uint64_t CYCCNT64;                                      // 64位的总周期数
static void DWT_CNT_Update(void);                       // 内部函数，用于更新溢出计数

/**
 * @brief  初始化DWT外设
 * @param  CPU_Freq_mHz: CPU核心频率，单位MHz
 * @retval None
 */
void DWT_Init(uint32_t CPU_Freq_mHz)
{
    /* 使能DWT外设 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* DWT CYCCNT寄存器计数清0 */
    DWT->CYCCNT       = (uint32_t)0u;

    /* 使能Cortex-M DWT CYCCNT寄存器 */
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
    CPU_FREQ_Hz       = CPU_Freq_mHz * 1000000;
    CPU_FREQ_Hz_ms    = CPU_FREQ_Hz / 1000;
    CPU_FREQ_Hz_us    = CPU_FREQ_Hz / 1000000;
    CYCCNT_RountCount = 0;
}

/**
 * @brief  获取两次调用之间的时间差 (float, 单位: 秒)
 * @param  cnt_last: 指向一个uint32_t变量的指针，用于存储上一次的计数值
 * @retval float: 时间差，单位秒
 * @note   通过指针传入上一次的计数值，函数会自动更新它
 */
float DWT_GetDeltaT(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    float dt = ((uint32_t)(cnt_now - *cnt_last)) / ((float)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;

    DWT_CNT_Update();

    return dt;
}

/**
 * @brief  获取两次调用之间的时间差 (double, 单位: 秒)
 * @param  cnt_last: 指向一个uint32_t变量的指针，用于存储上一次的计数值
 * @retval double: 时间差，单位秒 (双精度)
 * @note   与DWT_GetDeltaT功能相同，但使用double以获得更高精度
 */
double DWT_GetDeltaT64(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    double dt = ((uint32_t)(cnt_now - *cnt_last)) / ((double)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;

    DWT_CNT_Update();

    return dt;
}

/**
 * @brief  更新全局的系统时间结构体 SysTime
 * @param  None
 * @retval None
 * @note   将64位的总周期数转换为秒、毫秒、微秒的组合
 */
void DWT_SysTimeUpdate(void)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    static uint64_t CNT_TEMP1, CNT_TEMP2, CNT_TEMP3;

    DWT_CNT_Update();

    CYCCNT64   = (uint64_t)CYCCNT_RountCount * (uint64_t)UINT32_MAX + (uint64_t)cnt_now;
    CNT_TEMP1  = CYCCNT64 / CPU_FREQ_Hz;
    CNT_TEMP2  = CYCCNT64 - CNT_TEMP1 * CPU_FREQ_Hz;
    SysTime.s  = CNT_TEMP1;
    SysTime.ms = CNT_TEMP2 / CPU_FREQ_Hz_ms;
    CNT_TEMP3  = CNT_TEMP2 - SysTime.ms * CPU_FREQ_Hz_ms;
    SysTime.us = CNT_TEMP3 / CPU_FREQ_Hz_us;
}

/**
 * @brief  获取从DWT初始化开始经过的时间 (float, 单位: 秒)
 * @param  None
 * @retval float: 总时间，单位秒
 */
float DWT_GetTimeline_s(void)
{
    DWT_SysTimeUpdate();

    float DWT_Timelinef32 = SysTime.s + SysTime.ms * 0.001f + SysTime.us * 0.000001f;

    return DWT_Timelinef32;
}

/**
 * @brief  获取从DWT初始化开始经过的时间 (float, 单位: 毫秒)
 * @param  None
 * @retval float: 总时间，单位毫秒
 */
float DWT_GetTimeline_ms(void)
{
    DWT_SysTimeUpdate();

    float DWT_Timelinef32 = SysTime.s * 1000 + SysTime.ms + SysTime.us * 0.001f;

    return DWT_Timelinef32;
}

/**
 * @brief  获取从DWT初始化开始经过的时间 (uint64_t, 单位: 微秒)
 * @param  None
 * @retval uint64_t: 总时间，单位微秒
 */
uint64_t DWT_GetTimeline_us(void)
{
    DWT_SysTimeUpdate();

    uint64_t DWT_Timelinef32 = SysTime.s * 1000000 + SysTime.ms * 1000 + SysTime.us;

    return DWT_Timelinef32;
}

/**
 * @brief  (内部函数) 更新32位计数器的溢出次数
 * @param  None
 * @retval None
 * @note   当检测到当前计数值小于上一次计数值时，判定为溢出，溢出计数器加一
 */
static void DWT_CNT_Update(void)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;

    if (cnt_now < CYCCNT_LAST)
        CYCCNT_RountCount++; //溢出

    CYCCNT_LAST = cnt_now;
}

/**
 * @brief  使用DWT进行忙等待延时 (单位: 秒)
 * @param  Delay: 需要延时的时间，单位秒
 * @retval None
 * @attention 这是一个阻塞式延时，会100%占用CPU。
 *            在RTOS任务中应避免使用，改用vTaskDelay()。
 *            仅适用于裸机、中断关闭的临界区或RTOS启动前的短延时。
 */
void DWT_Delay(float Delay)
{
    uint32_t tickstart = DWT->CYCCNT;
    float wait = Delay;

    while ((DWT->CYCCNT - tickstart) < wait * (float)CPU_FREQ_Hz)
    {
    }
}
