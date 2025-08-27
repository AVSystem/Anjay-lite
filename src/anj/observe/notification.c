/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#include "../dm/dm_integration.h"
#include "observe.h"
#include "observe_internal.h"

#ifdef ANJ_WITH_OBSERVE

#    define SUB_ABS(a, b) (((a) > (b)) ? (a) - (b) : (b) - (a))
#    define MAX_OBSERVE_NUMBER 0xFFFFFF

static void get_min_max_period(_anj_attr_notification_t *effective_attr,
                               const _anj_observe_server_state_t *server_state,
                               uint32_t *max_period,
                               uint32_t *min_period) {
    *min_period = effective_attr->has_min_period
                          ? effective_attr->min_period
                          : server_state->default_min_period;
    *max_period = effective_attr->has_max_period
                          ? effective_attr->max_period
                          : server_state->default_max_period;

    if (*min_period > *max_period) {
        *max_period = 0;
    }
}

static int read_resource_value(anj_t *anj,
                               _anj_observation_res_val_t *current_observe_val,
                               anj_data_type_t *current_res_type) {
    anj_res_value_t current_res_value;
    int result;
    bool res_multi = false;
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    if ((result = _anj_dm_observe_read_resource(
                 anj, &current_res_value, current_res_type, &res_multi,
                 &ctx->processing_observation->path))) {
        observe_log(L_WARNING, "Can not read targeted resource value");
        _anj_observe_remove_observation(ctx);
        return result;
    } else {
        _anj_observe_write_anj_res_to_observe_val(
                current_observe_val, &current_res_value, current_res_type);
    }
    return 0;
}

#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
static void
sync_composite_observe_number(const _anj_observe_observation_t *observation) {
    if (observation->prev) {
        _anj_observe_observation_t *iterator = observation->prev;
        while (iterator != observation) {
            iterator->observe_number = observation->observe_number;
            iterator = iterator->prev;
        }
    }
}

static int update_composite_last_sent_value(anj_t *anj) {
    /* There is no need for updating last_sent_value for deactivated
     * observations */
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    if (ctx->processing_observation->observe_active
            && _anj_observe_attribute_has_value_change_condition(
                       &ctx->processing_observation->effective_attr)
            && _anj_dm_observe_is_any_resource_readable(
                       anj, &ctx->processing_observation->path)
                           != ANJ_COAP_CODE_NOT_FOUND) {
        int result;
        anj_data_type_t res_type;
        if ((result = read_resource_value(
                     anj,
                     &ctx->processing_observation->last_sent_value,
                     &res_type))) {
            return result;
        }
    }
    return 0;
}
#    endif // ANJ_WITH_OBSERVE_COMPOSITE

static int update_last_sent_value(anj_t *anj) {
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    if (!ctx->processing_observation->prev) {
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
        if (_anj_observe_attribute_has_value_change_condition(
                    &ctx->processing_observation->effective_attr)) {
            anj_data_type_t res_type;
            return read_resource_value(
                    anj, &ctx->processing_observation->last_sent_value,
                    &res_type);
        }
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    } else {
        const _anj_observe_observation_t *first_observation =
                ctx->processing_observation;
        do {
            int result = update_composite_last_sent_value(anj);
            if (result) {
                return result;
            }

            ctx->processing_observation = ctx->processing_observation->prev;
        } while ((ctx->processing_observation != first_observation));
    }
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    return 0;
}

static void set_notification_flag(_anj_observe_ctx_t *ctx, bool to_send) {
    ctx->processing_observation->notification_to_send = to_send;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    if (ctx->processing_observation->prev) {
        _anj_observe_observation_t *prev_observation =
                ctx->processing_observation->prev;
        while (prev_observation != ctx->processing_observation) {
            assert(prev_observation);
            prev_observation->notification_to_send = to_send;
            prev_observation = prev_observation->prev;
        }
    }
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
}

static void anj_exchange_completion(void *arg_ptr,
                                    const _anj_coap_msg_t *response,
                                    int result) {
    (void) response;
    anj_t *anj = (anj_t *) arg_ptr;
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    assert(ctx->in_progress_type == MSG_TYPE_NOTIFY);
    ctx->already_processed = 0;
    if (result) {
        _anj_dm_observe_terminate_operation(anj);
        _anj_observe_remove_observation(ctx);
        observe_log(L_ERROR, "Failed to send notification");
    } else {
        set_notification_flag(ctx, false);
        observe_log(L_INFO, "Notification sent");
    }
}

static int create_notification(anj_t *anj,
                               _anj_exchange_handlers_t *out_handlers,
                               const _anj_observe_server_state_t *server_state,
                               _anj_coap_msg_t *out_msg) {
    int result;
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    if ((result = update_last_sent_value(anj))) {
        return result;
    }

    *out_handlers = (_anj_exchange_handlers_t) {
        .read_payload = _anj_observe_build_message,
        .completion = anj_exchange_completion,
        .arg = anj
    };
    ctx->in_progress_type = MSG_TYPE_NOTIFY;

    _anj_observe_set_uri_paths_and_format(anj);
#    ifdef ANJ_WITH_LWM2M12
    bool con_attr = false;
    bool has_con_attr = false;
#        ifdef ANJ_WITH_OBSERVE_COMPOSITE
    if (ctx->processing_observation->prev) {
        const _anj_observe_observation_t *iterator =
                ctx->processing_observation;
        do {
            if (iterator->effective_attr.has_con) {
                con_attr = con_attr || iterator->effective_attr.con;
                has_con_attr = true;
            }
            iterator = iterator->prev;
        } while (iterator != ctx->processing_observation && !con_attr);
        con_attr = has_con_attr ? con_attr : server_state->default_con;
    } else
#        endif // ANJ_WITH_OBSERVE_COMPOSITE
    {
        has_con_attr = ctx->processing_observation->effective_attr.has_con;
        con_attr = has_con_attr
                           ? ctx->processing_observation->effective_attr.con
                           : server_state->default_con;
    }

    out_msg->operation =
            con_attr ? ANJ_OP_INF_CON_NOTIFY : ANJ_OP_INF_NON_CON_NOTIFY;
#    else  // ANJ_WITH_LWM2M12
    out_msg->operation = ANJ_OP_INF_NON_CON_NOTIFY;
#    endif // ANJ_WITH_LWM2M12
    // send a confirmable notification at least once every 24 hours
    if (anj_time_real_now()
            >= ctx->processing_observation->next_conf_notify_timestamp) {
        out_msg->operation = ANJ_OP_INF_CON_NOTIFY;
    }

    // set token
    memcpy(&out_msg->token, &ctx->processing_observation->token,
           sizeof(_anj_coap_token_t));
    ctx->processing_observation->observe_number =
            (ctx->processing_observation->observe_number + 1)
            % (MAX_OBSERVE_NUMBER + 1);
    out_msg->observe_number = ctx->processing_observation->observe_number;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    sync_composite_observe_number(ctx->processing_observation);
#    endif
    _anj_observe_refresh_timestamp(ctx, anj_time_real_now(),
                                   out_msg->operation == ANJ_OP_INF_CON_NOTIFY);
    return 0;
}

static uint64_t
calculate_next_notify_check_timestamp(_anj_observe_observation_t *observation,
                                      uint32_t max_period) {
    if (max_period == 0) {
        return ANJ_TIME_UNDEFINED;
    }

    return observation->last_notify_timestamp + max_period * 1000;
}

static int
observe_process_or_get_time(anj_t *anj,
                            _anj_exchange_handlers_t *out_handlers,
                            const _anj_observe_server_state_t *server_state,
                            _anj_coap_msg_t *out_msg,
                            uint64_t *time_to_next_notif,
                            bool get_time) {
    uint32_t min_period;
    uint32_t max_period;
    uint64_t elapsed_time;
    uint64_t current_time = anj_time_real_now();
    uint64_t tmp_time_to_next_notif;
    uint64_t next_notify_check_timestamp;
    int ret_val = 0;

    if (get_time) {
        *time_to_next_notif = ANJ_TIME_UNDEFINED;
    }
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        ctx->processing_observation = &ctx->observations[i];
        if (!ctx->processing_observation->observe_active
                || (ctx->processing_observation->ssid != server_state->ssid)) {
            continue;
        }

        /* If this condition is met, it means that the system time has been
         * modified, and for this reason, we send a notification regardless of
         * the attributes */
        if (current_time < ctx->processing_observation->last_notify_timestamp) {
            if (get_time) {
                *time_to_next_notif = 0;
            } else {
                ret_val = create_notification(anj, out_handlers, server_state,
                                              out_msg);
            }
            break;
        }

        get_min_max_period(&ctx->processing_observation->effective_attr,
                           server_state, &max_period, &min_period);

        next_notify_check_timestamp = calculate_next_notify_check_timestamp(
                ctx->processing_observation, max_period);

        if (get_time && next_notify_check_timestamp != ANJ_TIME_UNDEFINED) {
            if (current_time > next_notify_check_timestamp) {
                tmp_time_to_next_notif = 0;
            } else {
                tmp_time_to_next_notif =
                        next_notify_check_timestamp - current_time;
            }

            if (*time_to_next_notif > tmp_time_to_next_notif) {
                *time_to_next_notif = tmp_time_to_next_notif;
            }
        }

        elapsed_time = (current_time
                        - ctx->processing_observation->last_notify_timestamp)
                       / 1000;

        if (min_period > elapsed_time) {
            if (get_time && ctx->processing_observation->notification_to_send) {
                tmp_time_to_next_notif = min_period * 1000
                                         - (current_time
                                            - ctx->processing_observation
                                                      ->last_notify_timestamp);
                if (*time_to_next_notif > tmp_time_to_next_notif) {
                    *time_to_next_notif = tmp_time_to_next_notif;
                }
            }
            continue;
        }

        if ((max_period && max_period <= elapsed_time)
                || ctx->processing_observation->notification_to_send) {
            if (get_time) {
                *time_to_next_notif = 0;
            } else {
                ret_val = create_notification(anj, out_handlers, server_state,
                                              out_msg);
            }
            break;
        }
    }
    return ret_val;
}

int _anj_observe_process(anj_t *anj,
                         _anj_exchange_handlers_t *out_handlers,
                         const _anj_observe_server_state_t *server_state,
                         _anj_coap_msg_t *out_msg) {
    assert(anj && server_state && out_msg && out_handlers);
    assert(server_state->ssid > 0 && server_state->ssid < UINT16_MAX);

    return observe_process_or_get_time(anj, out_handlers, server_state, out_msg,
                                       NULL, false);
}

int anj_observe_time_to_next_notification(
        anj_t *anj,
        const _anj_observe_server_state_t *server_state,
        uint64_t *time_to_next_notification) {
    assert(anj && server_state && time_to_next_notification);
    assert(server_state->ssid > 0 && server_state->ssid < UINT16_MAX);

    /* We might be in middle of handling notification when this function is
     * called */
    _anj_observe_observation_t *previous_processing_observation =
            anj->observe_ctx.processing_observation;
    int ret = observe_process_or_get_time(anj, NULL, server_state, NULL,
                                          time_to_next_notification, true);
    anj->observe_ctx.processing_observation = previous_processing_observation;
    return ret;
}

// a > b
static bool
observation_value_greater_than_value(const _anj_observation_res_val_t *a,
                                     const double *b,
                                     const anj_data_type_t *type) {
    switch (*type) {
    case ANJ_DATA_TYPE_INT:
        return (double) a->int_value > *b;
    case ANJ_DATA_TYPE_UINT:
        return (double) a->uint_value > *b;
    case ANJ_DATA_TYPE_DOUBLE:
        return a->double_value > *b;
    default:
        ANJ_UNREACHABLE("incorrect data type");
    }
}

// a > b
static bool
value_greater_than_observation_value(const double *a,
                                     const _anj_observation_res_val_t *b,
                                     const anj_data_type_t *type) {
    switch (*type) {
    case ANJ_DATA_TYPE_INT:
        return (double) b->int_value < *a;
    case ANJ_DATA_TYPE_UINT:
        return (double) b->uint_value < *a;
    case ANJ_DATA_TYPE_DOUBLE:
        return b->double_value < *a;
    default:
        ANJ_UNREACHABLE("incorrect data type");
    }
}

// abs(a - b) >= b
static bool observation_value_difference_greater_or_equal_to_value(
        const _anj_observation_res_val_t *a,
        const _anj_observation_res_val_t *b,
        const double *c,
        const anj_data_type_t *type) {
    switch (*type) {
    case ANJ_DATA_TYPE_INT:
        return (double) SUB_ABS(a->int_value, b->int_value) >= *c;
    case ANJ_DATA_TYPE_UINT:
        return (double) SUB_ABS(a->uint_value, b->uint_value) >= *c;
    case ANJ_DATA_TYPE_DOUBLE:
        return SUB_ABS(a->double_value, b->double_value) >= *c;
    default:
        ANJ_UNREACHABLE("incorrect data type");
    }
}

static bool do_observation_value_crossed_threshold(
        const _anj_observation_res_val_t *prev_val,
        const _anj_observation_res_val_t *curr_val,
        const double *th,
        const anj_data_type_t *type) {
    return (observation_value_greater_than_value(prev_val, th, type)
            && value_greater_than_observation_value(th, curr_val, type))
           || (observation_value_greater_than_value(curr_val, th, type)
               && value_greater_than_observation_value(th, prev_val, type));
}

#    define ATTRIBUTES_NOT_MET -1

static int check_attributes(anj_t *anj,
                            _anj_observation_res_val_t *current_observe_val,
                            anj_data_type_t *current_res_type,
                            bool *already_read) {
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    _anj_observe_observation_t *observation = ctx->processing_observation;
    _anj_attr_notification_t *attr =
            &ctx->processing_observation->effective_attr;
    if (_anj_observe_attribute_has_value_change_condition(attr)) {
        assert(anj_uri_path_is(&observation->path, ANJ_ID_RID)
               || anj_uri_path_is(&observation->path, ANJ_ID_RIID));
        if (!*already_read) {
            int result;
            if ((result = read_resource_value(anj, current_observe_val,
                                              current_res_type))) {
                /* anj_observe_read_resource_t should return negative
                 * value in case of error */
                assert(result != ATTRIBUTES_NOT_MET);
                return result;
            } else {
                *already_read = true;
            }
        }

        if (!((attr->has_less_than
               && do_observation_value_crossed_threshold(
                          &observation->last_sent_value, current_observe_val,
                          &attr->less_than, current_res_type))
              ||
#    ifdef ANJ_WITH_LWM2M12
              (attr->has_edge
               && (attr->edge ? !observation->last_sent_value.bool_value
                                        && current_observe_val->bool_value
                              : observation->last_sent_value.bool_value
                                        && !current_observe_val->bool_value))
              ||
#    endif // ANJ_WITH_LWM2M12
              (attr->has_greater_than
               && do_observation_value_crossed_threshold(
                          &observation->last_sent_value, current_observe_val,
                          &attr->greater_than, current_res_type))
              || (attr->has_step
                  && observation_value_difference_greater_or_equal_to_value(
                             &observation->last_sent_value, current_observe_val,
                             &attr->step, current_res_type)))) {
            return ATTRIBUTES_NOT_MET;
        }
    }
    return 0;
}

int anj_observe_data_model_changed(anj_t *anj,
                                   const anj_uri_path_t *path,
                                   anj_observe_change_type_t change_type,
                                   uint16_t ssid) {
    assert(anj && path);
    assert(ssid < UINT16_MAX);
    assert((change_type == ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED
            && anj_uri_path_has(path, ANJ_ID_RID))
           || ((change_type == ANJ_OBSERVE_CHANGE_TYPE_ADDED
                || change_type == ANJ_OBSERVE_CHANGE_TYPE_DELETED)
               && !anj_uri_path_is(path, ANJ_ID_RID)));

    int ret_val = 0;
    int result = 0;
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    /* We might be in middle of handling notification when this function is
     * called */
    _anj_observe_observation_t *previous_processing_observation =
            ctx->processing_observation;
    switch (change_type) {
    case ANJ_OBSERVE_CHANGE_TYPE_ADDED:
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
        for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
            if (ctx->observations[i].ssid && ctx->observations[i].prev
                    && !anj_uri_path_outside_base(&ctx->observations[i].path,
                                                  path)) {
                ctx->processing_observation = &ctx->observations[i];
                /* At the time of adding the observation, the path may not have
                 * existed in the data model, so we need to check this now */
                if ((result =
                             _anj_observe_check_if_value_condition_attributes_should_be_disabled(
                                     anj, ctx->processing_observation))) {
                    _anj_observe_remove_observation(ctx);
                    ret_val = result;
                }
                _anj_observe_verify_effective_attributes(
                        ctx->processing_observation);
            }
        }
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    /* fall through */
    case ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED: {
        bool already_read = false;
        _anj_observation_res_val_t observe_value;
        anj_data_type_t res_type;
        for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
            if (!ctx->observations[i].notification_to_send
                    && ctx->observations[i].observe_active
                    && (ssid == 0 ? ctx->observations[i].ssid
                                  : ctx->observations[i].ssid != ssid)
                    && (!anj_uri_path_outside_base(path,
                                                   &ctx->observations[i].path)
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
                        /* If it is a composite observation, it is also
                           necessary to check the paths that may not have
                           existed in the data model before. */
                        || (change_type == ANJ_OBSERVE_CHANGE_TYPE_ADDED
                            && ctx->observations[i].prev
                            && !anj_uri_path_outside_base(
                                       &ctx->observations[i].path, path)
                            /* If this function returns an error different than
                               ANJ_COAP_CODE_NOT_FOUND then the observation
                               should be removed in the
                               _anj_observe_attribute_has_value_change_condition
                               function. */
                            && !_anj_dm_observe_is_any_resource_readable(
                                       anj, &ctx->observations[i].path))
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
                                )) {
                ctx->processing_observation = &ctx->observations[i];
                if (change_type == ANJ_OBSERVE_CHANGE_TYPE_VALUE_CHANGED) {
                    result = check_attributes(anj, &observe_value, &res_type,
                                              &already_read);
                    if (result == ATTRIBUTES_NOT_MET) {
                        continue;
                    } else if (result) {
                        ret_val = result;
                        continue;
                    }
                }
                ctx->processing_observation->notification_to_send = true;
            }
        }
        break;
    }
    case ANJ_OBSERVE_CHANGE_TYPE_DELETED: {
        for (size_t i = 0; i < ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER; i++) {
            if (ctx->attributes_storage[i].ssid
                    && !anj_uri_path_outside_base(
                               &ctx->attributes_storage[i].path, path)) {
                ctx->attributes_storage[i].ssid = 0;
            }
        }
        for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
            if (ctx->observations[i].ssid
                    && !anj_uri_path_outside_base(&ctx->observations[i].path,
                                                  path)
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
                    && !ctx->observations[i].prev
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
            ) {
                ctx->processing_observation = &ctx->observations[i];
                _anj_observe_remove_observation(ctx);
            }
        }
        break;
    }
    default:
        ANJ_UNREACHABLE("incorrect operation type");
    }

    ctx->processing_observation = previous_processing_observation;
    return ret_val;
}

#endif // ANJ_WITH_OBSERVE
