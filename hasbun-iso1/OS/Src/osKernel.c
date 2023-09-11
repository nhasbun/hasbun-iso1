#include <stddef.h>

#include "osKernel.h"
#include "system_stm32f4xx.h"
#include "stm32f429xx.h"
#include "stm32f4xx_hal.h"
#include "core_cm4.h"
#include "cmsis_gcc.h"

/* ==================== Define private variables ==================== */
#define FIRST_INDEX_TASK_PRIORITY     0U

typedef struct
{
    osTaskObject*   listTask[MAX_NUMBER_TASK]; // Task list.
    osTaskObject*   oldTask;                   // Old exiting task.
    osTaskObject*   currentTask;               // Current task intended to run.
    uint32_t		currentTaskIndex;		   // Current task index
    uint8_t         countTask;                 // Number of task created.
    bool            running;                   // Status task, if it is running true in otherwise false.
}osKernelObject;

/* ================== Private variables declaration ================= */
static osKernelObject osKernel = {
		.listTask = {0},
		.oldTask = NULL,
		.currentTask = NULL,
		.currentTaskIndex = 0xffff,
		.countTask = 0,
		.running = false
};

/* ================== Private functions declaration ================= */

static uint32_t getNextContext(uint32_t currentTaskStackPointer);
static uint32_t getNextTaskIndex();
static void disableKernelInterrupts(bool setDisable);
static void scheduler(void);

/* ================= Public functions implementation ================ */

bool osTaskCreate(osTaskObject * handler, void * callback, void * arg) {
    if (osKernel.countTask >= MAX_NUMBER_TASK)
    {
        return false;
    }

    /* Setting:
     * - xPSR bit 24 to 1 (thumb bit)
     * - PC counter value set to given callback/task method
     * - R0 reg is a pointer indicating a given argument structure for the task
     * - LR return values is fixed for combo: thread mode + MSP register usage
     */
    handler -> memory[MAX_STACK_SIZE/4 - XPSR_REG_POSITION]     = XPSR_VALUE;
    handler -> memory[MAX_STACK_SIZE/4 - PC_REG_POSTION]        = (uint32_t) callback;
    handler -> memory[MAX_STACK_SIZE/4 - R0_REG_POSTION]        = (uint32_t) arg;
    handler -> memory[MAX_STACK_SIZE/4 - LR_PREV_VALUE_POSTION] = EXEC_RETURN_VALUE;


    // Pointer function of task.
    handler -> entryPoint     = callback;
    handler -> id             = osKernel.countTask;
    handler -> stackPointer   = (uint32_t)(handler->memory + MAX_STACK_SIZE/4 - SIZE_STACK_FRAME);

    // Fill controls OS structure
    osKernel.listTask[osKernel.countTask] = handler;
    osKernel.countTask++;

    return true;
}


void osStart(void)
{
	disableKernelInterrupts(true);

    osKernel.running = false;

    /*
     * All interrupts has priority 0 (maximum) at start execution. For that don't happen fault
     * condition, we have to less priority of NVIC. This math calculation showing take lowest
     * priority possible.
     */
    NVIC_SetPriority(PendSV_IRQn, (1 << __NVIC_PRIO_BITS)-1);

    /* Activate and configure the time of Systick exception */
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / (1000U / SYSTICK_PERIOD_MS));

    osKernel.running = true;

    disableKernelInterrupts(false);
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
 * @brief Simplify circular advance in an array of kernel tasks.
 *
 * We assume at least one task in the kernel.
 */
static uint32_t getNextTaskIndex() {

	uint32_t desiredTaskIndex = osKernel.currentTaskIndex + 1;

	if (desiredTaskIndex >= osKernel.countTask) {
		desiredTaskIndex = 0;
	}

	return desiredTaskIndex;
}


/**
 * @brief Disable kernel interrupts method.
 *
 * Useful to maybe later add a serial print mechanism.
 */
static void disableKernelInterrupts(bool setDisable) {
	if (setDisable) {
		NVIC_DisableIRQ(SysTick_IRQn);
		NVIC_DisableIRQ(PendSV_IRQn);

	} else {
		NVIC_EnableIRQ(PendSV_IRQn);
		NVIC_EnableIRQ(SysTick_IRQn);
	}
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

	// Calculate next task index from kernel task list
	uint32_t targetTaskIndex = getNextTaskIndex();

	// Current task is null the first run and we ignore it
	if (osKernel.currentTask != NULL)
		osKernel.currentTask -> status = OS_TASK_READY;

	// We change target task
	osKernel.oldTask = osKernel.currentTask;
	osKernel.currentTaskIndex = targetTaskIndex;
	osKernel.currentTask = osKernel.listTask[targetTaskIndex];
	osKernel.currentTask -> status = OS_TASK_RUNNING;
}

/* ========== Processor Interruption and Exception Handlers ========= */

void SysTick_Handler(void)
{
	// Keep the tick count going
	HAL_IncTick();

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

    /* Se hace un branch indirect con el valor de LR que es nuevamente EXEC_RETURN */
    __ASM volatile ("bx lr");
}
