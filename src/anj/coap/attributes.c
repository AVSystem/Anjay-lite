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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <anj/defs.h>
#include <anj/utils.h>

#include "attributes.h"
#include "coap.h"
#include "common.h"

static int get_attr(const anj_coap_options_t *opts,
                    const char *attr,
                    uint32_t *uint_value,
                    double *double_value,
                    bool *is_present) {
    assert(!!uint_value != !!double_value);

    size_t it = 0;
    uint8_t attr_buff[ANJ_COAP_MAX_ATTR_OPTION_SIZE];

    while (true) {
        size_t attr_option_size = 0;
        memset(attr_buff, 0, sizeof(attr_buff));
        int res = _anj_coap_options_get_data_iterate(
                opts, _ANJ_COAP_OPTION_URI_QUERY, &it, &attr_option_size,
                attr_buff, sizeof(attr_buff));

        if (res == _ANJ_COAP_OPTION_MISSING) {
            return 0;
        } else if (res) {
            return _ANJ_ERR_ATTR_BUFF;
        }

        if (!strncmp(attr, (const char *) attr_buff, strlen(attr))) {
            // format "pmin={value}"
            size_t value_offset = strlen(attr);
            if (value_offset > attr_option_size) {
                return _ANJ_ERR_MALFORMED_MESSAGE;
            }
            *is_present = true;
            size_t value_buff_len = attr_option_size - value_offset;
            if (value_buff_len != 0 && attr_buff[value_offset] != '=') {
                return _ANJ_ERR_MALFORMED_MESSAGE;
            }
            // empty record "pmin=" or "pmin"
            if (value_buff_len == 0 || value_buff_len == 1) {
                if (uint_value) {
                    *uint_value = ANJ_ATTR_UINT_NONE;
                } else if (double_value) {
                    *double_value = ANJ_ATTR_DOUBLE_NONE;
                }
                return 0;
            }
            // skip '='
            value_offset++;
            value_buff_len--;
            if (uint_value) {
                res = anj_string_to_uint32_value(
                        uint_value,
                        (const char *) (attr_buff + value_offset),
                        value_buff_len);
            } else if (double_value) {
                res = anj_string_to_double_value(
                        double_value,
                        (const char *) (attr_buff + value_offset),
                        value_buff_len);
            }
            return res ? _ANJ_ERR_MALFORMED_MESSAGE : 0;
        }
    }
}

static int add_str_attr(anj_coap_options_t *opts,
                        const char *attr_name,
                        const char *attr_value,
                        bool value_present) {
    if (!value_present) {
        return 0;
    }

    char atrr_buff[ANJ_COAP_MAX_ATTR_OPTION_SIZE] = { 0 };

    size_t name_len = strlen(attr_name);
    if (name_len >= ANJ_COAP_MAX_ATTR_OPTION_SIZE) {
        return _ANJ_ERR_ATTR_BUFF;
    }
    memcpy(atrr_buff, attr_name, name_len);

    // not empty string
    if (attr_value) {
        size_t value_len = strlen(attr_value);
        atrr_buff[name_len] = '=';

        if (name_len + value_len + 1 >= ANJ_COAP_MAX_ATTR_OPTION_SIZE) {
            return _ANJ_ERR_ATTR_BUFF;
        }
        memcpy(&atrr_buff[name_len + 1], attr_value, value_len);
    }

    return _anj_coap_options_add_string(opts, _ANJ_COAP_OPTION_URI_QUERY,
                                        atrr_buff);
}

int anj_attr_discover_decode(const anj_coap_options_t *opts,
                             _anj_attr_discover_t *attr) {
    memset(attr, 0, sizeof(_anj_attr_discover_t));

    return get_attr(opts, "depth", &attr->depth, NULL, &attr->has_depth);
}

int anj_attr_notification_attr_decode(const anj_coap_options_t *opts,
                                      _anj_attr_notification_t *attr) {
    memset(attr, 0, sizeof(_anj_attr_notification_t));

    int res = get_attr(opts, "pmin", &attr->min_period, NULL,
                       &attr->has_min_period);
    _RET_IF_ERROR(res);
    res = get_attr(opts, "pmax", &attr->max_period, NULL,
                   &attr->has_max_period);
    _RET_IF_ERROR(res);
    res = get_attr(opts, "gt", NULL, &attr->greater_than,
                   &attr->has_greater_than);
    _RET_IF_ERROR(res);
    res = get_attr(opts, "lt", NULL, &attr->less_than, &attr->has_less_than);
    _RET_IF_ERROR(res);
    res = get_attr(opts, "st", NULL, &attr->step, &attr->has_step);
    _RET_IF_ERROR(res);
    res = get_attr(opts, "epmin", &attr->min_eval_period, NULL,
                   &attr->has_min_eval_period);
    _RET_IF_ERROR(res);
    res = get_attr(opts, "epmax", &attr->max_eval_period, NULL,
                   &attr->has_max_eval_period);
#ifdef ANJ_WITH_LWM2M12
    _RET_IF_ERROR(res);
    res = get_attr(opts, "edge", &attr->edge, NULL, &attr->has_edge);
    _RET_IF_ERROR(res);
    res = get_attr(opts, "con", &attr->con, NULL, &attr->has_con);
    _RET_IF_ERROR(res);
    res = get_attr(opts, "hqmax", &attr->hqmax, NULL, &attr->has_hqmax);
#endif // ANJ_WITH_LWM2M12

    return res;
}

int anj_attr_register_prepare(anj_coap_options_t *opts,
                              const _anj_attr_register_t *attr) {
    int res = add_str_attr(opts, "ep", attr->endpoint, attr->has_endpoint);
    _RET_IF_ERROR(res);
    if (attr->has_lifetime) {
        char lifetime_buff[ANJ_U32_STR_MAX_LEN + 1] = { 0 };
        anj_uint32_to_string_value(lifetime_buff, attr->lifetime);
        res = add_str_attr(opts, "lt", lifetime_buff, attr->has_lifetime);
        _RET_IF_ERROR(res);
    }
    res = add_str_attr(opts, "lwm2m", attr->lwm2m_ver, attr->has_lwm2m_ver);
    _RET_IF_ERROR(res);
    res = add_str_attr(opts, "b", attr->binding, attr->has_binding);
    _RET_IF_ERROR(res);
    res = add_str_attr(opts, "sms", attr->sms_number, attr->has_sms_number);
    _RET_IF_ERROR(res);
    res = add_str_attr(opts, "Q", NULL, attr->has_Q);

    return res;
}

int anj_attr_bootstrap_prepare(anj_coap_options_t *opts,
                               const _anj_attr_bootstrap_t *attr,
                               bool bootstrap_pack) {
    int res = add_str_attr(opts, "ep", attr->endpoint, attr->has_endpoint);
    _RET_IF_ERROR(res);
    if (attr->has_preferred_content_format && !bootstrap_pack) {
        char pct_buff[ANJ_U32_STR_MAX_LEN + 1] = { 0 };
        anj_uint16_to_string_value(pct_buff, attr->preferred_content_format);
        res = add_str_attr(opts, "pct", pct_buff,
                           attr->has_preferred_content_format);
    }

    return res;
}
