#include "app.h"
#include "osKernel.h"
#include "osSemaphore.h"
#include "osQueue.h"
#include "osIRQ.h"
#include "stm32f4xx_hal.h"
#include <stdlib.h>


#define USER_Btn_Pin GPIO_PIN_13
#define REPETITIVE_TASK_MS(MS, X)  do { \
    static volatile uint32_t init = -1; \
    if (init == -1) {init = HAL_GetTick();} \
    if (HAL_GetTick() - init > MS) { X; init = HAL_GetTick();} \
} while (0);\


// File scope variables
static osTaskObject osTask1;
static osTaskObject osTask2;
static osTaskObject osTask3;
static osSemaphoreObject semaphore1;
static osSemaphoreObject semaphore2;
static osQueueObject queue1;
static osQueueObject queue2;

static bool buttonPressed = false;


// File scope methods
static void task1();
static void task2();
static void task3();
static void externalInterrupt10_15(void * data);



void app_main() {
    osTaskCreate(&osTask1, OS_HIGH_PRIORITY,   task1);
    osTaskCreate(&osTask2, OS_NORMAL_PRIORITY, task2);
    osTaskCreate(&osTask3, OS_LOW_PRIORITY,    task3);

    osSemaphoreInit(&semaphore1, 0, 1);
    osSemaphoreInit(&semaphore2, 0, 1);
    osQueueInit(&queue1, sizeof(uint32_t));
    osQueueInit(&queue2, sizeof(int));

    while(!osRegisterIRQ(EXTI15_10_IRQn, externalInterrupt10_15, (void*) &buttonPressed));

    osStart();

    while (1)
    {
        __WFI();
    }
}



static void task1() {
    uint32_t i = 0;
    uint32_t buffer[32] = {0};

    while(1)
    {
        i++;
        osSemaphoreTake(&semaphore1);
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    }
}


static void task2() {
    uint32_t j = 0;
    int buffer = 0;

    while(1)
    {
        j++;
        osSemaphoreTake(&semaphore1);
        HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
    }
}


static void task3() {
    uint32_t k = 0;
    uint32_t num64 = 0;
    int _num = 0;

    while(1)
    {
        k++;
        osDelay(1000);
        osSemaphoreGive(&semaphore1);
    }
}


static void externalInterrupt10_15(void * data) {
    bool * buttonPressed = (bool*) data;
    *buttonPressed = true;

    /* Should be ignored */
    osDelay(0xffff);

    osSemaphoreGive(&semaphore1);

    __HAL_GPIO_EXTI_CLEAR_FLAG(USER_Btn_Pin);
}

