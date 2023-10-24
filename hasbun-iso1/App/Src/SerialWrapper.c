#include <string.h>
#include "SerialWrapper.h"
#include "stm32f4xx_hal.h"
#include "osKernel.h"


extern UART_HandleTypeDef huart3;


void serialPrint(char* buffer) {
    osEnterCriticalSection();
    HAL_UART_Transmit(&huart3, (uint8_t*)buffer, strlen(buffer), 1000);
    osExitCriticalSection();
}
