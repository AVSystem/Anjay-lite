/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>

#include <anj/defs.h>

#include "../../src/anj/coap/coap.h"

#include <anj_unit_test.h>

#ifdef ANJ_WITH_LWM2M12
ANJ_UNIT_TEST(attr_decode_udp, decode_write_attributes) {
    uint8_t MSG[] =
            "\x41"         // header v 0x01, Confirmable, tkl 1
            "\x03\x37\x21" // PUT code 0.3
            "\x12"         // token
            "\xB1\x31"     // uri-path_1 URI_PATH 11 /1
            "\x47\x70\x6d\x69\x6e\x3d\x32\x30"         // URI_QUERY 15 pmin=20
            "\x07\x65\x70\x6d\x69\x6e\x3d\x31"         // URI_QUERY 15 epmin=1
            "\x07\x65\x70\x6d\x61\x78\x3d\x32"         // URI_QUERY 15 epmax=2
            "\x05\x63\x6f\x6e\x3d\x31"                 // URI_QUERY 15 con=1
            "\x07\x67\x74\x3d\x32\x2e\x38\x35"         // URI_QUERY 15 gt=2.85
            "\x09\x6c\x74\x3d\x33\x33\x33\x33\x2e\x38" // URI_QUERY 15 lt=3333.8
            "\x07\x73\x74\x3d\x2D\x30\x2e\x38"         // URI_QUERY 15 st=-0.8
            "\x06\x65\x64\x67\x65\x3d\x30"             // URI_QUERY 15 edge=0
            "\x0A\x68\x71\x6d\x61\x78\x3d\x37\x37\x37\x37" // URI_QUERY 15
                                                           // hqmax=7777
            "\x09\x70\x6d\x61\x78\x3d\x31\x32\x30\x30" // URI_QUERY 15 pmax=1200
            ;

    _anj_coap_msg_t out_data;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_greater_than,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_less_than, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_step, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_eval_period,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_eval_period,
                          true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_edge, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_con, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_hqmax, true);

    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_period, 20);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_period, 1200);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_eval_period, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_eval_period, 2);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.edge, 0);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.con, 1);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.hqmax, 7777);

    ANJ_UNIT_ASSERT_EQUAL(
            (int) (100 * out_data.attr.notification_attr.greater_than), 285);
    ANJ_UNIT_ASSERT_EQUAL(
            (int) (100 * out_data.attr.notification_attr.less_than), 333380);
    ANJ_UNIT_ASSERT_EQUAL((int) (100 * out_data.attr.notification_attr.step),
                          -80);
}
#endif // ANJ_WITH_LWM2M12

ANJ_UNIT_TEST(attr_decode_udp, decode_write_attributes_empty_1) {
    uint8_t MSG[] =
            "\x41"         // header v 0x01, Confirmable, tkl 1
            "\x03\x37\x21" // PUT code 0.3
            "\x12"         // token
            "\xD7\x02\x70\x6d\x69\x6e\x3d\x32\x30" // URI_QUERY 15 pmin=20
            "\x05\x70\x6d\x61\x78\x3d"             // URI_QUERY 15 pmax=
            ;

    _anj_coap_msg_t out_data;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_greater_than,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_less_than, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_step, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_eval_period,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_eval_period,
                          false);
#ifdef ANJ_WITH_LWM2M12
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_edge, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_con, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_hqmax, false);
#endif // ANJ_WITH_LWM2M12
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.min_period, 20);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_period,
                          ANJ_ATTR_UINT_NONE);
}

ANJ_UNIT_TEST(attr_decode_udp, decode_write_attributes_empty_2) {
    uint8_t MSG[] = "\x41"         // header v 0x01, Confirmable, tkl 1
                    "\x03\x37\x21" // PUT code 0.3
                    "\x12"         // token
                    "\xD4\x02\x70\x6d\x61\x78" // URI_QUERY 15 pmax
            ;

    _anj_coap_msg_t out_data;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_period,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_greater_than,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_less_than, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_step, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_eval_period,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_eval_period,
                          false);
#ifdef ANJ_WITH_LWM2M12
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_edge, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_con, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_hqmax, false);
#endif // ANJ_WITH_LWM2M12
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_period,
                          ANJ_ATTR_UINT_NONE);
}

ANJ_UNIT_TEST(attr_decode_udp, decode_write_attributes_empty_3) {
    uint8_t MSG[] = "\x41"         // header v 0x01, Confirmable, tkl 1
                    "\x03\x37\x21" // PUT code 0.3
                    "\x12"         // token
                    "\xD4\x02\x70\x6d\x61\x78" // URI_QUERY 15 pmax
                    "\x03\x73\x74\x3d"         // URI_QUERY 15 st=
            ;

    _anj_coap_msg_t out_data;
    ANJ_UNIT_ASSERT_SUCCESS(
            _anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data));

    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_period,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_period, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_greater_than,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_less_than, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_step, true);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_min_eval_period,
                          false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_max_eval_period,
                          false);
#ifdef ANJ_WITH_LWM2M12
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_edge, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_con, false);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.has_hqmax, false);
#endif // ANJ_WITH_LWM2M12
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.max_period,
                          ANJ_ATTR_UINT_NONE);
    ANJ_UNIT_ASSERT_EQUAL(out_data.attr.notification_attr.step,
                          ANJ_ATTR_DOUBLE_NONE);
}

ANJ_UNIT_TEST(attr_decode_udp, decode_write_attributes_incorrect_attribute) {
    uint8_t MSG[] = "\x41"         // header v 0x01, Confirmable, tkl 1
                    "\x03\x37\x21" // PUT code 0.3
                    "\x12"         // token
                    "\xD5\x02\x70\x6d\x61\x78\x78" // URI_QUERY 15 pmaxx
            ;

    _anj_coap_msg_t out_data;
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data),
                          _ANJ_ERR_MALFORMED_MESSAGE);
}

ANJ_UNIT_TEST(attr_decode_udp, decode_write_attributes_incorrect_attribute_2) {
    uint8_t MSG[] =
            "\x41"         // header v 0x01, Confirmable, tkl 1
            "\x03\x37\x21" // PUT code 0.3
            "\x12"         // token
            "\xD7\x02\x70\x6d\x69\x6e\x3d\x6e\x30" // URI_QUERY 15 pmin=n0
            ;

    _anj_coap_msg_t out_data;
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data),
                          _ANJ_ERR_MALFORMED_MESSAGE);
}

// option buffer is set in CMakeLists.txt
// set(ANJ_COAP_ATTR_OPTION_MAX_SIZE 50)
ANJ_UNIT_TEST(attr_decode_udp, decode_write_attributes_size_error) {
    uint8_t MSG[] =
            "\x41"         // header v 0x01, Confirmable, tkl 1
            "\x03\x37\x21" // PUT code 0.3
            "\x12"         // token
            "\xDD\x02\x28\x73\x74\x3d\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31"
            "\x31\x31"
            "\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31"
            "\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31"
            "\x31\x31\x31\x31\x31\x31" // URI_QUERY 15 st=111..
            ;

    _anj_coap_msg_t out_data;
    ANJ_UNIT_ASSERT_EQUAL(_anj_coap_decode_udp(MSG, sizeof(MSG) - 1, &out_data),
                          _ANJ_ERR_ATTR_BUFF);
}
