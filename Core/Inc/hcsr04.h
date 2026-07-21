#ifndef HCSR04_H
#define HCSR04_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    HCSR04_STATE_IDLE = 0,
    HCSR04_STATE_WAIT_RISING,
    HCSR04_STATE_WAIT_FALLING,
    HCSR04_STATE_DONE,
    HCSR04_STATE_TIMEOUT
} HCSR04_State;

typedef struct
{
    uint32_t pulse_width_us;
    uint16_t distance_mm;
    uint8_t valid;
} HCSR04_Result;

bool HCSR04_StartMeasurement(void);
bool HCSR04_GetResult(HCSR04_Result *result);
void HCSR04_ProcessTimeout(uint32_t current_tick);

#endif /* HCSR04_H */
