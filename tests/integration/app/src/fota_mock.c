/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include "fota_mock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <anj/log.h>

#define log(...) anj_log(fota_mock, __VA_ARGS__)

// This module is only usable when the default FW Update Object, Pull
// method and CoAP downloader are all enabled; some test configurations
// build a minimal client without these, in which case it becomes a no-op.
#if defined(ANJ_WITH_DEFAULT_FOTA_OBJ) && defined(ANJ_FOTA_WITH_PULL_METHOD) \
        && defined(ANJ_WITH_COAP_DOWNLOADER)
#    define FOTA_MOCK_ENABLED 1
#else
#    define FOTA_MOCK_ENABLED 0
#endif

#if FOTA_MOCK_ENABLED

#    include <anj/coap_downloader.h>
#    include <anj/dm/fw_update.h>

// Largest prime below 2^16; used by the Adler-32 checksum below.
#    define ADLER32_MODULUS 65521u

// The mocked "firmware package" is simply a 4-byte big-endian Adler-32
// checksum of the rest of the data, followed by that data. The mock
// verifies the checksum itself (instead of relying on the server to
// re-verify it), reporting ANJ_DM_FW_UPDATE_RESULT_INTEGRITY_FAILURE on
// mismatch - exercising the Pull/Push transfer pipeline without any custom
// resource or RPC command.
#    define CHECKSUM_HEADER_SIZE 4

typedef enum {
    FOTA_MOCK_PENDING_NONE,
    // Report Update success on the next fota_mock_process() call.
    FOTA_MOCK_PENDING_REPORT_SUCCESS,
    // Trigger anj_core_restart() on the next fota_mock_process() call.
    FOTA_MOCK_PENDING_TRIGGER_RESTART,
    // Restart triggered; report Update success on the next connection
    // status change.
    FOTA_MOCK_PENDING_AFTER_RESTART,
} fota_mock_pending_action_t;

typedef struct {
    anj_t *anj;
    bool reboot_required;

    const char *psk_identity;
    size_t psk_identity_len;
    const char *psk_key;
    size_t psk_key_len;

    uint8_t header_buf[CHECKSUM_HEADER_SIZE];
    size_t header_bytes_received;
    uint32_t declared_checksum;
    uint32_t adler_a;
    uint32_t adler_b;

    fota_mock_pending_action_t pending_action;
} fota_mock_state_t;

static fota_mock_state_t state;
static anj_dm_fw_update_entity_ctx_t fu_entity;
static anj_coap_downloader_t coap_downloader;

static void checksum_reset(void) {
    state.header_bytes_received = 0;
    state.declared_checksum = 0;
    state.adler_a = 1;
    state.adler_b = 0;
}

// Consumes the leading CHECKSUM_HEADER_SIZE bytes of the package as the
// declared checksum, and folds the remainder into the running Adler-32.
static void process_incoming_bytes(const uint8_t *data, size_t len) {
    while (len && state.header_bytes_received < CHECKSUM_HEADER_SIZE) {
        state.header_buf[state.header_bytes_received++] = *data++;
        len--;
    }
    if (state.header_bytes_received == CHECKSUM_HEADER_SIZE) {
        state.declared_checksum = ((uint32_t) state.header_buf[0] << 24)
                                  | ((uint32_t) state.header_buf[1] << 16)
                                  | ((uint32_t) state.header_buf[2] << 8)
                                  | (uint32_t) state.header_buf[3];
    }
    for (size_t i = 0; i < len; i++) {
        state.adler_a = (state.adler_a + data[i]) % ADLER32_MODULUS;
        state.adler_b = (state.adler_b + state.adler_a) % ADLER32_MODULUS;
    }
}

static bool checksum_valid(void) {
    return state.header_bytes_received == CHECKSUM_HEADER_SIZE
           && ((state.adler_b << 16) | state.adler_a)
                      == state.declared_checksum;
}

static anj_dm_fw_update_result_t fota_uri_write(void *user_ptr,
                                                const char *uri) {
    (void) user_ptr;

    log(L_INFO, "Starting firmware download from %s", uri);
    checksum_reset();

    const anj_net_config_t *net_config = NULL;
#    ifdef ANJ_WITH_SECURITY
    anj_net_config_t psk_net_config;
    if (state.psk_identity && state.psk_key) {
        // Only relevant for coaps:// URIs; independent of the LwM2M Server
        // connection's own security configuration.
        memset(&psk_net_config, 0, sizeof(psk_net_config));
        psk_net_config.secure_socket_config.security.mode =
                ANJ_NET_SECURITY_PSK;
        psk_net_config.secure_socket_config.security.data.psk.identity =
                (anj_crypto_security_info_t) {
                    .tag = ANJ_CRYPTO_SECURITY_TAG_PSK_IDENTITY,
                    .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
                    .info.buffer = {
                        .data = state.psk_identity,
                        .data_size = state.psk_identity_len
                    }
                };
        psk_net_config.secure_socket_config.security.data.psk.key =
                (anj_crypto_security_info_t) {
                    .tag = ANJ_CRYPTO_SECURITY_TAG_PSK_KEY,
                    .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
                    .info.buffer = {
                        .data = state.psk_key,
                        .data_size = state.psk_key_len
                    }
                };
        net_config = &psk_net_config;
    }
#    endif // ANJ_WITH_SECURITY

    int res = anj_coap_downloader_start(&coap_downloader, uri, net_config);
    if (res == ANJ_COAP_DOWNLOADER_ERR_INVALID_URI) {
        return ANJ_DM_FW_UPDATE_RESULT_INVALID_URI;
    } else if (res) {
        log(L_ERROR, "Failed to start firmware download: %d", res);
        return ANJ_DM_FW_UPDATE_RESULT_FAILED;
    }
    return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
}

static int fota_update_start(void *user_ptr) {
    (void) user_ptr;

    log(L_INFO, "Firmware update requested");
    // The library sets the State Resource to Updating right after this
    // handler returns, so acting on state.pending_action is deferred to
    // fota_mock_process()/fota_mock_on_conn_status_changed() to avoid
    // racing with that transition.
    state.pending_action = state.reboot_required
                                   ? FOTA_MOCK_PENDING_TRIGGER_RESTART
                                   : FOTA_MOCK_PENDING_REPORT_SUCCESS;
    return 0;
}

static void fota_reset(void *user_ptr) {
    (void) user_ptr;

    state.pending_action = FOTA_MOCK_PENDING_NONE;
    checksum_reset();
    anj_coap_downloader_terminate(&coap_downloader);
}

static anj_dm_fw_update_result_t fota_package_write_start(void *user_ptr) {
    (void) user_ptr;
    checksum_reset();
    return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
}

static anj_dm_fw_update_result_t
fota_package_write(void *user_ptr, const void *data, size_t data_size) {
    (void) user_ptr;
    process_incoming_bytes((const uint8_t *) data, data_size);
    return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
}

static anj_dm_fw_update_result_t fota_package_write_finish(void *user_ptr) {
    (void) user_ptr;
    if (!checksum_valid()) {
        log(L_WARNING, "Firmware checksum mismatch");
        return ANJ_DM_FW_UPDATE_RESULT_INTEGRITY_FAILURE;
    }
    return ANJ_DM_FW_UPDATE_RESULT_SUCCESS;
}

static anj_dm_fw_update_handlers_t fu_handlers = {
    .package_write_start_handler = fota_package_write_start,
    .package_write_handler = fota_package_write,
    .package_write_finish_handler = fota_package_write_finish,
    .uri_write_handler = fota_uri_write,
    .update_start_handler = fota_update_start,
    .reset_handler = fota_reset,
};

static void coap_downloader_callback(void *arg,
                                     anj_coap_downloader_t *downloader,
                                     anj_coap_downloader_status_t conn_status,
                                     const uint8_t *data,
                                     size_t data_len) {
    (void) arg;

    switch (conn_status) {
    case ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING:
        process_incoming_bytes(data, data_len);
        break;
    case ANJ_COAP_DOWNLOADER_STATUS_FINISHED: {
        bool valid = checksum_valid();
        if (!valid) {
            log(L_WARNING, "Firmware checksum mismatch");
        }
        anj_dm_fw_update_object_set_download_result(
                state.anj, &fu_entity,
                valid ? ANJ_DM_FW_UPDATE_RESULT_SUCCESS
                      : ANJ_DM_FW_UPDATE_RESULT_INTEGRITY_FAILURE);
        break;
    }
    case ANJ_COAP_DOWNLOADER_STATUS_FAILED:
        log(L_ERROR, "Firmware download failed with error: %d",
            anj_coap_downloader_get_error(downloader));
        anj_dm_fw_update_object_set_download_result(
                state.anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_FAILED);
        break;
    default:
        break;
    }
}

int fota_mock_install(anj_t *anj, const fota_mock_config_t *config) {
    state.anj = anj;
    state.reboot_required = config->reboot_required;
    state.psk_identity = config->psk_identity;
    state.psk_identity_len = config->psk_identity_len;
    state.psk_key = config->psk_key;
    state.psk_key_len = config->psk_key_len;
    state.pending_action = FOTA_MOCK_PENDING_NONE;
    checksum_reset();

    if (anj_dm_fw_update_object_install(anj, &fu_entity, &fu_handlers, NULL)) {
        log(L_ERROR, "Failed to install Firmware Update Object");
        return -1;
    }

    anj_coap_downloader_configuration_t coap_downloader_config = {
        .event_cb = coap_downloader_callback,
        .event_cb_arg = NULL,
        .udp_tx_params = config->coap_udp_tx_params,
    };
    if (anj_coap_downloader_init(&coap_downloader, &coap_downloader_config)) {
        log(L_ERROR, "Failed to initialize CoAP downloader");
        return -1;
    }

    return 0;
}

void fota_mock_process(void) {
    anj_coap_downloader_step(&coap_downloader);

    switch (state.pending_action) {
    case FOTA_MOCK_PENDING_REPORT_SUCCESS:
        state.pending_action = FOTA_MOCK_PENDING_NONE;
        anj_dm_fw_update_object_set_update_result(
                state.anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);
        break;
    case FOTA_MOCK_PENDING_TRIGGER_RESTART:
        state.pending_action = FOTA_MOCK_PENDING_AFTER_RESTART;
        log(L_INFO, "Restarting the client to simulate a device reboot");
        anj_core_restart(state.anj);
        break;
    default:
        break;
    }
}

void fota_mock_on_conn_status_changed(anj_conn_status_t conn_status) {
    (void) conn_status;
    if (state.pending_action == FOTA_MOCK_PENDING_AFTER_RESTART) {
        state.pending_action = FOTA_MOCK_PENDING_NONE;
        anj_dm_fw_update_object_set_update_result(
                state.anj, &fu_entity, ANJ_DM_FW_UPDATE_RESULT_SUCCESS);
    }
}

#else // FOTA_MOCK_ENABLED

int fota_mock_install(anj_t *anj, const fota_mock_config_t *config) {
    (void) anj;
    (void) config;
    log(L_WARNING,
        "Mocked Firmware Update Object not installed: FOTA over CoAP Pull "
        "is disabled in this build configuration");
    return 0;
}

void fota_mock_process(void) {}

void fota_mock_on_conn_status_changed(anj_conn_status_t conn_status) {
    (void) conn_status;
}

#endif // FOTA_MOCK_ENABLED
