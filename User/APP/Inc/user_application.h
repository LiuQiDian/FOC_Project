#ifndef USER_APPLICATION_H
#define USER_APPLICATION_H

#include "main.h"
#include "vofa.h"
#include "pwm.h"
#include "mt6701.h"
#include "cmsis_os.h"
#include "foc.h"


#define VOFA_UART &huart1

void user_app(void);

#endif /* USER_APPLICATION_H */