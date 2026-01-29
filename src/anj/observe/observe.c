/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 46

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <anj/compat/time.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log.h>
#include <anj/time.h>
#include <anj/utils.h>

#include "../coap/coap.h"
#include "../dm/dm_integration.h"
#include "../io/io.h"
#include "../utils.h"
#include "observe.h"
#include "observe_internal.h"

#ifdef ANJ_WITH_OBSERVE

static uint8_t map_err_to_coap_code(int error_code) {
    switch (error_code) {
    case ANJ_COAP_CODE_BAD_REQUEST:
        return ANJ_COAP_CODE_BAD_REQUEST;
    case ANJ_COAP_CODE_UNAUTHORIZED:
        return ANJ_COAP_CODE_UNAUTHORIZED;
    case ANJ_COAP_CODE_NOT_FOUND:
        return ANJ_COAP_CODE_NOT_FOUND;
    case ANJ_COAP_CODE_METHOD_NOT_ALLOWED:
        return ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
    case ANJ_COAP_CODE_NOT_ACCEPTABLE:
        return ANJ_COAP_CODE_NOT_ACCEPTABLE;
    case ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT:
        return ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT;
    default:
        return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    }
}

static _anj_observe_observation_t *
find_observation(_anj_observe_ctx_t *ctx,
                 uint16_t ssid,
                 const _anj_coap_token_t *token) {
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        if (ctx->observations[i].ssid == ssid
                && _anj_tokens_equal(&ctx->observations[i].token, token)) {
            return &ctx->observations[i];
        }
    }
    return NULL;
}

static _anj_observe_observation_t *
find_spot_for_new_observation(_anj_observe_ctx_t *ctx) {
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        // ssid can be 0 only if the observation is not used
        if (ctx->observations[i].ssid == 0) {
            return &ctx->observations[i];
        }
    }
    return NULL;
}

#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
static void get_observation_paths_for_composite(anj_t *anj) {
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    const _anj_observe_observation_t *observation = ctx->processing_observation;
    const _anj_observe_observation_t *iterator = observation;
    size_t written = 0;
    do {
        if (_anj_dm_observe_is_any_resource_readable(anj, &iterator->path)
                != ANJ_COAP_CODE_NOT_FOUND) {
            ctx->uri_paths[written++] = &iterator->path;
        }
        iterator = iterator->prev;
    } while (iterator != ctx->processing_observation);
    ctx->uri_count = written;
}
#    endif // ANJ_WITH_OBSERVE_COMPOSITE

void _anj_observe_refresh_timestamp(_anj_observe_ctx_t *ctx,
                                    anj_time_monotonic_t timestamp,
                                    bool confirmable) {
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    const _anj_observe_observation_t *first_observation =
            ctx->processing_observation;
    do {
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
        ctx->processing_observation->last_notify_timestamp = timestamp;
        if (confirmable) {
            ctx->processing_observation->next_conf_notify_timestamp =
                    anj_time_monotonic_add(
                            timestamp,
                            anj_time_duration_new(1, ANJ_TIME_UNIT_DAY));
        }
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
        if (ctx->processing_observation->prev) {
            ctx->processing_observation = ctx->processing_observation->prev;
        }
    } while ((ctx->processing_observation != first_observation));
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
}

void _anj_observe_write_anj_res_to_observe_val(
        _anj_observation_res_val_t *observe_val,
        const anj_res_value_t *res_value,
        const anj_data_type_t *type) {
    switch (*type) {
    case ANJ_DATA_TYPE_INT:
        observe_val->int_value = res_value->int_value;
        break;
    case ANJ_DATA_TYPE_UINT:
        observe_val->uint_value = res_value->uint_value;
        break;
    case ANJ_DATA_TYPE_DOUBLE:
        observe_val->double_value = res_value->double_value;
        break;
    case ANJ_DATA_TYPE_BOOL:
        observe_val->bool_value = res_value->bool_value;
        break;
    default:
        ANJ_UNREACHABLE("incorrect data type");
    }
}

/* If st/gt/lt or edge are present but observation targets multi-instance
 * resource, then they are removed from the effective_attr and are not
 * taken into account when sending notifications.
 */
int _anj_observe_check_if_value_condition_attributes_should_be_disabled(
        anj_t *anj, _anj_observe_observation_t *observation) {
    _anj_attr_notification_t *attr = &observation->effective_attr;
    anj_res_value_t res_value;
    anj_data_type_t res_type;
    bool res_multi = false;
    int res;

    if (_anj_observe_attribute_has_value_change_condition(attr)
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
            /* Check composite observations attribute only if the path exists in
               the data model.
               Verification of whether an observation is a composite observation
               is based on checking the content format. Standard observation
               operation can't have Content Format option, so it is always set
               to _ANJ_COAP_FORMAT_NOT_DEFINED.
               Typically, the `prev` pointer is used to check if the observation
               is composite, but this function can be called when this pointer
               hasn't been set yet. */
            && (observation->content_format_opt == _ANJ_COAP_FORMAT_NOT_DEFINED
                || _anj_dm_observe_is_any_resource_readable(anj,
                                                            &observation->path)
                           != ANJ_COAP_CODE_NOT_FOUND)
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    ) {
        assert(anj_uri_path_has(&observation->path, ANJ_ID_RID));
        if (!(res = _anj_dm_observe_read_resource(anj, &res_value, &res_type,
                                                  &res_multi,
                                                  &observation->path))) {
            if (res_multi && anj_uri_path_is(&observation->path, ANJ_ID_RID)) {
                observe_log(L_WARNING,
                            "Attributes not supported for multi-instance "
                            "resources");
                attr->has_step = false;
                attr->has_greater_than = false;
                attr->has_less_than = false;
#    ifdef ANJ_WITH_LWM2M12
                attr->has_edge = false;
#    endif // ANJ_WITH_LWM2M12
            } else {
                _anj_observe_write_anj_res_to_observe_val(
                        &observation->last_sent_value, &res_value, &res_type);
            }
        } else {
            observe_log(L_WARNING, "Can't read resource");
            return res;
        }
    }
    return 0;
}

void _anj_observe_verify_effective_attributes(
        _anj_observe_observation_t *observation) {
    _anj_attr_notification_t *attr = &observation->effective_attr;
    if (_anj_observe_verify_attributes(attr, &observation->path, false)) {
        observe_log(L_WARNING, "Effective attributes are invalid, observation "
                               "will not be active");
        observation->observe_active = false;
    } else {
        observation->observe_active = true;
    }
}

/* If the observation has attributes assigned to it, we use them, otherwise we
 * check if there are no attributes assigned at the record level
 * (WRITE-ATTRIBUTES operation), and fill in the missing attributes with those
 * from the levels above (object->instance->resource->resource instance).
 */
static int calculate_effective_attr_set_init_values(
        anj_t *anj, _anj_observe_observation_t *observation, uint16_t ssid) {
    _anj_attr_notification_t *attr = &observation->effective_attr;
    int res;

    memset(&observation->effective_attr, 0,
           sizeof(observation->effective_attr));

#    ifdef ANJ_WITH_LWM2M12
    if (!_anj_observe_is_empty_attr(&observation->observation_attr)) {
        observe_log(L_DEBUG, "Observation has assigned attributes. Ignoring "
                             "write attr");
        *attr = observation->observation_attr;
    } else
#    endif // ANJ_WITH_LWM2M12
    {
        anj_uri_path_t path = observation->path;
        // start inhering from the most top level (root is forbidden)

        for (path.uri_len = 1; path.uri_len <= observation->path.uri_len;
             path.uri_len++) {
            _anj_observe_attr_storage_t *data_model_attr =
                    _anj_observe_get_attr_from_path(&anj->observe_ctx, &path,
                                                    ssid);
            if (data_model_attr) {
                _anj_observe_update_attr(attr, &data_model_attr->attr);
            }
        }
    }

    if ((res = _anj_observe_check_if_value_condition_attributes_should_be_disabled(
                 anj, observation))) {
        return res;
    }

    _anj_observe_verify_effective_attributes(observation);

    return 0;
}

void _anj_observe_remove_observation(_anj_observe_ctx_t *ctx) {
    _anj_observe_observation_t *base_observation = ctx->processing_observation;
    assert(base_observation);

    base_observation->ssid = 0;
    base_observation->notification_to_send = false;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    if (base_observation->prev) {
        _anj_observe_observation_t *prev_observation = base_observation->prev;
        while (prev_observation != base_observation) {
            assert(prev_observation);
            prev_observation->ssid = 0;
            prev_observation->notification_to_send = false;
            prev_observation = prev_observation->prev;
        }
    }
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    observe_log(L_INFO, "Observation removed");
}

void _anj_observe_set_uri_paths_and_format(anj_t *anj) {
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    if (ctx->processing_observation->prev) {
        // only existing paths are taken into account
        get_observation_paths_for_composite(anj);

        if (ctx->processing_observation->accept_opt
                == _ANJ_COAP_FORMAT_NOT_DEFINED) {
            ctx->format = ctx->processing_observation->content_format_opt;
        } else {
            ctx->format = ctx->processing_observation->accept_opt;
        }
    } else
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    {
        ctx->format = _ANJ_COAP_FORMAT_NOT_DEFINED;
        ctx->uri_count = 1;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
        ctx->uri_paths[0] = &ctx->processing_observation->path;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    }
}

/* Common check for both Observe and Observe-Composite operation */
static int observation_check_existence(_anj_observe_ctx_t *ctx,
                                       const _anj_coap_msg_t *request,
                                       uint16_t ssid) {
    _anj_observe_observation_t *observation =
            find_observation(ctx, ssid, ctx->token);
    if (observation) {
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
        bool composite = observation->prev;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
        observe_log(L_DEBUG, "Observation already exists");
        if ((!anj_uri_path_equal(&observation->path, &request->uri)
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
             && !composite
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
             )
#    ifdef ANJ_WITH_LWM2M12
                || !_anj_observe_compare_attr(&observation->observation_attr,
                                              &request->attr.notification_attr)
#    endif // ANJ_WITH_LWM2M12
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
                || ((observation->content_format_opt != request->content_format
                     || observation->accept_opt != request->accept)
                    && composite)
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
        ) {
            observe_log(L_ERROR, "Options different from the initial request");
            return ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
        }
        ctx->observation_exists = true;
        ctx->processing_observation = observation;
    }
    return 0;
}

static int add_observation(anj_t *anj,
                           const _anj_attr_notification_t *notification_attr,
                           const anj_uri_path_t *uri_path,
                           uint16_t content_format,
                           uint16_t accept_opt,
                           uint16_t ssid) {
    (void) content_format;
    (void) accept_opt;
    if (!anj_uri_path_has(uri_path, ANJ_ID_OID)) {
        return ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
    }
    int res;
    _anj_observe_observation_t *observation;

#    ifdef ANJ_WITH_LWM2M12
    if (!_anj_observe_is_empty_attr(notification_attr)) {
        res = _anj_observe_attributes_apply_condition(anj, uri_path,
                                                      notification_attr);
        if (res) {
            return res;
        }
    }
#    else  // ANJ_WITH_LWM2M12
    (void) notification_attr; // suppress unused parameter warning
#    endif // ANJ_WITH_LWM2M12
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    observation = find_spot_for_new_observation(ctx);
    if (!observation) {
        observe_log(L_ERROR, "No space for the new observation");
        return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    }
    observation->path = *uri_path;
    observation->ssid = ssid;
    observation->token = *ctx->token;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    observation->accept_opt = accept_opt;
    observation->content_format_opt = content_format;
    observation->prev = ctx->processing_observation;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    ctx->processing_observation = observation;
#    ifdef ANJ_WITH_LWM2M12
    observation->observation_attr = *notification_attr;
#    endif // ANJ_WITH_LWM2M12
    if ((res = calculate_effective_attr_set_init_values(
                 anj, ctx->processing_observation, ssid))) {
        return res;
    }
    return 0;
}

#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
static uint8_t add_composite_observation(void *arg_ptr,
                                         uint8_t *payload,
                                         size_t payload_len,
                                         bool last_block) {
    anj_t *anj = (anj_t *) arg_ptr;
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    int result = _ANJ_IO_EOF;
    int foo_obs_result = 0;

    assert(ctx->in_progress_type != MSG_TYPE_CANCEL_OBSERVE_RESPONSE);

    // when LwM2M server reinforce its interest in a resource by sending an
    // observation with an existing token we assume that the payload is the same
    if (!ctx->observation_exists) {
        if (_anj_io_in_ctx_feed_payload(&anj->anj_io.in_ctx, payload,
                                        payload_len, last_block)) {
            return ANJ_COAP_CODE_BAD_REQUEST;
        }

        anj_data_type_t type = ANJ_DATA_TYPE_ANY;
        const anj_res_value_t *value = NULL;

        const anj_uri_path_t *path;
        while (!foo_obs_result
               && !(result = _anj_io_in_ctx_get_entry(&anj->anj_io.in_ctx,
                                                      &type, &value, &path))) {
            type = ANJ_DATA_TYPE_ANY;
            foo_obs_result =
#        ifdef ANJ_WITH_LWM2M12
                    add_observation(anj, &ctx->notification_attr, path,
                                    ctx->format, ctx->accept, ctx->ssid);
#        else  // ANJ_WITH_LWM2M12
                    add_observation(anj, NULL, path, ctx->format, ctx->accept,
                                    ctx->ssid);
#        endif // ANJ_WITH_LWM2M12
            if (foo_obs_result) {
                foo_obs_result = map_err_to_coap_code(foo_obs_result);
            }
            if (ctx->first_observation == NULL) {
                ctx->first_observation = ctx->processing_observation;
            }
        }

        if (ctx->first_observation) {
            ctx->first_observation->prev = ctx->processing_observation;
        }
    } else {
        assert(ctx->processing_observation->prev);
        ctx->processing_observation = ctx->processing_observation->prev;
    }

    if ((result == _ANJ_IO_EOF && !foo_obs_result)
            || result == _ANJ_IO_WANT_NEXT_PAYLOAD) {
        if (last_block) {
            _anj_observe_refresh_timestamp(ctx, anj_time_monotonic_now(), true);
            _anj_observe_set_uri_paths_and_format(anj);
        }
        return 0;
    } else if (_ANJ_IO_ERR_FORMAT == result) {
        result = ANJ_COAP_CODE_BAD_REQUEST;
    } else if (result) {
        result = ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
    }

    return result ? (uint8_t) result : (uint8_t) foo_obs_result;
}
#    endif // ANJ_WITH_OBSERVE_COMPOSITE

void _anj_observe_init(anj_t *anj) {
    assert(anj);
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    memset(ctx, 0, sizeof(*ctx));
}

uint8_t _anj_observe_build_message(void *arg_ptr,
                                   uint8_t *buff,
                                   size_t buff_len,
                                   _anj_exchange_read_result_t *out_params) {
    anj_t *anj = (anj_t *) arg_ptr;
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    out_params->format = ctx->format;
    int result = 0;
    bool composite = false;
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    const anj_uri_path_t **uri_paths = ctx->uri_paths;

    composite = ctx->processing_observation->prev;
#    else  // ANJ_WITH_OBSERVE_COMPOSITE
    const anj_uri_path_t *uri_paths[1] = { &ctx->processing_observation->path };
#    endif // ANJ_WITH_OBSERVE_COMPOSITE

    result = _anj_dm_observe_build_msg(anj, uri_paths, ctx->uri_count,
                                       &ctx->already_processed, buff,
                                       &out_params->payload_len, buff_len,
                                       &out_params->format, composite);

    if (result) {
        return result != _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED
                       ? map_err_to_coap_code(result)
                       : _ANJ_EXCHANGE_BLOCK_TRANSFER_NEEDED;
    }
    return 0;
}

static void observe_exchange_completion(void *arg_ptr,
                                        const _anj_coap_msg_t *response,
                                        int result) {
    (void) response;
    anj_t *anj = (anj_t *) arg_ptr;
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    assert(ctx->in_progress_type == MSG_TYPE_OBSERVE_RESPONSE
           || ctx->in_progress_type == MSG_TYPE_CANCEL_OBSERVE_RESPONSE);
    ctx->already_processed = 0;
    _anj_dm_observe_finalize_operation(anj, result);
    if (result != _ANJ_EXCHANGE_RESULT_SUCCESS) {
        if (ctx->in_progress_type == MSG_TYPE_OBSERVE_RESPONSE) {
            observe_log(L_ERROR, "Failed to add observation: %d", result);
            if (ctx->processing_observation) {
                _anj_observe_remove_observation(ctx);
            }
        } else if (ctx->in_progress_type == MSG_TYPE_CANCEL_OBSERVE_RESPONSE) {
            observe_log(L_ERROR, "Failed to remove observation: %d", result);
        }
    } else {
        if (ctx->in_progress_type == MSG_TYPE_OBSERVE_RESPONSE) {
            observe_log(L_INFO, "Observation added");
        } else if (ctx->in_progress_type == MSG_TYPE_CANCEL_OBSERVE_RESPONSE) {
            _anj_observe_remove_observation(ctx);
        }
    }
}

static void refresh_after_write_attr(anj_t *anj,
                                     const anj_uri_path_t *path,
                                     uint16_t ssid) {
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        if (ctx->observations[i].ssid == ssid
                && !anj_uri_path_outside_base(&ctx->observations[i].path,
                                              path)) {
            calculate_effective_attr_set_init_values(anj, &ctx->observations[i],
                                                     ssid);
        }
    }
}

static bool composite_are_not_enabled(void) {
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    return false;
#    else  // ANJ_WITH_OBSERVE_COMPOSITE
    observe_log(L_ERROR, "Composite observation are not enabled");
    return true;
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
}

static bool wrong_format(uint16_t format) {
    if (format == _ANJ_COAP_FORMAT_SENML_ETCH_JSON
            || format == _ANJ_COAP_FORMAT_SENML_ETCH_CBOR
            || format == _ANJ_COAP_FORMAT_SENML_CBOR
            || format == _ANJ_COAP_FORMAT_SENML_JSON) {
        return false;
    }
    observe_log(L_ERROR, "Wrong payload format");
    return true;
}

static int
check_observe_attributes(const anj_uri_path_t *uri_path,
                         const _anj_attr_notification_t *notification_attr,
                         bool composite) {
#    ifdef ANJ_WITH_LWM2M12
    return _anj_observe_verify_attributes(notification_attr, uri_path,
                                          composite);
#    else  // ANJ_WITH_LWM2M12
    (void) uri_path; // suppress unused parameter warning
    (void) notification_attr;
    (void) composite;
    observe_log(L_ERROR, "Observe attributes are not supported");
    return ANJ_COAP_CODE_BAD_REQUEST;
#    endif // ANJ_WITH_LWM2M12
}

static int parse_request(anj_t *anj,
                         _anj_exchange_handlers_t *out_handlers,
                         const _anj_coap_msg_t *request,
                         uint8_t *response_code,
                         uint16_t ssid) {
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    observe_log(L_TRACE, "Request received, ssid: %" PRIu16, ssid);
    int result = ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
    bool composite = false;
    uint8_t positive_ret_val = ANJ_COAP_CODE_CONTENT;
    memset(out_handlers, 0, sizeof(*out_handlers));

    if (_anj_uri_path_to_security_or_oscore_obj(&request->uri)) {
        *response_code = ANJ_COAP_CODE_UNAUTHORIZED;
        return -1;
    }

    switch (request->operation) {
    case ANJ_OP_DM_WRITE_ATTR:
        positive_ret_val = ANJ_COAP_CODE_CHANGED;
        result = _anj_observe_write_attr_handle(anj, request, ssid);
        refresh_after_write_attr(anj, &request->uri, ssid);
        break;
    case ANJ_OP_INF_OBSERVE_COMP:
        if (composite_are_not_enabled()) {
            break;
        }
        composite = true;
        if (wrong_format(request->content_format)) {
            result = ANJ_COAP_CODE_BAD_REQUEST;
            break;
        }
        // fall through
    case ANJ_OP_INF_OBSERVE:
        *out_handlers = (_anj_exchange_handlers_t) {
            .read_payload = _anj_observe_build_message,
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
            .write_payload = add_composite_observation,
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
            .completion = observe_exchange_completion,
            .arg = anj
        };
        ctx->in_progress_type = MSG_TYPE_OBSERVE_RESPONSE;
        ctx->token = &request->token;
        result = observation_check_existence(ctx, request, ssid);
        if (result) {
            break;
        }
        if (!ctx->observation_exists
                && !_anj_observe_is_empty_attr(
                           &request->attr.notification_attr)) {
            result = check_observe_attributes(
                    &request->uri, &request->attr.notification_attr, composite);
            if (result) {
                break;
            }
        }
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
        if (composite) {
            if (!ctx->observation_exists) {
                result = _anj_io_in_ctx_init(&anj->anj_io.in_ctx,
                                             ANJ_OP_INF_OBSERVE_COMP,
                                             NULL,
                                             request->content_format);
                if (result == _ANJ_IO_ERR_UNSUPPORTED_FORMAT) {
                    result = ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT;
                    break;
                } else if (result) {
                    result = ANJ_COAP_CODE_BAD_REQUEST;
                    break;
                }
            }
            ctx->first_observation = NULL;
            ctx->format = request->content_format;
            ctx->accept = request->accept;
            ctx->ssid = ssid;
#        ifdef ANJ_WITH_LWM2M12
            ctx->notification_attr = request->attr.notification_attr;
#        endif // ANJ_WITH_LWM2M12
        } else {
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
            if (!ctx->observation_exists) {
                result = add_observation(anj, &request->attr.notification_attr,
                                         &request->uri,
                                         _ANJ_COAP_FORMAT_NOT_DEFINED,
                                         _ANJ_COAP_FORMAT_NOT_DEFINED, ssid);
                if (result) {
                    break;
                }
            }
            _anj_observe_refresh_timestamp(ctx, anj_time_monotonic_now(), true);
#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
        }
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
        if (result) {
            break;
        }
        if (!composite) {
            _anj_observe_set_uri_paths_and_format(anj);
        }
        break;
    case ANJ_OP_INF_CANCEL_OBSERVE_COMP:
        if (composite_are_not_enabled()) {
            break;
        }
        // fall through
    case ANJ_OP_INF_CANCEL_OBSERVE:
        *out_handlers = (_anj_exchange_handlers_t) {
            .read_payload = _anj_observe_build_message,
            .completion = observe_exchange_completion,
            .arg = anj
        };
        ctx->in_progress_type = MSG_TYPE_CANCEL_OBSERVE_RESPONSE;
        ctx->token = &request->token;
        ctx->processing_observation = find_observation(ctx, ssid, ctx->token);
        if (!ctx->processing_observation) {
            observe_log(L_WARNING, "Observation does not exist");
            result = ANJ_COAP_CODE_NOT_FOUND;
            break;
        }
        _anj_observe_set_uri_paths_and_format(anj);
        result = 0;
        break;
    default:
        ANJ_UNREACHABLE("incorrect operation type");
        break;
    }

#    ifdef ANJ_WITH_OBSERVE_COMPOSITE
    if (request->operation == ANJ_OP_INF_OBSERVE_COMP
            && result == ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT) {
        *response_code = ANJ_COAP_CODE_UNSUPPORTED_CONTENT_FORMAT;
    } else
#    endif // ANJ_WITH_OBSERVE_COMPOSITE
    {
        *response_code =
                result ? map_err_to_coap_code(result) : positive_ret_val;
    }
    return *response_code == positive_ret_val ? 0 : -1;
}

int _anj_observe_new_request(anj_t *anj,
                             _anj_exchange_handlers_t *out_handlers,
                             const _anj_observe_server_state_t *server_state,
                             const _anj_coap_msg_t *request,
                             uint8_t *out_response_code) {
    assert(anj && server_state && request && out_response_code);
    assert(server_state->ssid > 0 && server_state->ssid < UINT16_MAX);

    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    ctx->already_processed = 0;
    ctx->processing_observation = NULL;
    ctx->observation_exists = false;
    ctx->token = NULL;

    return parse_request(anj, out_handlers, request, out_response_code,
                         server_state->ssid);
}

void _anj_observe_remove_all_observations(anj_t *anj, uint16_t ssid) {
    assert(anj && ssid != 0);
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        if (ctx->observations[i].ssid == ssid
                || ssid == ANJ_OBSERVE_ANY_SERVER) {
            ctx->observations[i].ssid = 0;
        }
    }
}

#    ifdef ANJ_WITH_RST_AS_CANCEL_OBSERVE
void _anj_observe_update_last_mid(anj_t *anj, uint16_t mid) {
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    _anj_observe_observation_t *observation = ctx->processing_observation;
    if (!observation) {
        return;
    }
    observation->last_sent_mid = mid;
}

void _anj_observe_cancel_observation_by_mid(anj_t *anj, uint16_t mid) {
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER; i++) {
        if (ctx->observations[i].last_sent_mid == mid
                && ctx->observations[i].ssid != 0) {
            ctx->processing_observation = &ctx->observations[i];
            _anj_observe_remove_observation(ctx);
        }
    }
}
#    endif // ANJ_WITH_RST_AS_CANCEL_OBSERVE

#endif // ANJ_WITH_OBSERVE
