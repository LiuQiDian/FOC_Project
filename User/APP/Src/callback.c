#include "callback.h"

TaskHandle_t foc_task_handle = NULL;

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc == &hadc1) {

  }
}