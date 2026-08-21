/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef FOTA_MOCK_H
#define FOTA_MOCK_H

#include <stdbool.h>
#include <stddef.h>

#include <anj/core.h>
#include <anj/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Configuration for @ref fota_mock_install. */
typedef struct {
    /**
     * If true, reporting Update success is deferred until after a
     * simulated device reboot (@ref anj_core_restart). If false, success is
     * reported immediately.
     */
    bool reboot_required;

    /**
     * Optional PSK credentials for securing CoAPs firmware downloads,
     * independent of any credentials used for the LwM2M Server connection.
     * Both fields must be set to enable PSK; otherwise only coap:// works.
     * Pointed-to memory must outlive the Firmware Update Object.
     */
    const char *psk_identity;
    size_t psk_identity_len;
    const char *psk_key;
    size_t psk_key_len;

    /**
     * Optional UDP transmission parameters for the CoAP downloader used for
     * Pull-mode downloads, independent of the LwM2M Server connection's own
     * parameters. If NULL, the downloader's defaults are used. Only read
     * during @ref fota_mock_install; the pointed-to memory does not need to
     * outlive the call.
     */
    const anj_exchange_udp_tx_params_t *coap_udp_tx_params;
} fota_mock_config_t;

/**
 * Installs a mocked Firmware Update Object (/5) supporting both Pull mode
 * (over CoAP/CoAPs, via @ref anj_coap_downloader_t) and Push mode. Installing
 * the received package and rebooting are mocked out; see @ref
 * fota_mock_config_t::reboot_required.
 *
 * @return 0 on success, a non-zero value in case of an error.
 */
int fota_mock_install(anj_t *anj, const fota_mock_config_t *config);

/**
 * Drives the mocked Firmware Update Object state machine. Call periodically
 * alongside @ref anj_core_step.
 */
void fota_mock_process(void);

/**
 * Must be forwarded from the application's @ref
 * anj_connection_status_callback_t (registered via @ref
 * anj_configuration_t::connection_status_cb).
 */
void fota_mock_on_conn_status_changed(anj_conn_status_t conn_status);

#ifdef __cplusplus
}
#endif

#endif // FOTA_MOCK_H
