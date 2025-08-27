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
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anj/core.h>
#include <anj/defs.h>
#include <anj/log/log.h>
#include <anj/utils.h>

#include "../dm/dm_integration.h"
#include "observe.h"
#include "observe_internal.h"

#ifdef ANJ_WITH_OBSERVE

#    define compare_attr_val(HasVal1, Val1, Val2) \
        if (HasVal1) {                            \
            if (Val1 != Val2) {                   \
                return false;                     \
            }                                     \
        }

bool _anj_observe_compare_attr(const _anj_attr_notification_t *attr1,
                               const _anj_attr_notification_t *attr2) {
    bool result = attr1->has_min_period == attr2->has_min_period
                  && attr1->has_max_period == attr2->has_max_period
                  && attr1->has_greater_than == attr2->has_greater_than
                  && attr1->has_less_than == attr2->has_less_than
                  && attr1->has_step == attr2->has_step
                  && attr1->has_min_eval_period == attr2->has_min_eval_period
                  && attr1->has_max_eval_period == attr2->has_max_eval_period;
    if (!result) {
        return false;
    }
    compare_attr_val(attr1->has_min_period, attr1->min_period,
                     attr2->min_period);
    compare_attr_val(attr1->has_max_period, attr1->max_period,
                     attr2->max_period);
    compare_attr_val(attr1->has_greater_than, attr1->greater_than,
                     attr2->greater_than);
    compare_attr_val(attr1->has_less_than, attr1->less_than, attr2->less_than);
    compare_attr_val(attr1->has_step, attr1->step, attr2->step);
    compare_attr_val(attr1->has_min_eval_period, attr1->min_eval_period,
                     attr2->min_eval_period);
    compare_attr_val(attr1->has_max_eval_period, attr1->max_eval_period,
                     attr2->max_eval_period);
#    ifdef ANJ_WITH_LWM2M12
    result = attr1->has_edge == attr2->has_edge
             && attr1->has_con == attr2->has_con
             && attr1->has_hqmax == attr2->has_hqmax;
    if (!result) {
        return false;
    }
    compare_attr_val(attr1->has_edge, attr1->edge, attr2->edge);
    compare_attr_val(attr1->has_con, attr1->con, attr2->con);
    compare_attr_val(attr1->has_hqmax, attr1->hqmax, attr2->hqmax);
#    endif // ANJ_WITH_LWM2M12
    return true;
}

bool _anj_observe_attribute_has_value_change_condition(
        const _anj_attr_notification_t *attr) {
    return (attr->has_less_than || attr->has_step
#    ifdef ANJ_WITH_LWM2M12
            || attr->has_edge
#    endif // ANJ_WITH_LWM2M12
            || attr->has_greater_than);
}

int _anj_observe_verify_attributes(const _anj_attr_notification_t *attr,
                                   const anj_uri_path_t *path,
                                   bool composite) {
    if (attr->has_min_eval_period && attr->has_max_eval_period
            && attr->min_eval_period >= attr->max_eval_period) {
        return ANJ_COAP_CODE_BAD_REQUEST;
    }
    if (attr->has_less_than && attr->has_greater_than
            && attr->less_than >= attr->greater_than) {
        return ANJ_COAP_CODE_BAD_REQUEST;
    }
    if (attr->has_less_than && attr->has_step && attr->has_greater_than
            && attr->less_than + 2 * attr->step >= attr->greater_than) {
        return ANJ_COAP_CODE_BAD_REQUEST;
    }
    if (_anj_observe_attribute_has_value_change_condition(attr)
            && (composite || !anj_uri_path_has(path, ANJ_ID_RID))) {
        return ANJ_COAP_CODE_BAD_REQUEST;
    }
#    ifdef ANJ_WITH_LWM2M12
    if (attr->has_edge && attr->edge != 0 && attr->edge != 1) {
        return ANJ_COAP_CODE_BAD_REQUEST;
    }
    if (attr->has_con && attr->con != 0 && attr->con != 1) {
        return ANJ_COAP_CODE_BAD_REQUEST;
    }
#    endif // ANJ_WITH_LWM2M12
    return 0;
}

int _anj_observe_attributes_apply_condition(
        anj_t *anj,
        const anj_uri_path_t *path,
        const _anj_attr_notification_t *attr) {
    if (!anj_uri_path_has(path, ANJ_ID_OID)) {
        return ANJ_COAP_CODE_METHOD_NOT_ALLOWED;
    }

    anj_data_type_t resource_type;
    int res = _anj_dm_observe_is_any_resource_readable(anj, path);
    if (res) {
        return res;
    }
    if (attr->has_greater_than || attr->has_less_than || attr->has_step) {
        if ((res = _anj_dm_observe_read_resource(anj, NULL, &resource_type,
                                                 NULL, path))) {
            return res;
        }
        if (!(resource_type
              & (ANJ_DATA_TYPE_DOUBLE | ANJ_DATA_TYPE_INT
                 | ANJ_DATA_TYPE_UINT))) {
            return ANJ_COAP_CODE_BAD_REQUEST;
        }
    }
#    ifdef ANJ_WITH_LWM2M12
    if (attr->has_edge) {
        if ((res = _anj_dm_observe_read_resource(anj, NULL, &resource_type,
                                                 NULL, path))) {
            return res;
        }
        if (resource_type != ANJ_DATA_TYPE_BOOL) {
            return ANJ_COAP_CODE_BAD_REQUEST;
        }
    }
#    endif // ANJ_WITH_LWM2M12
    return 0;
}

static _anj_observe_attr_storage_t *
find_spot_for_new_attr(_anj_observe_ctx_t *ctx) {
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER; ++i) {
        // ssid can be 0 only if the attr spot is not used
        if (ctx->attributes_storage[i].ssid == 0) {
            memset(&ctx->attributes_storage[i], 0,
                   sizeof(ctx->attributes_storage[i]));
            return &ctx->attributes_storage[i];
        }
    }
    return NULL;
}

static void remove_attr(_anj_observe_attr_storage_t *attr_rec) {
    attr_rec->ssid = 0;
}

static void update_uint_attr(bool *is_active,
                             bool new_is_active,
                             uint32_t *value,
                             uint32_t new_value) {
    if (new_is_active) {
        if (new_value == ANJ_ATTR_UINT_NONE) {
            *is_active = false;
        } else {
            *is_active = true;
            *value = new_value;
        }
    }
}

static void update_double_attr(bool *is_active,
                               bool new_is_active,
                               double *value,
                               double new_value) {
    if (new_is_active) {
        if (isnan(new_value)) {
            *is_active = false;
        } else {
            *is_active = true;
            *value = new_value;
        }
    }
}

void _anj_observe_update_attr(_anj_attr_notification_t *attr,
                              const _anj_attr_notification_t *new_attr) {
    update_uint_attr(&attr->has_min_period, new_attr->has_min_period,
                     &attr->min_period, new_attr->min_period);
    update_uint_attr(&attr->has_max_period, new_attr->has_max_period,
                     &attr->max_period, new_attr->max_period);
    update_uint_attr(&attr->has_min_eval_period, new_attr->has_min_eval_period,
                     &attr->min_eval_period, new_attr->min_eval_period);
    update_uint_attr(&attr->has_max_eval_period, new_attr->has_max_eval_period,
                     &attr->max_eval_period, new_attr->max_eval_period);
    update_double_attr(&attr->has_greater_than, new_attr->has_greater_than,
                       &attr->greater_than, new_attr->greater_than);
    update_double_attr(&attr->has_less_than, new_attr->has_less_than,
                       &attr->less_than, new_attr->less_than);
    update_double_attr(&attr->has_step, new_attr->has_step, &attr->step,
                       new_attr->step);
#    ifdef ANJ_WITH_LWM2M12
    update_uint_attr(&attr->has_edge, new_attr->has_edge, &attr->edge,
                     new_attr->edge);
    update_uint_attr(&attr->has_con, new_attr->has_con, &attr->con,
                     new_attr->con);
    update_uint_attr(&attr->has_hqmax, new_attr->has_hqmax, &attr->hqmax,
                     new_attr->hqmax);
#    endif // ANJ_WITH_LWM2M12
}

static int add_attr(_anj_observe_ctx_t *ctx,
                    const _anj_coap_msg_t *request,
                    _anj_observe_attr_storage_t **out_record,
                    uint16_t ssid) {
    _anj_observe_attr_storage_t *attr_rec =
            _anj_observe_get_attr_from_path(ctx, &request->uri, ssid);
    if (attr_rec) {
        observe_log(L_DEBUG,
                    "Path has attributes attached. Going to update them");
        _anj_observe_update_attr(&attr_rec->attr,
                                 &request->attr.notification_attr);
    } else {
        observe_log(L_DEBUG,
                    "Path has no attributes attached. Going to add them");
        attr_rec = find_spot_for_new_attr(ctx);
        if (!attr_rec) {
            observe_log(L_ERROR, "No space for new attributes");
            return ANJ_COAP_CODE_INTERNAL_SERVER_ERROR;
        }
        attr_rec->ssid = ssid;
        attr_rec->path = request->uri;
        attr_rec->attr = request->attr.notification_attr;
    }
    *out_record = attr_rec;
    return 0;
}

_anj_observe_attr_storage_t *_anj_observe_get_attr_from_path(
        _anj_observe_ctx_t *ctx, const anj_uri_path_t *path, uint16_t ssid) {
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER; ++i) {
        if (anj_uri_path_equal(path, &ctx->attributes_storage[i].path)
                && ssid == ctx->attributes_storage[i].ssid) {
            return &ctx->attributes_storage[i];
        }
    }
    return NULL;
}

bool _anj_observe_is_empty_attr(const _anj_attr_notification_t *attr) {
    return !attr->has_min_period && !attr->has_max_period
           && !attr->has_greater_than && !attr->has_less_than && !attr->has_step
#    ifdef ANJ_WITH_LWM2M12
           && !attr->has_edge && !attr->has_con && !attr->has_hqmax
#    endif // ANJ_WITH_LWM2M12
           && !attr->has_min_eval_period && !attr->has_max_eval_period;
}

int _anj_observe_write_attr_handle(anj_t *anj,
                                   const _anj_coap_msg_t *request,
                                   uint16_t ssid) {
    _anj_observe_attr_storage_t *record;
    int res = add_attr(&anj->observe_ctx, request, &record, ssid);
    if (res) {
        return res;
    }
    res = _anj_observe_verify_attributes(&record->attr, &request->uri, false);
    if (!res) {
        res = _anj_observe_attributes_apply_condition(anj, &request->uri,
                                                      &record->attr);
    }
    // in case of error or empty attributes, remove the record
    if (res || _anj_observe_is_empty_attr(&record->attr)) {
        observe_log(L_WARNING, "Attributes verification failed");
        remove_attr(record);
    }
    observe_log(L_DEBUG, "New attributes successfully added");
    return res;
}

void _anj_observe_remove_all_attr_storage(anj_t *anj, uint16_t ssid) {
    assert(anj && ssid > 0 && ssid < ANJ_OBSERVE_ANY_SERVER);
    _anj_observe_ctx_t *ctx = &anj->observe_ctx;
    for (size_t i = 0; i < ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER; ++i) {
        if (ctx->attributes_storage[i].ssid == ssid
                || ssid == ANJ_OBSERVE_ANY_SERVER) {
            ctx->attributes_storage[i].ssid = 0;
        }
    }
}

#    ifdef ANJ_WITH_DISCOVER_ATTR
int _anj_observe_get_attr_storage(anj_t *anj,
                                  uint16_t ssid,
                                  bool with_parents_attr,
                                  const anj_uri_path_t *path,
                                  _anj_attr_notification_t *out_attr) {
    assert(anj && path && out_attr && ssid > 0 && ssid < ANJ_OBSERVE_ANY_SERVER
           && anj_uri_path_length(path));

    memset(out_attr, 0, sizeof(*out_attr));
    bool found = false;
    anj_uri_path_t current_path = *path;
    if (with_parents_attr) {
        // start from object level to catch all related attribute
        current_path.uri_len = 1;
    }
    while (1) {
        _anj_observe_attr_storage_t *attr_storage =
                _anj_observe_get_attr_from_path(&anj->observe_ctx,
                                                &current_path, ssid);
        if (attr_storage) {
            found = true;
            _anj_observe_update_attr(out_attr, &attr_storage->attr);
        }
        if (anj_uri_path_length(&current_path) == anj_uri_path_length(path)) {
            return found ? 0 : ANJ_COAP_CODE_NOT_FOUND;
        }
        current_path.uri_len++;
    }
}
#    endif // ANJ_WITH_DISCOVER_ATTR

#endif // ANJ_WITH_OBSERVE
