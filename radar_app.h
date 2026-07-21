#ifndef RADAR_APP_H
#define RADAR_APP_H

#include <stdint.h>
#include "cmsis_os2.h"

#define RADAR_MIN_ANGLE_DEG         0U
#define RADAR_MAX_ANGLE_DEG       180U
#define RADAR_STEP_ANGLE_DEG        5U
#define RADAR_MEASURE_TIMEOUT_MS   35U
#define RADAR_IDLE_DELAY_MS         1U
#define RADAR_QUEUE_LENGTH          8U

#define RADAR_SPEED_FAST_SETTLE_MS  80U
#define RADAR_SPEED_SLOW_SETTLE_MS 150U

typedef enum {
    RADAR_SPEED_FAST = 0,
    RADAR_SPEED_SLOW
} RadarSpeedMode;

typedef enum {
    RADAR_APP_RUNNING = 0,
    RADAR_APP_SPEED_SETTING
} RadarAppMode;

typedef union {
    struct {
        uint16_t appMode;
        uint16_t speedMode;
    } state;
    uint32_t packed;
} RadarControlState;

extern volatile RadarControlState g_radarControlState;

#define RADAR_CALIBRATION_MODE       0U
#define RADAR_CALIBRATION_ANGLE_DEG 90U
#define RADAR_CALIBRATION_SAMPLES   20U
#define RADAR_CALIBRATION_WARMUP_SAMPLES 3U

#if RADAR_CALIBRATION_ANGLE_DEG > 180U
#error "Calibration angle must be in range 0..180 degrees"
#endif

typedef struct
{
    uint16_t angle_deg;
    uint16_t min_distance_mm;
    uint16_t max_distance_mm;
    uint16_t average_distance_mm;

    uint32_t warmup_samples_seen;
    uint32_t total_samples;
    uint32_t valid_samples;
    uint32_t invalid_samples;
    uint32_t sum_distance_mm;

    uint8_t complete;
} RadarCalibrationStats;

#if RADAR_CALIBRATION_MODE == 1U
extern volatile RadarCalibrationStats g_radarCalibrationStats;
#endif

typedef struct
{
    uint16_t angle_deg;
    uint16_t distance_mm;
    uint32_t timestamp_ms;
    uint8_t valid;
} RadarSample;

extern osMessageQueueId_t radarQueueHandle;

#endif /* RADAR_APP_H */
