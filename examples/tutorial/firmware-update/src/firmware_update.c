/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <anj/defs.h>
#include <anj/dm/fw_update.h>
#include <anj/log.h>

#include "firmware_update.h"

// Paths used for firmware storage and reboot marker
#define FW_IMAGE_PATH "/tmp/firmware_image.bin"
#define FW_UPDATED_MARKER "/tmp/fw-updated-marker"

// Firmware Update context
typedef struct {
    const char *endpoint_name;
    const char *firmware_version;
    FILE *firmware_file;
    bool waiting_for_reboot;
    size_t offset;
} firmware_update_t;

// Static instances
static firmware_update_t firmware_update;
static anj_dm_fw_update_entity_ctx_t fu_entity;

// Firmware Update Handlers
static anj_dm_fw_update_result_t fu_write_start(void *user_ptr) {
    firmware_update_t *fu = (firmware_update_t *) user_ptr;
    assert(fu->firmware_file == NULL);

    // Ensure previous file is removed
    if (remove(FW_IMAGE_PATH) != 0 && errno != ENOENT) {
        log(L_ERROR, "Failed to remove existing firmware image");
        return ANJ_DM_FW_UPDATE_RESULT_FAILED;
    }

    fu->firmware_file = fopen(FW_IMAGE_PATH, "wb");
    if (!fu->firmware_file) {
        log(L_ERROR, "Failed to open firmware image for writing");
        return ANJ_DM_FW_UPDATE_RESULT_FAILED;
    }

    log(L_INFO, "Firmware Download started");
    return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
}

static anj_dm_fw_update_result_t
fu_write(void *user_ptr, const void *data, size_t data_size) {
    firmware_update_t *fu = (firmware_update_t *) user_ptr;
    assert(fu->firmware_file != NULL);

    log(L_INFO, "Writing %lu bytes at offset %lu", data_size, fu->offset);
    fu->offset += data_size;

    if (fwrite(data, 1, data_size, fu->firmware_file) != data_size) {
        log(L_ERROR, "Failed to write firmware chunk");
        return ANJ_DM_FW_UPDATE_RESULT_FAILED;
    }
    return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
}

static anj_dm_fw_update_result_t fu_write_finish(void *user_ptr) {
    firmware_update_t *fu = (firmware_update_t *) user_ptr;
    assert(fu->firmware_file != NULL);

    if (fclose(fu->firmware_file)) {
        log(L_ERROR, "Failed to close firmware file");
        fu->firmware_file = NULL;
        return ANJ_DM_FW_UPDATE_RESULT_FAILED;
    }

    fu->firmware_file = NULL;
    fu->offset = 0;
    log(L_INFO, "Firmware Download finished");
    return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
}

static int fu_update_start(void *user_ptr) {
    firmware_update_t *fu = (firmware_update_t *) user_ptr;
    log(L_INFO, "Firmware Update process started");
    fu->waiting_for_reboot = true;
    return 0;
}

static void fu_reset(void *user_ptr) {
    firmware_update_t *fu = (firmware_update_t *) user_ptr;

    fu->waiting_for_reboot = false;
    if (fu->firmware_file) {
        fclose(fu->firmware_file);
        fu->firmware_file = NULL;
    }
    (void) remove(FW_IMAGE_PATH);
}

static const char *fu_get_version(void *user_ptr) {
    firmware_update_t *fu = (firmware_update_t *) user_ptr;
    return fu->firmware_version;
}

// Handlers table
static anj_dm_fw_update_handlers_t fu_handlers = {
    .package_write_start_handler = fu_write_start,
    .package_write_handler = fu_write,
    .package_write_finish_handler = fu_write_finish,
    .uri_write_handler = NULL,
    .update_start_handler = fu_update_start,
    .reset_handler = fu_reset,
    .get_version = fu_get_version,
};

// Polls for the reboot request
void fw_update_check(void) {
    if (firmware_update.waiting_for_reboot) {
        log(L_INFO, "Rebooting to apply new firmware");

        firmware_update.waiting_for_reboot = false;

        if (chmod(FW_IMAGE_PATH, 0700) == -1) {
            log(L_ERROR, "Failed to make firmware executable");
            return;
        }

        FILE *marker = fopen(FW_UPDATED_MARKER, "w");
        if (marker) {
            fclose(marker);
        } else {
            log(L_ERROR, "Failed to create update marker");
            return;
        }

        execl(FW_IMAGE_PATH, FW_IMAGE_PATH, firmware_update.endpoint_name,
              NULL);
        log(L_ERROR, "execl() failed");

        unlink(FW_UPDATED_MARKER);
        exit(EXIT_FAILURE);
    }
}

// Installs the Firmware Update Object on the LwM2M client instance
int fw_update_object_install(anj_t *anj,
                             const char *firmware_version,
                             const char *endpoint_name) {
    firmware_update.firmware_version = firmware_version;
    firmware_update.endpoint_name = endpoint_name;
    firmware_update.waiting_for_reboot = false;

    if (anj_dm_fw_update_object_install(anj, &fu_entity, &fu_handlers,
                                        &firmware_update)) {
        return -1;
    }

    if (access(FW_UPDATED_MARKER, F_OK) == 0) {
        log(L_INFO, "Firmware Updated successfully");
        unlink(FW_UPDATED_MARKER);
        (void) anj_dm_fw_update_object_set_update_result(
                anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);
    }

    return 0;
}
