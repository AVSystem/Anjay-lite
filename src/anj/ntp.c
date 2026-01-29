/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 52

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/compat/time.h>
#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/log.h>
#include <anj/ntp.h>
#include <anj/time.h>
#include <anj/utils.h>

#ifdef ANJ_WITH_PERSISTENCE
#    include <anj/persistence.h>
#endif // ANJ_WITH_PERSISTENCE

#include "core/srv_conn.h"
#include "utils.h"

#ifdef ANJ_WITH_NTP

#    define ntp_log(...) anj_log(ntp, __VA_ARGS__)

#    define NTP_PORT "123"

#    define INTERNAL_STATE_INIT 0U
#    define INTERNAL_STATE_IDLE 1U
#    define INTERNAL_STATE_CONNECTING 2U
#    define INTERNAL_STATE_SENDING_REQUEST 3U
#    define INTERNAL_STATE_WAITING_RESPONSE 4U
#    define INTERNAL_STATE_DISCONNECTING 5U

#    define NTP_OBJ_OID 3415

#    define NTP_RESOURCES_COUNT 5

enum {
    RID_SERVER_ADDRESS = 1,
    RID_BACKUP_SERVER_ADDRESS = 2,
    RID_NTP_PERIOD = 3,
    RID_LAST_TIME_SYNC = 4,
    RID_TIME_SYNC_ERROR = 5,
};

enum {
    RID_SERVER_ADDRESS_IDX = 0,
    RID_BACKUP_SERVER_ADDRESS_IDX,
    RID_NTP_PERIOD_IDX,
    RID_LAST_TIME_SYNC_IDX,
    RID_TIME_SYNC_ERROR_IDX,
    _RID_LAST
};

ANJ_STATIC_ASSERT(_RID_LAST == NTP_RESOURCES_COUNT,
                  ntp_obj_resources_count_mismatch);

static const anj_dm_res_t NTP_RES[NTP_RESOURCES_COUNT] = {
    [RID_SERVER_ADDRESS_IDX] = {
        .rid = RID_SERVER_ADDRESS,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_RW
    },
    [RID_BACKUP_SERVER_ADDRESS_IDX] = {
        .rid = RID_BACKUP_SERVER_ADDRESS,
        .type = ANJ_DATA_TYPE_STRING,
        .kind = ANJ_DM_RES_RW
    },
    [RID_NTP_PERIOD_IDX] = {
        .rid = RID_NTP_PERIOD,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_RW
    },
    [RID_LAST_TIME_SYNC_IDX] = {
        .rid = RID_LAST_TIME_SYNC,
        .type = ANJ_DATA_TYPE_INT,
        .kind = ANJ_DM_RES_R
    },
    [RID_TIME_SYNC_ERROR_IDX] = {
        .rid = RID_TIME_SYNC_ERROR,
        .type = ANJ_DATA_TYPE_BOOL,
        .kind = ANJ_DM_RES_R
    }
};

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) iid;
    (void) riid;

    anj_ntp_t *ctx = ANJ_CONTAINER_OF(obj, anj_ntp_t, ntp_object);

    switch (rid) {
    case RID_SERVER_ADDRESS:
        out_value->bytes_or_string.data = ctx->server_address;
        break;
    case RID_BACKUP_SERVER_ADDRESS:
        out_value->bytes_or_string.data = ctx->backup_server_address;
        break;
    case RID_NTP_PERIOD:
        out_value->int_value = (int64_t) ctx->period_hours;
        break;
    case RID_LAST_TIME_SYNC:
        out_value->int_value = anj_time_duration_to_scalar(
                anj_time_monotonic_diff(anj_time_monotonic_now(),
                                        ctx->last_successful_sync),
                ANJ_TIME_UNIT_HOUR);
        break;
    case RID_TIME_SYNC_ERROR:
        out_value->bool_value = ctx->time_sync_error;
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }
    return 0;
}

static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) iid;
    (void) riid;

    anj_ntp_t *ctx = ANJ_CONTAINER_OF(obj, anj_ntp_t, ntp_object);

    switch (rid) {
    case RID_SERVER_ADDRESS: {
        int res =
                anj_dm_write_string_chunked(value, ctx->server_address,
                                            ANJ_NTP_SERVER_ADDR_MAX_LEN, NULL);
        if (res != 0) {
            return res;
        }
        // primary server resource must be set to non-empty value
        if (strlen(ctx->server_address) == 0) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        break;
    }
    case RID_BACKUP_SERVER_ADDRESS:
        return anj_dm_write_string_chunked(value, ctx->backup_server_address,
                                           ANJ_NTP_SERVER_ADDR_MAX_LEN, NULL);
        break;
    case RID_NTP_PERIOD:
        if (value->int_value < 0 || value->int_value > UINT32_MAX) {
            return ANJ_DM_ERR_BAD_REQUEST;
        }
        ctx->period_hours = (uint32_t) value->int_value;
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }

    return 0;
}

static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
    (void) anj;
    anj_ntp_t *ctx = ANJ_CONTAINER_OF(obj, anj_ntp_t, ntp_object);
    memcpy(ctx->cache_server_address, ctx->server_address,
           ANJ_NTP_SERVER_ADDR_MAX_LEN);
    memcpy(ctx->cache_backup_server_address, ctx->backup_server_address,
           ANJ_NTP_SERVER_ADDR_MAX_LEN);
    ctx->cache_period_hours = ctx->period_hours;
    return 0;
}

static void transaction_end(anj_t *anj,
                            const anj_dm_obj_t *obj,
                            anj_dm_transaction_result_t result) {
    (void) anj;
    anj_ntp_t *ctx = ANJ_CONTAINER_OF(obj, anj_ntp_t, ntp_object);

    if (result == ANJ_DM_TRANSACTION_FAILURE) {
        // restore cached values
        memcpy(ctx->server_address, ctx->cache_server_address,
               ANJ_NTP_SERVER_ADDR_MAX_LEN);
        memcpy(ctx->backup_server_address, ctx->cache_backup_server_address,
               ANJ_NTP_SERVER_ADDR_MAX_LEN);
        ctx->period_hours = ctx->cache_period_hours;
    }
#    ifdef ANJ_WITH_PERSISTENCE
    else {
        ctx->event_cb(ctx->event_cb_arg, ctx, ANJ_NTP_STATUS_OBJECT_UPDATED,
                      ANJ_TIME_REAL_ZERO);
    }
#    endif // ANJ_WITH_PERSISTENCE
}

static const anj_dm_handlers_t HANDLERS = {
    .res_read = res_read,
    .res_write = res_write,
    .transaction_begin = transaction_begin,
    .transaction_end = transaction_end,
};

static const anj_dm_obj_inst_t NTP_INST = {
    .iid = 0,
    .resources = NTP_RES,
    .res_count = NTP_RESOURCES_COUNT
};

#    ifdef ANJ_WITH_PERSISTENCE
static const uint8_t g_persistence_header[] = { 'N', 'T', 'P',
                                                0x01 }; // version

static int instance_persistence(anj_ntp_t *ntp,
                                const anj_persistence_context_t *ctx) {
    if (anj_persistence_u32(ctx, &ntp->period_hours)
            || anj_persistence_string(ctx, ntp->server_address,
                                      ANJ_NTP_SERVER_ADDR_MAX_LEN)
            || anj_persistence_string(ctx, ntp->backup_server_address,
                                      ANJ_NTP_SERVER_ADDR_MAX_LEN)) {
        return -1;
    }
    return 0;
}

int anj_ntp_obj_store(anj_ntp_t *ntp, const anj_persistence_context_t *ctx) {
    assert(ntp && ctx);
    assert(anj_persistence_direction(ctx) == ANJ_PERSISTENCE_STORE);

    if (anj_persistence_magic(ctx, g_persistence_header,
                              sizeof(g_persistence_header))) {
        return -1;
    }
    if (instance_persistence(ntp, ctx)) {
        ntp_log(L_ERROR, "Failed to store NTP object state");
        return -1;
    }
    ntp_log(L_INFO, "NTP object state stored successfully");
    return 0;
}

int anj_ntp_obj_restore(anj_ntp_t *ntp, const anj_persistence_context_t *ctx) {
    assert(ntp && ctx);
    assert(anj_persistence_direction(ctx) == ANJ_PERSISTENCE_RESTORE);

    if (anj_persistence_magic(ctx, g_persistence_header,
                              sizeof(g_persistence_header))) {
        return -1;
    }
    if (instance_persistence(ntp, ctx)) {
        ntp_log(L_ERROR, "Failed to restore NTP object state");
        return -1;
    }
    ntp_log(L_INFO, "NTP object state restored successfully");
    return 0;
}
#    endif // ANJ_WITH_PERSISTENCE

static bool ntp_period_exceeded(anj_time_monotonic_t last_sync,
                                uint32_t period_hours) {
    // period of 0 means NTP automatic synchronization is disabled
    if (period_hours == 0) {
        return false;
    }
    int64_t elapsed_hours = anj_time_duration_to_scalar(
            anj_time_monotonic_diff(anj_time_monotonic_now(), last_sync),
            ANJ_TIME_UNIT_HOUR);
    return elapsed_hours >= (int64_t) period_hours;
}

static int connect_with_server(anj_ntp_t *ntp) {
    anj_net_config_t net_cfg = {
        .raw_socket_config = ntp->net_socket_cfg
    };
    return _anj_srv_conn_connect(&ntp->connection_ctx,
                                 ANJ_NET_BINDING_UDP,
                                 &net_cfg,
                                 ntp->using_backup_server
                                         ? ntp->backup_server_address
                                         : ntp->server_address,
                                 NTP_PORT);
}

static void handle_retries(anj_ntp_t *ntp) {
    ntp->last_sync_time = anj_time_monotonic_now();
    ntp->attempts++;
    if (ntp->attempts >= ntp->max_attempts) {
        if (!ntp->using_backup_server
                && strlen(ntp->backup_server_address) > 0) {
            // switch to backup server
            ntp_log(L_WARNING, "Switching to backup NTP server");
            ntp->using_backup_server = true;
            ntp->attempts = 0;
            ntp->internal_state = INTERNAL_STATE_CONNECTING;
        } else {
            // all attempts exhausted
            ntp_log(L_ERROR, "NTP synchronization failed");
            ntp->internal_state = INTERNAL_STATE_IDLE;
            ntp->time_sync_error = true;
            ntp->event_cb(ntp->event_cb_arg, ntp,
                          ANJ_NTP_STATUS_FINISHED_WITH_ERROR,
                          ANJ_TIME_REAL_ZERO);
        }
    } else {
        // retry with the same server
        ntp_log(L_INFO, "Retrying NTP synchronization (attempt %u of %u)",
                ntp->attempts + 1, ntp->max_attempts);
        ntp->internal_state = INTERNAL_STATE_CONNECTING;
    }
}

static void prepare_ntp_request(anj_ntp_t *ntp) {
    memset(ntp->ntp_msg, 0, _ANJ_NTP_MSG_SIZE);
    ntp->ntp_msg[0] = 0x23; // LI = 0, Version = 4, Mode = 3 (client)
}

static anj_time_real_t parse_ntp_response(anj_ntp_t *ntp) {
    uint32_t seconds = 0;
    memcpy(&seconds, &ntp->ntp_msg[40], sizeof(seconds));
    seconds = _anj_convert_be32(seconds);

    // NTP timestamp starts from 1900, Unix timestamp starts from 1970.
    // This is 70 years difference including 17 leap years:
    // 70 * 365 + 17 = 25567 days -> hours -> seconds.
    const uint64_t NTP_UNIX_EPOCH_DIFF = 2208988800ULL;

    // 32-bit NTP seconds can wrap around every ~136 years (NTP "era").
    // We "unfold" the value into 64-bit space by assuming that if the
    // raw 32-bit value is lower than the Unix epoch offset, we are
    // already in the next era and need to add 2^32 seconds.
    uint64_t ntp_sec = (uint64_t) seconds;
    if (ntp_sec < NTP_UNIX_EPOCH_DIFF) {
        ntp_sec += (1ULL << 32);
    }

    // Convert fractional part to microseconds.
    // "fraction" is a 32-bit fraction of a second, so fraction / 2^32
    // gives the fractional part in seconds (always < 1.0).
    uint32_t fraction = 0;
    memcpy(&fraction, &ntp->ntp_msg[44], sizeof(fraction));
    fraction = _anj_convert_be32(fraction);
    const int64_t NTP_FRAC_SCALE = 4294967296LL; // 2^32
    const int64_t US_PER_SEC = 1000000LL;
    int64_t frac_us = ((int64_t) fraction * US_PER_SEC) / NTP_FRAC_SCALE;

    int64_t unix_sec = (int64_t) (ntp_sec - NTP_UNIX_EPOCH_DIFF);
    int64_t total_us = unix_sec * US_PER_SEC + frac_us;

    return anj_time_real_from_duration(
            anj_time_duration_new(total_us, ANJ_TIME_UNIT_US));
}

static void finalize_ntp_synchronization(anj_ntp_t *ntp,
                                         anj_time_real_t synchronized_time) {
    ntp->synchronized = true;
    ntp->time_sync_error = false;
    ntp->last_successful_sync = anj_time_monotonic_now();
    ntp->last_sync_time = ntp->last_successful_sync;
    ntp->event_cb(ntp->event_cb_arg, ntp, ANJ_NTP_STATUS_FINISHED_SUCCESSFULLY,
                  synchronized_time);

    // extract milliseconds parts for logging
    int64_t s_part =
            anj_time_real_to_scalar(synchronized_time, ANJ_TIME_UNIT_S);
    int64_t ms_part =
            anj_time_real_to_scalar(synchronized_time, ANJ_TIME_UNIT_MS)
            - s_part * 1000;
    ntp_log(L_INFO,
            "NTP synchronization successful, synchronized time: %" PRId64
            ".%03" PRId64 " s",
            s_part,
            ms_part);
}

void anj_ntp_step(anj_ntp_t *ntp) {
    assert(ntp && ntp->event_cb);

    switch (ntp->internal_state) {
    case INTERNAL_STATE_INIT: {
        // anj_ntp_start() may be called in callback, so set state to IDLE first
        ntp->internal_state = INTERNAL_STATE_IDLE;
        ntp->event_cb(ntp->event_cb_arg, ntp, ANJ_NTP_STATUS_INITIAL,
                      ANJ_TIME_REAL_ZERO);
        break;
    }
    case INTERNAL_STATE_IDLE: {
        // check if new synchronization should be started
        if (ntp_period_exceeded(ntp->last_sync_time, ntp->period_hours)) {
            // reset last_sync_time time to avoid multiple calls, last_sync_time
            // is pointing to the start of synchronization attempt
            ntp->last_sync_time = anj_time_monotonic_now();
            ntp->event_cb(ntp->event_cb_arg, ntp,
                          ANJ_NTP_STATUS_PERIOD_EXCEEDED, ANJ_TIME_REAL_ZERO);
        }
        break;
    }
    case INTERNAL_STATE_CONNECTING: {
        int result = connect_with_server(ntp);
        if (anj_net_is_inprogress(result)) {
            break;
        }
        if (!anj_net_is_ok(result)) {
            ntp->internal_state = INTERNAL_STATE_DISCONNECTING;
            ntp_log(L_ERROR, "Failed to connect to server: %d", result);
            break;
        }
        ntp_log(L_TRACE, "Connected to NTP server");

        prepare_ntp_request(ntp);
        ntp->internal_state = INTERNAL_STATE_SENDING_REQUEST;
        break;
    }
    case INTERNAL_STATE_SENDING_REQUEST: {
        int result = _anj_srv_conn_send(&ntp->connection_ctx, ntp->ntp_msg,
                                        _ANJ_NTP_MSG_SIZE);
        if (anj_net_is_inprogress(result)) {
            break;
        }
        if (!anj_net_is_ok(result)) {
            ntp->internal_state = INTERNAL_STATE_DISCONNECTING;
            ntp_log(L_ERROR, "Failed to send NTP request: %d", result);
            break;
        }
        ntp_log(L_TRACE, "NTP request sent");
        ntp->internal_state = INTERNAL_STATE_WAITING_RESPONSE;
        ntp->recv_wait_start_time = anj_time_monotonic_now();
        break;
    }
    case INTERNAL_STATE_WAITING_RESPONSE: {
        size_t recv_len = 0;
        int result = _anj_srv_conn_receive(&ntp->connection_ctx, ntp->ntp_msg,
                                           &recv_len, _ANJ_NTP_BUFF_SIZE);
        // check for timeout
        if (anj_time_monotonic_gt(
                    anj_time_monotonic_now(),
                    anj_time_monotonic_add(ntp->recv_wait_start_time,
                                           ntp->response_timeout))) {
            ntp->internal_state = INTERNAL_STATE_DISCONNECTING;
            ntp_log(L_ERROR, "NTP response timed out");
            break;
        }

        if (anj_net_is_inprogress(result) || anj_net_is_again(result)) {
            break;
        }
        if (!anj_net_is_ok(result)) {
            ntp->internal_state = INTERNAL_STATE_DISCONNECTING;
            ntp_log(L_ERROR, "Failed to receive NTP response: %d", result);
            break;
        }
        if (recv_len < _ANJ_NTP_MSG_SIZE) {
            ntp->internal_state = INTERNAL_STATE_DISCONNECTING;
            ntp_log(L_ERROR, "Received NTP response is too short");
            break;
        }

        finalize_ntp_synchronization(ntp, parse_ntp_response(ntp));
        ntp->internal_state = INTERNAL_STATE_DISCONNECTING;
        break;
    }
    case INTERNAL_STATE_DISCONNECTING: {
        int result = _anj_srv_conn_close(&ntp->connection_ctx, true);
        if (anj_net_is_inprogress(result)) {
            break;
        }
        if (!anj_net_is_ok(result)) {
            // error while closing connection but does not affect state
            ntp_log(L_ERROR, "Error while closing connection: %d", result);
        } else {
            ntp_log(L_TRACE, "Connection closed");
        }

        if (ntp->synchronized) {
            // completed successfully
            ntp->internal_state = INTERNAL_STATE_IDLE;
            break;
        }
        handle_retries(ntp);
        break;
    }
    default:
        break;
    }
}

int anj_ntp_init(anj_t *anj,
                 anj_ntp_t *ntp,
                 const anj_ntp_configuration_t *config) {
    assert(anj && ntp && config);
    ntp_log(L_DEBUG, "Initializing NTP module");
    memset(ntp, 0, sizeof(*ntp));

    /**
     * Validate configuration: server_address must be set and non-empty,
     * backup_server_address is optional but both addresses must fit in the
     * buffer, event_cb must be set.
     * */
    if (!config->ntp_server_address || strlen(config->ntp_server_address) == 0
            || !config->event_cb
            || (config->backup_ntp_server_address
                && strlen(config->backup_ntp_server_address)
                           >= ANJ_NTP_SERVER_ADDR_MAX_LEN)
            || strlen(config->ntp_server_address)
                           >= ANJ_NTP_SERVER_ADDR_MAX_LEN) {
        ntp_log(L_ERROR, "Invalid configuration");
        return ANJ_NTP_ERR_CONFIGURATION;
    }

    // copy configuration
    ntp->event_cb = config->event_cb;
    ntp->event_cb_arg = config->event_cb_arg;
    // if not set, default to 1 attempt
    ntp->max_attempts = config->attempts ? config->attempts : 1;
    // if not set, default to 5 seconds
    ntp->response_timeout = (anj_time_duration_eq(config->response_timeout,
                                                  ANJ_TIME_DURATION_ZERO))
                                    ? anj_time_duration_new(5, ANJ_TIME_UNIT_S)
                                    : config->response_timeout;
    ntp->period_hours = config->ntp_period_hours;
    if (config->net_socket_cfg) {
        ntp->net_socket_cfg = *config->net_socket_cfg;
    }
    strcpy(ntp->server_address, config->ntp_server_address);
    if (config->backup_ntp_server_address) {
        strcpy(ntp->backup_server_address, config->backup_ntp_server_address);
    }

    // set internal variables
    ntp->internal_state = INTERNAL_STATE_INIT;
    ntp->last_sync_time = anj_time_monotonic_now();
    ntp->last_successful_sync = anj_time_monotonic_now();
    ntp->time_sync_error = false;

    // initialize NTP object
    ntp->ntp_object = (anj_dm_obj_t) {
        .oid = NTP_OBJ_OID,
        .version = "2.0",
        .max_inst_count = 1,
        .insts = &NTP_INST,
        .handlers = &HANDLERS
    };
    // install NTP object into Anjay instance
    if (anj_dm_add_obj(anj, &ntp->ntp_object)) {
        ntp_log(L_ERROR, "Failed to add NTP object");
        return ANJ_NTP_ERR_OBJECT_CREATION_FAILED;
    }
    return 0;
}

int anj_ntp_start(anj_ntp_t *ntp) {
    assert(ntp);
    if (ntp->internal_state != INTERNAL_STATE_IDLE
            && ntp->internal_state != INTERNAL_STATE_INIT) {
        ntp_log(L_ERROR, "NTP synchronization already in progress");
        return ANJ_NTP_ERR_IN_PROGRESS;
    }
    ntp_log(L_INFO, "Starting NTP synchronization");
    ntp->internal_state = INTERNAL_STATE_CONNECTING;
    ntp->using_backup_server = false;
    ntp->synchronized = false;
    ntp->attempts = 0;
    ntp->event_cb(ntp->event_cb_arg, ntp, ANJ_NTP_STATUS_IN_PROGRESS,
                  ANJ_TIME_REAL_ZERO);
    return 0;
}

void anj_ntp_terminate(anj_ntp_t *ntp) {
    assert(ntp);
    if (ntp->internal_state == INTERNAL_STATE_IDLE
            || ntp->internal_state == INTERNAL_STATE_INIT) {
        // there is no ongoing operation to terminate
        return;
    }
    ntp_log(L_INFO, "Terminating NTP synchronization");
    ntp->internal_state = INTERNAL_STATE_DISCONNECTING;
}

#endif // ANJ_WITH_NTP
