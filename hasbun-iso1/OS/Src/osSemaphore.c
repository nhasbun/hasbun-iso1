#include <stddef.h>
#include <stdint.h>

#include "cmsis_gcc.h"
#include "osSemaphore.h"
#include "osKernel.h"



void osSemaphoreInit(osSemaphoreObject* semaphore, const uint32_t maxCount, const uint32_t count) {
    /* TODO implement counting semaphore (count argument ignored for now) */

	if (count)
		semaphore -> isTaked = true;
	else
		semaphore -> isTaked = false;

	semaphore -> waitingTask = NULL;
}


bool osSemaphoreTake(osSemaphoreObject* semaphore) {

    if (__get_IPSR() > 0)     return false;  // interrupt context
	if (semaphore -> isTaked) return false;

	semaphore -> isTaked = true;
	semaphore -> waitingTask = osGetRunningTask();
	semaphore -> waitingTask -> state = OS_TASK_BLOCK;

	/* wait for systick -> scheduler */
	while (semaphore -> isTaked) {
		__WFI();
	}

	return true;
}


void osSemaphoreGive(osSemaphoreObject* semaphore) {

	if (!(semaphore -> isTaked))
		return;

	semaphore -> isTaked = false;

	/* we ignore any other state than being waiting/blocked */
	if (semaphore -> waitingTask -> state == OS_TASK_BLOCK)
	    semaphore -> waitingTask -> state = OS_TASK_READY;

	/* calling osYield is optional here, I think I prefer to just simply wait for next tick */
}
