#include <stddef.h>

#include "osKernel.h"
#include "osQueue.h"
#include "system_stm32f4xx.h"
#include "stm32f429xx.h"
#include "core_cm4.h"
#include "cmsis_gcc.h"

/* ==================== Define private variables ==================== */
#define FIRST_INDEX_TASK_PRIORITY     0U

typedef struct
{
    osTaskObject*   priorityTaskMatrix[MAX_NUMBER_PRIORITY + 1][MAX_NUMBER_TASK]; // Task matrix by priority (including idle task)
    osTaskObject*   oldTask;                   // Old exiting task.
    osTaskObject*   currentTask;               // Current task intended to run.
    uint32_t        oldPriority;               // Old exiting priority.
    uint32_t        currentPriority;           // Current running priority.
    uint32_t        currentTaskIndex[MAX_NUMBER_PRIORITY + 1];
                                               // currentTaskIndex by priority.
    uint32_t        countTask;                 // Total task count (used for id).
    uint8_t         countTaskByPriority[MAX_NUMBER_PRIORITY + 1];
                                               // Number of task created by priority (including idle).
    uint32_t        osTime;                    // System time in milliseconds since osStart call.

    bool            running;                   // Status task, if it is running true in otherwise false.


    osTaskObject*   idleTask;                  // specific idle task object.
} osKernelObject;


/* Fix: Creating internal idle priority */
static const int OS_IDLE_PRIORITY = (int)MAX_NUMBER_PRIORITY;

/* ================== Private variables declaration ================= */
static osTaskObject _idleTask = {{0}};

static osKernelObject osKernel = {
		.priorityTaskMatrix = {{0}},
		.oldTask = NULL,
		.currentTask = NULL,
		.oldPriority = 0,
		.currentPriority = OS_VERYHIGH_PRIORITY,
		.currentTaskIndex = {0xffffffff},
		.countTask = 0,
		.countTaskByPriority = {0},
		.osTime = 0,
		.running = false,

		.idleTask = &_idleTask
};

/* ================== Private functions declaration ================= */

static uint32_t getNextContext(uint32_t currentTaskStackPointer);
static void getNextTask(osPriorityType * priority, uint32_t * taskIndex);
static void scheduler(void);
static void idleTaskCreate();

/* ================= Public functions implementation ================ */

bool osTaskCreate(osTaskObject* handler, osPriorityType priority, void* callback) {
    if (osKernel.countTask >= MAX_NUMBER_TASK)
    {
        return false;
    }

    if (priority > OS_IDLE_PRIORITY - 1)
    	priority = OS_IDLE_PRIORITY - 1;

    /* Setting:
     * - xPSR bit 24 to 1 (thumb bit)
     * - PC counter value set to given callback/task method
     * - R0 reg is a pointer indicating a given argument structure for the task
     * - LR return values is fixed for combo: thread mode + MSP register usage
     */
    handler -> memory[MAX_STACK_SIZE/4 - XPSR_REG_POSITION]     = XPSR_VALUE;
    handler -> memory[MAX_STACK_SIZE/4 - PC_REG_POSTION]        = (uint32_t) callback;
    // handler -> memory[MAX_STACK_SIZE/4 - R0_REG_POSTION]        = (uint32_t) arg;
    handler -> memory[MAX_STACK_SIZE/4 - LR_PREV_VALUE_POSTION] = EXEC_RETURN_VALUE;
    // If occur some problem and task executed return so after that execute this function.
    handler->memory[MAX_STACK_SIZE/4 - LR_REG_POSTION]          = (uint32_t)osReturnTaskHook;


    // Pointer function of task.
    handler -> entryPoint     = callback;
    handler -> id             = osKernel.countTask;
    handler -> state          = OS_TASK_READY;
    handler -> priority       = priority;
    handler -> stackPointer   = (uint32_t)(handler->memory + MAX_STACK_SIZE/4 - SIZE_STACK_FRAME);

    // Delay and Queue fields init
    handler -> delayStopTime = 0;
    handler -> queueEmpty    = NULL;
    handler -> queueFull     = NULL;


    // Fill controls OS structure (adjusted for priority)
    osKernel.priorityTaskMatrix[priority][osKernel.countTaskByPriority[priority]] = handler;
    osKernel.countTaskByPriority[priority] += 1;
    osKernel.countTask++;

    return true;
}


void osStart(void)
{
	osDisableKernelInterrupts(true);

    osKernel.running = false;

    idleTaskCreate();

    /*
     * All interrupts has priority 0 (maximum) at start execution. For that don't happen fault
     * condition, we have to less priority of NVIC. This math calculation showing take lowest
     * priority possible.
     */
    NVIC_SetPriority(PendSV_IRQn, (1 << __NVIC_PRIO_BITS)-1);

    /* Activate and configure the time of Systick exception */
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / (1000U * SYSTICK_PERIOD_MS));

    osKernel.running = true;

    osDisableKernelInterrupts(false);
}


void osDelay(const uint32_t tick) {

    if (tick == 0) return;

	osTaskObject * thisTask = osKernel.currentTask;

	thisTask -> state = OS_TASK_BLOCK;
	thisTask -> delayStopTime = osKernel.osTime + tick;

	osYield();
}


void osEnterCriticalSection(void) {
    __disable_irq();
}


void osExitCriticalSection(void) {
    __enable_irq();
}


__WEAK void osReturnTaskHook(void) {

}

__WEAK void osIdleTask(void) {
    while(1) __WFI();
}


osTaskObject * osGetRunningTask() {

	if (!osKernel.running)
		return NULL;

	if (osKernel.idleTask == osKernel.currentTask)
		return NULL;

	return osKernel.currentTask;
}


void osDisableKernelInterrupts(bool setDisable) {
    if (setDisable) {
        NVIC_DisableIRQ(SysTick_IRQn);
        NVIC_DisableIRQ(PendSV_IRQn);

    } else {
        NVIC_EnableIRQ(PendSV_IRQn);
        NVIC_EnableIRQ(SysTick_IRQn);
    }
}


void osYield() {
    osDisableKernelInterrupts(true);

    scheduler();
    /*
     * Set up bit corresponding exception PendSV
     */
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

    /*
     * Instruction Synchronization Barrier; flushes the pipeline and ensures that
     * all previous instructions are completed before executing new instructions
     */
    __ISB();

    /*
     * Data Synchronization Barrier; ensures that all memory accesses are
     * completed before next instruction is executed
     */
    __DSB();

    osDisableKernelInterrupts(false);
}

/* ================ Private functions implementation ================ */

/**
 * @brief Get next context task.
 *
 * @param[in] currentStaskPointer Stack pointer of current task.
 *
 * @return Return stack pointer of new task to execute.
 */
static uint32_t getNextContext(uint32_t currentTaskStackPointer)
{
	// save current context pointer
	osKernel.oldTask->stackPointer = currentTaskStackPointer;
	// return new task context pointer
	return osKernel.currentTask->stackPointer;
}

/**
 * @brief Calculate next task intended to be running.
 *
 * Get next task matrix coordinates (priority + priority index in priorityTaskMatrix in osKernel).
 */
static void getNextTask(osPriorityType * priority, uint32_t * taskIndex) {

	// By default running with lowest kernel idle priority
	osPriorityType priorityTarget = OS_IDLE_PRIORITY;

	// Check entire matrix for task achieving two objectives:
	// 1- update any task in waiting state and move to ready state accordingly
	// 2- find highest priority number with at least 1 task in ready state
	for (int i = 0; i < MAX_NUMBER_PRIORITY; i++) {
		for (int j = 0; j < osKernel.countTaskByPriority[i]; j++) {

			osTaskObject * task = osKernel.priorityTaskMatrix[i][j];

			// Checking for delays
			if (task -> state == OS_TASK_BLOCK &&
                task -> delayStopTime != 0) {

			    if (task -> delayStopTime == osKernel.osTime) {
				    task -> delayStopTime = 0;
					task -> state = OS_TASK_READY;
				}
			}

			// Checking for queue empty
			if (task -> state == OS_TASK_BLOCK &&
			    task -> queueEmpty != NULL) {

			    if (!isQueueEmpty(task -> queueEmpty)) {
			        task -> queueEmpty = NULL;
			        task -> state = OS_TASK_READY;
			    }
			}

			// Checking for queue full
			if (task -> state == OS_TASK_BLOCK &&
                task -> queueFull != NULL) {

                if (!isQueueFull(task -> queueFull)) {
                    task -> queueFull = NULL;
                    task -> state = OS_TASK_READY;
                }
            }

			// Checking if task if ready then updating target priority.
			if (task -> state == OS_TASK_READY) {

				if (i < priorityTarget)
					priorityTarget = i;
			}
		}
	}

	/* Update osKernel priority attendance and current task index.
	 *
	 * We work with a circular task index for every existing priority.
	 * This index is kept in memory and brought back so work is fairly distributed
	 * even in a recent priority change situation.
	 */

	if (priorityTarget != osKernel.currentPriority) {
		osKernel.oldPriority = osKernel.currentPriority;
		osKernel.currentPriority = priorityTarget;
	}

	// calculate matrix coordinates
	uint32_t _priority = 0;
	uint32_t _taskIndex = 0;

	for(;;) {
		osKernel.currentTaskIndex[osKernel.currentPriority] += 1;

		if (osKernel.currentTaskIndex[osKernel.currentPriority] > osKernel.countTaskByPriority[osKernel.currentPriority] - 1)
			osKernel.currentTaskIndex[osKernel.currentPriority] = 0;

		_priority  = osKernel.currentPriority;
		_taskIndex = osKernel.currentTaskIndex[osKernel.currentPriority];

		/* if pointed task is not ready we skip it and move to the next */
		if (osKernel.priorityTaskMatrix[_priority][_taskIndex] -> state == OS_TASK_READY)
			break;
	}

	// return values
	*priority = _priority;
	*taskIndex = _taskIndex;
}

/**
 * @brief Get the task that must be run.
 *
 * @return Returns true if a new task to be executed.
 */
static void scheduler(void)
{
	// Catch no registered tasks
	if (osKernel.countTask == 0) return;

	// If current task is in state running we change to ready
	if (osKernel.currentTask -> state == OS_TASK_RUNNING)
		osKernel.currentTask -> state = OS_TASK_READY;

	// calculate next task coordinates
	osPriorityType priority = 0;
	uint32_t taskIndex = 0;
	getNextTask(&priority, &taskIndex);

	// We change target task
	osKernel.oldTask = osKernel.currentTask;
	osKernel.currentTask = osKernel.priorityTaskMatrix[priority][taskIndex];
	osKernel.currentTask -> state = OS_TASK_RUNNING;
}


static void idleTaskCreate() {
	osTaskObject * handler = osKernel.idleTask;


    /* Setting:
     * - xPSR bit 24 to 1 (thumb bit)
     * - PC counter value set to given callback/task method
     * - R0 reg is a pointer indicating a given argument structure for the task
     * - LR return values is fixed for combo: thread mode + MSP register usage
     */
    handler -> memory[MAX_STACK_SIZE/4 - XPSR_REG_POSITION]     = XPSR_VALUE;
    handler -> memory[MAX_STACK_SIZE/4 - PC_REG_POSTION]        = (uint32_t) osIdleTask;
    // handler -> memory[MAX_STACK_SIZE/4 - R0_REG_POSTION]        = (uint32_t) arg;
    handler -> memory[MAX_STACK_SIZE/4 - LR_PREV_VALUE_POSTION] = EXEC_RETURN_VALUE;
    // If occur some problem and task executed return so after that execute this function.
    handler->memory[MAX_STACK_SIZE/4 - LR_REG_POSTION]          = (uint32_t) osReturnTaskHook;


    // Pointer function of task.
    handler -> entryPoint     = osIdleTask;
    handler -> id             = osKernel.countTask;
    handler -> state          = OS_TASK_READY;
    handler -> priority       = (osPriorityType) OS_IDLE_PRIORITY;
    handler -> stackPointer   = (uint32_t)(handler->memory + MAX_STACK_SIZE/4 - SIZE_STACK_FRAME);

    // Delay and Queue fields init
    handler -> delayStopTime = 0;
    handler -> queueEmpty    = NULL;
    handler -> queueFull     = NULL;

    // Fill controls OS structure (adjusted for priority)
    osKernel.priorityTaskMatrix[OS_IDLE_PRIORITY][osKernel.countTaskByPriority[OS_IDLE_PRIORITY]] = handler;
    osKernel.countTaskByPriority[OS_IDLE_PRIORITY] += 1;
    osKernel.countTask++;
}


/* ========== Processor Interruption and Exception Handlers ========= */

void SysTick_Handler(void)
{
	osKernel.osTime++;
	scheduler();

    /*
     * Set up bit corresponding exception PendSV
     */
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

    /*
     * Instruction Synchronization Barrier; flushes the pipeline and ensures that
     * all previous instructions are completed before executing new instructions
     */
    __ISB();

    /*
     * Data Synchronization Barrier; ensures that all memory accesses are
     * completed before next instruction is executed
     */
    __DSB();
}

__attribute__ ((naked)) void PendSV_Handler(void)
{
    // Se entra a la seccion critica y se deshabilita las interrupciones.
    __ASM volatile ("cpsid i");

    /**
     * Implementación de stacking para FPU:
     *
     * Las tres primeras corresponden a un testeo del bit EXEC_RETURN[4]. La instruccion TST hace un
     * AND estilo bitwise (bit a bit) entre el registro LR y el literal inmediato. El resultado de esta
     * operacion no se guarda y los bits N y Z son actualizados. En este caso, si el bit EXEC_RETURN[4] = 0
     * el resultado de la operacion sera cero, y la bandera Z = 1, por lo que se da la condicion EQ y
     * se hace el push de los registros de FPU restantes
     */
    __ASM volatile ("tst lr, 0x10");
    __ASM volatile ("it eq");
    __ASM volatile ("vpusheq {s16-s31}");

    /**
     * Cuando se ingresa al handler de PendSV lo primero que se ejecuta es un push para
	 * guardar los registros R4-R11 y el valor de LR, que en este punto es EXEC_RETURN
	 * El push se hace al reves de como se escribe en la instruccion, por lo que LR
	 * se guarda en la posicion 9 (luego del stack frame). Como la funcion getNextContext
	 * se llama con un branch con link, el valor del LR es modificado guardando la direccion
	 * de retorno una vez se complete la ejecucion de la funcion
	 * El pasaje de argumentos a getContextoSiguiente se hace como especifica el AAPCS siendo
	 * el unico argumento pasado por RO, y el valor de retorno tambien se almacena en R0
	 *
	 * NOTA: El primer ingreso a este handler (luego del reset) implica que el push se hace sobre el
	 * stack inicial, ese stack se pierde porque no hay seguimiento del MSP en el primer ingreso
     */
    __ASM volatile ("push {r4-r11, lr}");
    __ASM volatile ("mrs r0, msp");
    __ASM volatile ("bl %0" :: "i"(getNextContext));
    __ASM volatile ("msr msp, r0");
    __ASM volatile ("pop {r4-r11, lr}");    //Recuperados todos los valores de registros

    /**
     * Implementación de unstacking para FPU:
     *
     * Habiendo hecho el cambio de contexto y recuperado los valores de los registros, es necesario
     * determinar si el contexto tiene guardados registros correspondientes a la FPU. si este es el caso
     * se hace el unstacking de los que se hizo PUSH manualmente.
     */
    __ASM volatile ("tst lr,0x10");
    __ASM volatile ("it eq");
    __ASM volatile ("vpopeq {s16-s31}");

    // Se sale de la seccion critica y se habilita las interrupciones.
    __ASM volatile ("cpsie i");

    /* Se hace un branch indirect con el valor de LR que es nuevamente EXEC_RETURN */
    __ASM volatile ("bx lr");
}
