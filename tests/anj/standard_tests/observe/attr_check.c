/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stddef.h>

#define ANJ_UNIT_ENABLE_SHORT_ASSERTS
#include <anj/anj_config.h>
#include <anj/core.h>
#include <anj/defs.h>
#include <anj/utils.h>

#include "../../../../src/anj/observe/observe.h"
#include "../../../../src/anj/observe/observe_internal.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_OBSERVE

ANJ_UNIT_TEST(attr_check, attr_pmin_pmax) {
    _anj_attr_notification_t attr = {
        .has_min_period = true,
        .min_period = 5,
        .has_max_period = true,
        .max_period = 10
    };
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false));
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true));
}

ANJ_UNIT_TEST(attr_check, attr_pmin_pmax_equal) {
    _anj_attr_notification_t attr = {
        .has_min_period = true,
        .min_period = 5,
        .has_max_period = true,
        .max_period = 5
    };
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false));
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true));
}

ANJ_UNIT_TEST(attr_check, attr_pmin_pmax_ok) {
    _anj_attr_notification_t attr = {
        .has_min_period = true,
        .min_period = 10,
        .has_max_period = true,
        .max_period = 5
    };
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false));
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true));
}

ANJ_UNIT_TEST(attr_check, attr_epmin_epmax) {
    _anj_attr_notification_t attr = {
        .has_min_eval_period = true,
        .min_eval_period = 5,
        .has_max_eval_period = true,
        .max_eval_period = 10
    };
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false));
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true));
}

ANJ_UNIT_TEST(attr_check, attr_epmin_epmax_fail1) {
    _anj_attr_notification_t attr = {
        .has_min_eval_period = true,
        .min_eval_period = 10,
        .has_max_eval_period = true,
        .max_eval_period = 5
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_epmin_epmax_fail2) {
    _anj_attr_notification_t attr = {
        .has_min_eval_period = true,
        .min_eval_period = 5,
        .has_max_eval_period = true,
        .max_eval_period = 5
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_lt_gt) {
    _anj_attr_notification_t attr = {
        .has_less_than = true,
        .less_than = 10,
        .has_greater_than = true,
        .greater_than = 11
    };
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false));
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_lt_gt_fail) {
    _anj_attr_notification_t attr = {
        .has_less_than = true,
        .less_than = 10,
        .has_greater_than = true,
        .greater_than = 10
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_lt_st_gt_fail) {
    _anj_attr_notification_t attr = {
        .has_less_than = true,
        .less_than = 5,
        .has_step = true,
        .step = 5,
        .has_greater_than = true,
        .greater_than = 15
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

#    ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(attr_check, attr_edge_ok) {
    _anj_attr_notification_t attr = {
        .has_edge = true,
        .edge = 1
    };
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false));
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_edge_fail1) {
    _anj_attr_notification_t attr = {
        .has_edge = true,
        .edge = -1
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_edge_fail2) {
    _anj_attr_notification_t attr = {
        .has_edge = true,
        .edge = 2
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_con_ok) {
    _anj_attr_notification_t attr = {
        .has_con = true,
        .con = 1
    };
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false));
    ASSERT_OK(_anj_observe_verify_attributes(
            &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true));
}

ANJ_UNIT_TEST(attr_check, attr_con_fail1) {
    _anj_attr_notification_t attr = {
        .has_con = true,
        .con = -1
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_con_fail2) {
    _anj_attr_notification_t attr = {
        .has_con = true,
        .con = 2
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}
#    endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(attr_check, attr_lt_path_iid) {
    _anj_attr_notification_t attr = {
        .has_less_than = true,
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_INSTANCE_PATH(3, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_INSTANCE_PATH(3, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_lt_path_oid) {
    _anj_attr_notification_t attr = {
        .has_less_than = true,
    };
    ASSERT_EQ(_anj_observe_verify_attributes(&attr, &ANJ_MAKE_OBJECT_PATH(3),
                                             false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(&attr, &ANJ_MAKE_OBJECT_PATH(3),
                                             true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

#    ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(attr_check, attr_edge_path_iid) {
    _anj_attr_notification_t attr = {
        .has_edge = true,
    };
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_INSTANCE_PATH(3, 1), false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(
                      &attr, &ANJ_MAKE_INSTANCE_PATH(3, 1), true),
              ANJ_COAP_CODE_BAD_REQUEST);
}

ANJ_UNIT_TEST(attr_check, attr_edge_path_oid) {
    _anj_attr_notification_t attr = {
        .has_edge = true,
    };
    ASSERT_EQ(_anj_observe_verify_attributes(&attr, &ANJ_MAKE_OBJECT_PATH(3),
                                             false),
              ANJ_COAP_CODE_BAD_REQUEST);
    ASSERT_EQ(_anj_observe_verify_attributes(&attr, &ANJ_MAKE_OBJECT_PATH(3),
                                             true),
              ANJ_COAP_CODE_BAD_REQUEST);
}
#    endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(attr_check, targets_root) {
    anj_t anj = { 0 };
    _anj_observe_init(&anj);
    _anj_coap_msg_t request = {
        .uri = ANJ_MAKE_ROOT_PATH()
    };
    _anj_observe_server_state_t state = {
        .ssid = 1
    };
    ASSERT_EQ(_anj_observe_write_attr_handle(&anj, &request, state.ssid),
              ANJ_COAP_CODE_METHOD_NOT_ALLOWED);
}

ANJ_UNIT_TEST(attr_check, find_record_no_such_record) {
    _anj_observe_ctx_t ctx = {
        .attributes_storage =
                {
                    {
                        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                        .ssid = 1
                    }
                }
    };
    ASSERT_NULL(_anj_observe_get_attr_from_path(
            &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 0), 1));
}

ANJ_UNIT_TEST(attr_check, find_records) {
    _anj_observe_ctx_t ctx = {
        .attributes_storage =
                {
                    {
                        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                        .ssid = 1
                    },
                    {
                        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                        .ssid = 2
                    },
                    {
                        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 3),
                        .ssid = 1
                    }
                }
    };
    ASSERT_EQ((ptrdiff_t) _anj_observe_get_attr_from_path(
                      &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 1),
              (ptrdiff_t) &ctx.attributes_storage[0]);
    ASSERT_EQ((ptrdiff_t) _anj_observe_get_attr_from_path(
                      &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 2),
              (ptrdiff_t) &ctx.attributes_storage[1]);
    ASSERT_EQ((ptrdiff_t) _anj_observe_get_attr_from_path(
                      &ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 3), 1),
              (ptrdiff_t) &ctx.attributes_storage[2]);
}

ANJ_UNIT_TEST(attr_check, clean_attr_storage_for_id) {
    anj_t anj = { 0 };
    anj.observe_ctx = (_anj_observe_ctx_t) {
        .attributes_storage =
                {
                    {
                        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                        .ssid = 1
                    },
                    {
                        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 1),
                        .ssid = 2
                    },
                    {
                        .path = ANJ_MAKE_RESOURCE_PATH(3, 1, 3),
                        .ssid = 1
                    }
                }
    };
    _anj_observe_ctx_t *ctx = &anj.observe_ctx;
    ASSERT_EQ((ptrdiff_t) _anj_observe_get_attr_from_path(
                      ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 1),
              (ptrdiff_t) &ctx->attributes_storage[0]);
    ASSERT_EQ((ptrdiff_t) _anj_observe_get_attr_from_path(
                      ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 2),
              (ptrdiff_t) &ctx->attributes_storage[1]);
    ASSERT_EQ((ptrdiff_t) _anj_observe_get_attr_from_path(
                      ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 3), 1),
              (ptrdiff_t) &ctx->attributes_storage[2]);
    _anj_observe_remove_all_attr_storage(&anj, 1);
    ASSERT_NULL(_anj_observe_get_attr_from_path(
            ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 1));
    ASSERT_EQ((ptrdiff_t) _anj_observe_get_attr_from_path(
                      ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 1), 2),
              (ptrdiff_t) &ctx->attributes_storage[1]);
    ASSERT_NULL(_anj_observe_get_attr_from_path(
            ctx, &ANJ_MAKE_RESOURCE_PATH(3, 1, 3), 1));
}

#endif // ANJ_WITH_OBSERVE
