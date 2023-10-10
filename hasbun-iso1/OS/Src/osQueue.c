#include <string.h>
#include "osQueue.h"
#include "osKernel.h"
#include "cmsis_gcc.h"


/* ================== Private functions declaration ================= */
static void put(osQueueObject * queue, const void * data);
static void get(osQueueObject * queue, void * buffer);


bool osQueueInit(osQueueObject* queue, const uint32_t dataSize) {

	if (dataSize > MAX_SIZE_QUEUE)
		return false;

	if (queue -> init)
		return false;

	queue -> init  = true;
	queue -> savedData = 0;
	queue -> dataSize = dataSize;
	queue -> queueLenght = MAX_SIZE_QUEUE / dataSize;

	return true;
}


void osQueueSend(osQueueObject* queue, const void* data, const uint32_t timeout) {
	/* TODO implement timeout functionality */

	if (! queue->init)
		return;

	/* if queue is full, task becomes blocked/waiting */
	while (isQueueFull(queue)) {

	    if (__get_IPSR() > 0) return;  // interrupt context

	    osTaskObject * task = osGetRunningTask();

		task->state = OS_TASK_BLOCK;
		task->queueFull = queue;

		osYield();
	}

	put(queue, data);
}


void osQueueReceive(osQueueObject* queue, void* buffer, const uint32_t timeout) {
	/* TODO implement timeout functionality */

	if (! queue->init)
		return;

	/* if queue is empty, task becomes blocked/waiting */
	while (isQueueEmpty(queue)) {

	    if (__get_IPSR() > 0) return;  // interrupt context

		osGetRunningTask()->state = OS_TASK_BLOCK;
		osGetRunningTask()->queueEmpty = queue;

		osYield();
	}

	get(queue, buffer);
}


bool isQueueFull(osQueueObject * queue) {
    return queue -> savedData == queue-> queueLenght;
}


bool isQueueEmpty(osQueueObject * queue) {
    return queue -> savedData == 0;
}


/* ================ Private functions implementation ================ */


static void put(osQueueObject * queue, const void * data) {

    /* ATTENTION
     * The order of instructions matter!
     *
     * Putting savedData first avoids having to create a critical section
     * since savedData automatically blocks any further queue full check.
     */
    queue -> savedData++;

	memcpy(queue->data + (queue->savedData - 1) * queue->dataSize, data, queue->dataSize);
}

static void get(osQueueObject * queue, void * buffer) {

    /* always get item at index 0 */
    memcpy(buffer, queue->data, queue->dataSize);

    /* re-ordering of elements */
    for (int i = 0; i < (queue->savedData - 1)*queue->dataSize; i++) {

        queue->data[i] = queue->data[i + queue->dataSize];
    }

	/* ATTENTION
     * The order of instructions matter!
     *
	 * Putting savedData last, avoids having to create a critical section
	 * since savedData automatically unblocks queue empty checks after doing changes.
	 */
	queue -> savedData--;
}


