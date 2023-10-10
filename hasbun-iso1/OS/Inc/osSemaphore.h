#ifndef INC_OSSEMAPHORE_H
#define INC_OSSEMAPHORE_H

#include <stdint.h>
#include <stdbool.h>
#include "osKernel.h"


typedef struct {
	bool isTook;
	osTaskObject * waitingTask[MAX_NUMBER_TASK];
	uint32_t numWaitingTasks;
} osSemaphoreObject;

/**
 * @brief Initializes semaphore binary or not.
 *
 * @param[in,out]   semaphore   Semaphore handler.
 * @param[in]       maxCount    Maximum count value that can be reached.
 * @param[in]       count       The count value assigned to the semaphore when it is created.
 */
void osSemaphoreInit(osSemaphoreObject* semaphore, const uint32_t maxCount, const uint32_t count);

/**
 * @brief Take semaphore.
 *
 * Inside interrupts, blocks the underlying running task.
 * If idle task is running, semaphore is taken but no blocking any task.
 *
 * @param[in,out]   semaphore   Semaphore handler.
 *
 * @return Returns true if the semaphore could be taken.
 */
bool osSemaphoreTake(osSemaphoreObject* semaphore);

/**
 * @brief Give semaphore.
 *
 * @param[in,out]   semaphore   Semaphore handler.
 */
void osSemaphoreGive(osSemaphoreObject* semaphore);


#endif // INC_OSSEMAPHORE_H
