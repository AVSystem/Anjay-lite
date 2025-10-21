/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <anj/compat/net/anj_net_api.h>
#include <anj/core.h>

#include "../mock/net_api_mock.h"

#include "../../../src/anj/core/server.h"

#include <anj_unit_test.h>

#define TEST_INIT()               \
    net_api_mock_t mock = { 0 };  \
    net_api_mock_ctx_init(&mock); \
    mock.inner_mtu_value = 500;   \
    _anj_server_connection_ctx_t ctx = { 0 };

ANJ_UNIT_TEST(server, instant_connect_disconnect) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                                "localhost", "9998"));
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "localhost");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "9998");
    ANJ_UNIT_ASSERT_EQUAL(ctx.mtu, 500);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_GET_INNER_MTU], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CREATE], 1);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, true));
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SHUTDOWN], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLOSE], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
}

ANJ_UNIT_TEST(server, connect_disconnect_with_net_inprogress) {
    TEST_INIT();

    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EINPROGRESS;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                              "localhost", "9998"),
                          ANJ_NET_EINPROGRESS);
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_EINPROGRESS;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                              "localhost", "9998"),
                          ANJ_NET_EINPROGRESS);
    mock.call_result[ANJ_NET_FUN_CONNECT] = ANJ_NET_OK;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                                "localhost", "9998"));
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.hostname, "localhost");
    ANJ_UNIT_ASSERT_EQUAL_STRING(mock.port, "9998");
    ANJ_UNIT_ASSERT_EQUAL(ctx.mtu, 500);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 3);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_GET_INNER_MTU], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CREATE], 1);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, true));
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SHUTDOWN], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLOSE], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
}

ANJ_UNIT_TEST(server, connect_errors) {
    TEST_INIT();

    mock.call_result[ANJ_NET_FUN_CREATE] = -22;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                              "localhost", "9998"),
                          -22);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CREATE], 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, true));
    mock.call_result[ANJ_NET_FUN_CREATE] = 0;

    mock.call_result[ANJ_NET_FUN_CONNECT] = -3;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                              "localhost", "9998"),
                          -3);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 0 + 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CREATE], 1 + 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_GET_INNER_MTU], 0);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, true));
    mock.call_result[ANJ_NET_FUN_CONNECT] = 0;

    mock.call_result[ANJ_NET_FUN_GET_INNER_MTU] = -4;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                              "localhost", "9998"),
                          -4);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 0 + 1 + 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CREATE], 1 + 1 + 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_GET_INNER_MTU], 0 + 1);
    // don't call cleanup
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, false));
    mock.call_result[ANJ_NET_FUN_GET_INNER_MTU] = 0;

    mock.inner_mtu_value = 0;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                              "localhost", "9998"),
                          -1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 0 + 1 + 1 + 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CREATE], 1 + 1 + 1 + 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_GET_INNER_MTU],
                          0 + 1 + 1);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, false));
    mock.inner_mtu_value = 500;
}

ANJ_UNIT_TEST(server, disconnect) {
    TEST_INIT();

    // ctx not exists
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, true));
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SHUTDOWN], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLOSE], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 0);

    mock.call_result[ANJ_NET_FUN_CONNECT] = -22;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                              "localhost", "9998"),
                          -22);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CONNECT], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_GET_INNER_MTU], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CREATE], 1);

    // there is no connection and cleanup - return imidiately
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, false));
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SHUTDOWN], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLOSE], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 0);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, true));
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SHUTDOWN], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLOSE], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
}

ANJ_UNIT_TEST(server, disconnect_with_shutdown_error) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                                "localhost", "9998"));

    // error in shutdown should not stop the process
    mock.call_result[ANJ_NET_FUN_SHUTDOWN] = -33;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_close(&ctx, true));
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SHUTDOWN], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLOSE], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
}

ANJ_UNIT_TEST(server, disconnect_with_close_error) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                                "localhost", "9998"));

    // error in shutdown should not stop the process
    mock.call_result[ANJ_NET_FUN_CLOSE] = -33;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_close(&ctx, false), -33);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SHUTDOWN], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLOSE], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 0);
}

ANJ_UNIT_TEST(server, disconnect_with_cleanup_error) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                                "localhost", "9998"));

    // error in shutdown should not stop the process
    mock.call_result[ANJ_NET_FUN_CLEANUP] = -11;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_close(&ctx, true), -11);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_SHUTDOWN], 1);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLOSE], 0);
    ANJ_UNIT_ASSERT_EQUAL(mock.call_count[ANJ_NET_FUN_CLEANUP], 1);
}

ANJ_UNIT_TEST(server, send) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                                "localhost", "9998"));

    uint8_t buffer[20] = "1234567890ABCDEFGHIJ";

    mock.bytes_to_send = 0;
    mock.call_result[ANJ_NET_FUN_SEND] = ANJ_NET_EINPROGRESS;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_send(&ctx, buffer, 20),
                          ANJ_NET_EINPROGRESS);
    ANJ_UNIT_ASSERT_EQUAL(ctx.bytes_sent, 0);
    mock.bytes_to_send = 10;
    mock.call_result[ANJ_NET_FUN_SEND] = ANJ_NET_OK;
    // first chunk is send, but not all data so _anj_server_send returns
    // EINPROGRESS
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_send(&ctx, buffer, 20),
                          ANJ_NET_EINPROGRESS);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, "1234567890", 10);
    ANJ_UNIT_ASSERT_EQUAL(ctx.bytes_sent, 10);
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_send(&ctx, buffer, 20));
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(mock.send_data_buffer, "ABCDEFGHIJ", 10);
    ANJ_UNIT_ASSERT_EQUAL(ctx.bytes_sent, 0);
}

ANJ_UNIT_TEST(server, recv) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                                "localhost", "9998"));

    uint8_t buffer[20] = { 0 };
    size_t out_length;

    mock.bytes_to_recv = 10;
    mock.data_to_recv = (uint8_t *) "1234567890";

    mock.call_result[ANJ_NET_FUN_RECV] = ANJ_NET_EAGAIN;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_receive(&ctx, buffer, &out_length, 20),
                          ANJ_NET_EAGAIN);
    ANJ_UNIT_ASSERT_EQUAL(out_length, 0);

    mock.call_result[ANJ_NET_FUN_RECV] = ANJ_NET_EMSGSIZE;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_receive(&ctx, buffer, &out_length, 20),
                          ANJ_NET_EAGAIN);
    ANJ_UNIT_ASSERT_EQUAL(out_length, 0);

    mock.call_result[ANJ_NET_FUN_RECV] = -88;
    ANJ_UNIT_ASSERT_EQUAL(_anj_server_receive(&ctx, buffer, &out_length, 20),
                          -88);
    ANJ_UNIT_ASSERT_EQUAL(out_length, 0);

    mock.call_result[ANJ_NET_FUN_RECV] = ANJ_NET_OK;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_receive(&ctx, buffer, &out_length, 20));
    ANJ_UNIT_ASSERT_EQUAL(out_length, 10);
    ANJ_UNIT_ASSERT_EQUAL_BYTES_SIZED(buffer, "1234567890", 10);
}

ANJ_UNIT_TEST(server, payload_size) {
    TEST_INIT();

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_connect(&ctx, ANJ_NET_BINDING_UDP, NULL,
                                                "localhost", "9998"));

    // tests only with server_request = true,
    // _anj_coap_calculate_msg_header_max_size is tested in another place
    _anj_coap_msg_t msg;
    size_t out_payload_size;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_calculate_max_payload_size(
            &ctx, &msg, 50, 200, true, &out_payload_size));
    // payload_buff_size is result
    ANJ_UNIT_ASSERT_EQUAL(out_payload_size, 50);

    ctx.mtu = 100;
    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_calculate_max_payload_size(
            &ctx, &msg, 200, 200, true, &out_payload_size));
    // inner_mtu_value - _ANJ_COAP_UDP_RESPONSE_MSG_HEADER_MAX_SIZE is result
    ANJ_UNIT_ASSERT_EQUAL(out_payload_size, 75);

    ANJ_UNIT_ASSERT_SUCCESS(_anj_server_calculate_max_payload_size(
            &ctx, &msg, 200, 50, true, &out_payload_size));
    // out_msg_buffer_size - _ANJ_COAP_UDP_RESPONSE_MSG_HEADER_MAX_SIZE is
    // result
    ANJ_UNIT_ASSERT_EQUAL(out_payload_size, 25);

    // out_msg_buffer_size is too small
    ANJ_UNIT_ASSERT_FAILED(_anj_server_calculate_max_payload_size(
            &ctx, &msg, 200, 20, true, &out_payload_size));

    // payload_buff_size is < 16 -> minimal block size
    ANJ_UNIT_ASSERT_FAILED(_anj_server_calculate_max_payload_size(
            &ctx, &msg, 200, 40, true, &out_payload_size));
}
