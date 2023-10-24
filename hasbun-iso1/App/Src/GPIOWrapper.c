#include "GPIOWrapper.h"
#include "stm32f4xx_hal.h"


void gpioSetLevel(uint16_t pin, uint32_t port, bool value) {

    if(value)
          HAL_GPIO_WritePin((GPIO_TypeDef*) port, pin, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin((GPIO_TypeDef*) port, pin, GPIO_PIN_RESET);
}



bool gpioGetLevel(uint16_t pin, uint32_t port) {
    /* Buttons are using this function with negative logic */
    return !HAL_GPIO_ReadPin((GPIO_TypeDef*) port, pin);
}
