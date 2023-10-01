#include "app.h"
#include "osKernel.h"
#include "stm32f4xx_hal.h"


#define REPETITIVE_TASK_MS(MS, X)  do { \
    static volatile uint32_t init = -1; \
    if (init == -1) {init = HAL_GetTick();} \
    if (HAL_GetTick() - init > MS) { X; init = HAL_GetTick();} \
} while (0);\



// File scope variables
static osTaskObject osTask1;
static osTaskObject osTask2;
static osTaskObject osTask3;


// File scope methods
static void task1();
static void task2();
static void task3();



void app_main() {
    osTaskCreate(&osTask1, OS_HIGH_PRIORITY,   task1);
    osTaskCreate(&osTask2, OS_NORMAL_PRIORITY, task2);
    osTaskCreate(&osTask3, OS_LOW_PRIORITY,    task3);

    osStart();


    while (1)
    {
        __WFI();
    }
}



static void task1() {
    uint32_t i = 0;

    while(1)
    {
        i++;
        osDelay(500);
    }
}


static void task2() {
    uint32_t j = 0;

    while(1)
    {
        j++;
        osDelay(1000);
    }
}


static void task3() {
    uint32_t k = 0;

    while(1)
    {
        k++;
    }
}

