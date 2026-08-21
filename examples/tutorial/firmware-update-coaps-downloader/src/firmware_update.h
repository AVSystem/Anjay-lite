/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef _FIRMWARE_UPDATE_H_
#define _FIRMWARE_UPDATE_H_

#include <anj/defs.h>
#include <anj/log.h>

#define log(...) anj_log(fota_example_log, __VA_ARGS__)

/**
 * Configuration structure
 */
typedef struct {
    const char *firmware_version;
    const char *endpoint_name;

    /**
     * Retry configuration which will be passed
     * to COAP downloader
     */
    anj_time_duration_t retry_delay;
    uint8_t retry_count;
} firmware_update_config_t;

/**
 * Checks if a Firmware Update is pending and executes it if needed.
 * Should be called periodically in the main loop.
 */
void fw_update_process(void);

/**
 * Installs the Firmware Update Object on the LwM2M client instance.
 *
 * @param anj               Anjay Lite instance to operate on.
 * @param config            Configuration for the firmware update.
 *
 * @return 0 on success, -1 on failure.
 */
int fw_update_object_install(anj_t *anj,
                             const firmware_update_config_t *config);

#endif // _FIRMWARE_UPDATE_H_
