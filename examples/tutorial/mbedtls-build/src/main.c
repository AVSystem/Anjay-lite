/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "mbedtls/ssl.h"
#include "mbedtls/version.h"

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log/log.h>

#define log(...) anj_log(example_log, __VA_ARGS__)

int main(void) {
    anj_t anj;
    anj_configuration_t config = {
        .endpoint_name = "test"
    };
    if (anj_core_init(&anj, &config)) {
        log(L_ERROR, "Failed to initialize Anjay Lite");
        return -1;
    }

    log(L_INFO, "Mbed TLS version: %s", MBEDTLS_VERSION_STRING);

    const int *ciphersuites = mbedtls_ssl_list_ciphersuites();
    log(L_INFO, "Available cipher suites:");

    while (*ciphersuites) {
        const char *name = mbedtls_ssl_get_ciphersuite_name(*ciphersuites);
        if (name) {
            log(L_INFO, " - %s", name);
        }
        ciphersuites++;
    }

    return 0;
}
