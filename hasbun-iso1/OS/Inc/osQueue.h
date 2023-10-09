
#ifndef INC_OSQUEUE_H
#define INC_OSQUEUE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_SIZE_QUEUE  4     // Maximum buffer size of the queue

/**
 * @brief Data structure queue.
 */
typedef struct {
	bool init;
	void * data[MAX_SIZE_QUEUE];
	uint32_t savedData;
	uint32_t dataSize;

} osQueueObject;

/**
 * @brief Initialize the queue.
 *
 * @param[in, out]  queue       Queue object.
 * @param[in]       dataSize    Data size of the queue in bytes.
 *
 * @return Returns true if was success in otherwise false.
 */
bool osQueueInit(osQueueObject* queue, const uint32_t dataSize);

/**
 * @brief Send data to the queue.
 *
 * Expected to work inside interrupts but without blocking when full.
 * Inside an interrupt is recommended to use isQueueFull before.
 *
 * @param[in, out]  queue   Queue object.
 * @param[in, out]  data    Data sent to the queue.
 * @param[in]       timeout Number of ticks to wait before blocking the task..
 *
 * @return Returns true if it could be put in the queue
 * in otherwise false.
 */
void osQueueSend(osQueueObject* queue, const void* data, const uint32_t timeout);

/**
 * @brief Receive data to the queue.
 *
 * Expected to work inside interrupts but without blocking when empty.
 * Inside an interrupt is recommended to use isQueueEmpty before.
 *
 * @param[in, out]  queue   Queue object.
 * @param[in, out]  buffer  Buffer to  save the data read from the queue.
 * @param[in]       timeout Number of ticks to wait before blocking the task..
 *
 * @return Returns true if it was possible to take it out in the queue
 * in otherwise false.
 */
void osQueueReceive(osQueueObject* queue, void* buffer, const uint32_t timeout);


/**
 * @brief Helper function to check if a queue is full.
 */
bool isQueueFull(osQueueObject * queue);

/**
 * @brief Helper function to check if a queue is empty.
 */
bool isQueueEmpty(osQueueObject * queue);

#endif // INC_OSQUEUE_H
