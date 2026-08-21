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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <anj/core.h>
#include <anj/defs.h>

#define log(...) anj_log(fota_example_log, __VA_ARGS__)

/**
 * Configuration structure
 */
typedef struct {
    const char *firmware_version;
    const char *endpoint_name;

    anj_time_duration_t retry_delay;
    uint8_t retry_count;
} firmware_update_config_t;

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

/**
 * Checks if a Firmware Update is pending and executes it if needed.
 * Should be called periodically in the main loop.
 */
void fw_update_process(void);

/**
 * Checks if a firmware download is currently active.
 *
 * @return true if a download is active, false otherwise.
 */
bool fw_update_is_download_active(void);

/**
 * Returns the number of received firmware chunks.
 *
 * @return Number of received chunks.
 */
size_t fw_update_get_received_chunk_count(void);

/**
 * Suspends the ongoing firmware download.
 *
 * @return 0 on success, non-zero on failure.
 */
int fw_update_suspend_download(void);

/**
 * Resumes a previously suspended firmware download.
 *
 * @return 0 on success, non-zero on failure.
 */
int fw_update_resume_download(void);

#endif // _FIRMWARE_UPDATE_H_
