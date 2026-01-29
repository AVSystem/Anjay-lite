/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
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

#include <anj/coap_downloader.h>
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
    anj_t *anj; // Pointer to the Anjay instance
} firmware_update_t;

// Static instances
static firmware_update_t firmware_update;
static anj_dm_fw_update_entity_ctx_t fu_entity;
static anj_coap_downloader_t coap_downloader;

static int fu_update_start(void *user_ptr) {
    firmware_update_t *fu = (firmware_update_t *) user_ptr;
    log(L_INFO, "Firmware Update process started");
    fu->waiting_for_reboot = true;
    return 0;
}

static void fu_reset(void *user_ptr) {
    firmware_update_t *fu = (firmware_update_t *) user_ptr;

    fu->waiting_for_reboot = false;
    anj_coap_downloader_terminate(&coap_downloader);
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

static anj_dm_fw_update_result_t fu_uri_write(void *user_ptr,
                                              const char *_uri) {
    (void) user_ptr;
    log(L_INFO, "fu_uri_write called with URI: %s", _uri);
    int res = anj_coap_downloader_start(&coap_downloader, _uri, NULL);
    if (res == ANJ_COAP_DOWNLOADER_ERR_INVALID_URI) {
        return ANJ_DM_FW_UPDATE_RESULT_INVALID_URI;
    } else if (res) {
        return ANJ_DM_FW_UPDATE_RESULT_FAILED;
    }
    return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
}

// Handlers table
static anj_dm_fw_update_handlers_t fu_handlers = {
    .uri_write_handler = fu_uri_write,
    .update_start_handler = fu_update_start,
    .reset_handler = fu_reset,
    .get_version = fu_get_version,
};

// Polls for the reboot request
void fw_update_process(void) {
    anj_coap_downloader_step(&coap_downloader);
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

static void coap_downloader_callback(void *arg,
                                     anj_coap_downloader_t *downloader,
                                     anj_coap_downloader_status_t conn_status,
                                     const uint8_t *data,
                                     size_t data_len) {
    firmware_update_t *fu = (firmware_update_t *) arg;

    switch (conn_status) {
    case ANJ_COAP_DOWNLOADER_STATUS_STARTING: {
        assert(fu->firmware_file == NULL);
        // Ensure previous file is removed
        if (remove(FW_IMAGE_PATH) != 0 && errno != ENOENT) {
            log(L_ERROR, "Failed to remove existing firmware image");
            anj_coap_downloader_terminate(&coap_downloader);
            break;
        }
        fu->firmware_file = fopen(FW_IMAGE_PATH, "wb");
        if (!fu->firmware_file) {
            log(L_ERROR, "Failed to open firmware image for writing");
            anj_coap_downloader_terminate(&coap_downloader);
            break;
        }
        log(L_INFO, "Firmware download starting");
        break;
    }
    case ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING: {
        assert(fu->firmware_file != NULL);
        log(L_INFO, "Writing %lu bytes at offset %lu", data_len, fu->offset);
        fu->offset += data_len;
        if (fwrite(data, 1, data_len, fu->firmware_file) != data_len) {
            log(L_ERROR, "Failed to write firmware chunk");
            anj_coap_downloader_terminate(&coap_downloader);
        }
        break;
    }
    case ANJ_COAP_DOWNLOADER_STATUS_FINISHING: {
        if (fu->firmware_file && fclose(fu->firmware_file)) {
            log(L_ERROR, "Failed to close firmware file");
        }
        fu->firmware_file = NULL;
        fu->offset = 0;
        break;
    }
    case ANJ_COAP_DOWNLOADER_STATUS_FINISHED:
        log(L_INFO, "Firmware download finished successfully");
        anj_dm_fw_update_object_set_download_result(
                fu->anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);
        break;
    case ANJ_COAP_DOWNLOADER_STATUS_FAILED:
        log(L_ERROR, "Firmware download failed with error: %d",
            anj_coap_downloader_get_error(downloader));
        anj_dm_fw_update_object_set_download_result(
                fu->anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_FAILED);
        break;
    default:
        log(L_WARNING, "Unknown firmware download status");
        break;
    }
}

// Installs the Firmware Update Object on the LwM2M client instance
int fw_update_object_install(anj_t *anj,
                             const char *firmware_version,
                             const char *endpoint_name) {
    firmware_update.firmware_version = firmware_version;
    firmware_update.endpoint_name = endpoint_name;
    firmware_update.waiting_for_reboot = false;
    firmware_update.anj = anj;

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

    anj_coap_downloader_configuration_t coap_downloader_config = {
        .event_cb = coap_downloader_callback,
        .event_cb_arg = &firmware_update,
    };
    if (anj_coap_downloader_init(&coap_downloader, &coap_downloader_config)) {
        return -1;
    }

    return 0;
}
