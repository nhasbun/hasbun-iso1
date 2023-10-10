#include <stddef.h>
#include <stdint.h>

#include "cmsis_gcc.h"
#include "osSemaphore.h"
#include "osKernel.h"


void osSemaphoreInit(osSemaphoreObject* semaphore, const uint32_t maxCount, const uint32_t count) {
    /* TODO implement counting semaphore (count argument ignored for now) */

	if (!count)
		semaphore -> isTook = true;
	else
		semaphore -> isTook = false;

	for (int i  = 0; i < MAX_NUMBER_TASK; i++) semaphore->waitingTask[i] = NULL;

	semaphore -> numWaitingTasks = 0;
}


bool osSemaphoreTake(osSemaphoreObject* semaphore) {

    if (semaphore == NULL) return false;

	/* if semaphore is took we block the task */
    if (semaphore->isTook) {

        osTaskObject * thisTask = osGetRunningTask();

        /* not blocking idle task */
        if (thisTask != NULL) {

            semaphore -> waitingTask[semaphore->numWaitingTasks] = thisTask;
            thisTask -> state = OS_TASK_BLOCK;
            semaphore -> numWaitingTasks++;

            /* calling the scheduler */
            osYield();
        }
	}

    semaphore -> isTook = true;

	return true;
}


void osSemaphoreGive(osSemaphoreObject* semaphore) {

	if (!(semaphore -> isTook))
		return;

	/* with no waiting tasks, we clear the semaphore */
	if (semaphore -> numWaitingTasks == 0)
	    semaphore -> isTook = false;

	/* we ignore any other state than being waiting/blocked */
	else if (semaphore -> waitingTask[0] -> state == OS_TASK_BLOCK) {

	    /* allow waiting task to run */
	    semaphore -> waitingTask[0] -> state = OS_TASK_READY;
	    semaphore -> numWaitingTasks--;

	    /* re-ordering waiting tasks */
	    for (int i = 0; i < semaphore->numWaitingTasks; i++) {
	        semaphore->waitingTask[i] = semaphore->waitingTask[i+1];
	    }
	}


	/* calling osYield is optional here, I think I prefer to just simply wait for next tick */
}
