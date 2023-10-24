#include "osIRQ.h"


#define NOHANDLER 0
#define NODATA    0


bool osRegisterIRQ(osIRQnType irqType, IRQHandler function, void *data)
{
    /* Check if an IRQ is already registered for this irqType */
    if (irqVector[irqType].handler != NOHANDLER) return false;

    irqVector[irqType].handler = function;
    irqVector[irqType].data = data;

    NVIC_ClearPendingIRQ(irqType);
    NVIC_EnableIRQ(irqType);

    return true;
}

bool osUnregisterIRQ(osIRQnType irqType)
{
    /* Check if an IRQ is actually registered for this irqType */
    if (irqVector[irqType].handler == NOHANDLER) return false;

    irqVector[irqType].handler = NOHANDLER;
    irqVector[irqType].data = NODATA;

    NVIC_ClearPendingIRQ(irqType);
    NVIC_EnableIRQ(irqType);

    return true;
}

