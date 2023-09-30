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
    osTaskCreate(&osTask1, task1, NULL);
    osTaskCreate(&osTask2, task2, NULL);
    osTaskCreate(&osTask3, task3, NULL);

    osStart();


    while (1)
    {
        __WFI();
    }
}



static void task1() {
	uint32_t init = -1;
    uint32_t i = 0;

    while(1)
    {
        i++;
        if (init == -1) {init = HAL_GetTick();}
		if (HAL_GetTick() - init > 1000) { HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); init = HAL_GetTick();}
    }
}


static void task2() {
	uint32_t init = -1;
    uint32_t j = 0;

    while(1)
    {
        j++;
        if (init == -1) {init = HAL_GetTick();}
		if (HAL_GetTick() - init > 1500) { HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin); init = HAL_GetTick();}
    }
}


static void task3() {
	uint32_t init = -1;
    uint32_t k = 0;

    while(1)
    {
        k++;
        if (init == -1) {init = HAL_GetTick();}
		if (HAL_GetTick() - init > 1500) { HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin); init = HAL_GetTick();}
    }
}

