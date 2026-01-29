/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_BOOTSTRAP_H
#define ANJ_INTERNAL_BOOTSTRAP_H

#include <anj/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ANJ_INTERNAL_INCLUDE_BOOTSTRAP
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_BOOTSTRAP

#ifdef ANJ_WITH_BOOTSTRAP

// clang-format off
/**
 * @anj_internal_api_do_not_use
 * A group of codes that can be returned by @ref _anj_bootstrap_process.
 */

/** Bootstrap is in progress. */
#define _ANJ_BOOTSTRAP_IN_PROGRESS 0
/** New message to send. */
#define _ANJ_BOOTSTRAP_NEW_REQUEST_TO_SEND 1

/**
 * @anj_internal_api_do_not_use
 * The values below returned by @ref _anj_bootstrap_process indicate that
 * Bootstrap has been completed.
 * Next @ref _anj_bootstrap_process call will start new bootstrap process.
 */

/** Bootstrap has been successfully finished. */
#define _ANJ_BOOTSTRAP_FINISHED 2
/** Bootstrap-related exchange failed. */
#define _ANJ_BOOTSTRAP_ERR_EXCHANGE_ERROR (-1)
/** Bootstrap bootstrap_finish_timeout has expired. */
#define _ANJ_BOOTSTRAP_ERR_BOOTSTRAP_TIMEOUT (-2)
/** A non-timeout communication error. */
#define _ANJ_BOOTSTRAP_ERR_NETWORK (-3)
/**
 * Data model validation failed. There is no correct instance of Server
 * and Security object in the data model.
 */
#define _ANJ_BOOTSTRAP_ERR_DATA_MODEL_VALIDATION (-4)
// clang-format on

/** @anj_internal_api_do_not_use */
typedef struct {
    bool in_progress;
    bool bootstrap_finish_handled;
    anj_time_duration_t bootstrap_lifetime;
    anj_time_monotonic_t bootstrap_finish_timeout;
    int error_code;
    const char *endpoint;
} _anj_bootstrap_ctx_t;

#endif // ANJ_WITH_BOOTSTRAP

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_BOOTSTRAP_H
