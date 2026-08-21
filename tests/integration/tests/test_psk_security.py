# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import time

from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.transport import Transport
from framework_tools.lwm2m.coap.content_format import ContentFormat
from framework_tools.lwm2m.senml_cbor import CBOR, SenmlLabel
import framework_tools.lwm2m.messages as msgs

import utils

PSK_IDENTITY = "test-identity"
PSK_KEY = "test-key"
ENDPOINT = "test-endpoint"
LIFETIME = 100


def _make_psk_server(psk_identity=PSK_IDENTITY,
                     psk_key=PSK_KEY):
    return Lwm2mServer(coap.TlsServer(
        psk_identity=psk_identity,
        psk_key=psk_key,
        transport=Transport.UDP
    ))


def _init_app_with_psk_server(app,
                              server,
                              endpoint=ENDPOINT,
                              psk_identity=PSK_IDENTITY,
                              psk_key=PSK_KEY,
                              lifetime=LIFETIME,
                              udp_tx_params=None,
                              communication_retry=None,
                              bootstrap_config=None,
                              bootstrap_server=False):
    config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": f'coaps://127.0.0.1:{server.get_listen_port()}',
            "security": {
                    "kind": "psk",
                    "psk_identity": psk_identity,
                    "psk_key": psk_key,
                    },
            "lifetime": lifetime,
            "bootstrap": bootstrap_server
        }]
    }
    if communication_retry is not None:
        config["servers"][0]["communication_retry"] = communication_retry
    if udp_tx_params is not None:
        config["udp_tx_params"] = udp_tx_params
    if bootstrap_config is not None:
        config["bootstrap_config"] = bootstrap_config

    assert app.rpc.call("init", config) == 0


def _handle_register(server, endpoint=ENDPOINT, lifetime=LIFETIME,
                     timeout_s=None, accept_register=True, send_bad_request=False):
    expected = msgs.Lwm2mRegister(
        f'/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U')
    pkt = server.recv(timeout_s=timeout_s) if timeout_s else server.recv()
    utils.assert_msg_equal(expected, pkt)
    if accept_register:
        server.send(
            msgs.Lwm2mCreated.matching(pkt)(
                location=f"/rd/{endpoint}"))
    elif send_bad_request:
        server.send(
            msgs.Lwm2mErrorResponse.matching(pkt)(
                code=coap.Code.RES_BAD_REQUEST))
    return pkt


# Registration accepted with valid DTLS PSK credentials.
# The simplest test to verify that the client can successfully complete
# the DTLS handshake.
def test_registration_accepted_with_valid_dtls_psk_credentials(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)
        _handle_register(server)


# Registration rejected with wrong PSK key.
def test_registration_rejected_with_wrong_psk_key(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server, psk_key="wrong-test-key")
        utils.expect_dtls_handshake_rejected(server,
                                             utils.MBEDTLS_ERR_SSL_INVALID_MAC)
        # for default configuration, the client should still be in registering
        # state
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

    # for second attempt the client should go to the failed state, because we
    # don't allow any retries
    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(
            app,
            server,
            psk_key="wrong-test-key",
            communication_retry={
                "retry_count": 1,
                "retry_timer_s": 1,
                "seq_delay_timer_s": 1,
                "seq_retry_count": 1
            })
        utils.expect_dtls_handshake_rejected(server,
                                             utils.MBEDTLS_ERR_SSL_INVALID_MAC)
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.FAILURE


# Registration rejected with unknown PSK identity.
def test_registration_rejected_with_unknown_psk_identity(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server,
                                  psk_identity="wrong-test-identity")
        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY)
        # for default configuration, the client should still be in registering
        # state
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Registration retried after DTLS handshake timeout. Check if Anjay Lite and
# Mbedtls properly handle DTLS handshake retransmissions.
def test_registration_retried_after_dtls_handshake_timeout(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)

        # Catch the first ClientHello and drop it to cause the handshake
        # to timeout. With default handshake timeouts, the client should
        # retry the handshake.
        utils.expect_and_drop_dtls_client_hello(server)
        # Catch also the second ClientHello, for default configuration the
        # client should retry the handshake several times before giving up.
        utils.expect_and_drop_dtls_client_hello(server)
        # Let the next DTLS handshake retransmission reach the TlsServer.
        # Client should complete the handshake and register successfully.
        _handle_register(server)


def _prepare_server_for_next_dtls_connection(server):
    # After receiving close_notify, reset the test server to restore its
    # listening DTLS ServerSocket before accepting the next connection.
    server.reset()


# Communication sequence retry starts new handshake for next sequence.
# retry_count=1 is important here to trigger a full communication sequence
# retry after a single failed attempt, which allows us to verify that the
# next handshake is performed on a fresh DTLS connection.
@utils.app_config({'ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS': '2100'})
def test_communication_sequence_retry_starts_new_handshake_for_next_sequence(
        app_spawner):
    server = _make_psk_server()

    # Use a short lifetime and no retransmissions to trigger the communication
    # sequence retry mechanism quickly.
    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(
            app,
            server,
            udp_tx_params={
                "ack_timeout_s": 1,
                "ack_random_factor": 1.01,
                "max_retransmit": 1
            },
            communication_retry={
                "retry_count": 1,
                "retry_timer_s": 1,
                "seq_delay_timer_s": 3,
                "seq_retry_count": 4
            })

        # Sequence 1: DTLS handshake succeeds, but the Register exchange is not
        # answered. max_retransmit=1 means Anjay sends the Register twice before
        # treating this communication sequence as failed.
        _handle_register(server, accept_register=False)
        _handle_register(server, accept_register=False, send_bad_request=True)

        # After the Register exchange times out, Anjay Lite closes the current
        # DTLS connection. The next communication sequence shall start after
        # seq_delay_timer_s and perform a fresh DTLS handshake.
        utils.expect_dtls_close_notify(server)

        sequence_2_delay_start = time.monotonic()
        _prepare_server_for_next_dtls_connection(server)

        # Sequence 2 shall start after seq_delay_timer_s. Receiving the first
        # ClientHello is blocking, so measure the elapsed time immediately after
        # consuming it from the raw UDP socket.
        utils.expect_and_drop_dtls_client_hello(server)
        sequence_2_delay_s = time.monotonic() - sequence_2_delay_start
        assert 2.8 <= sequence_2_delay_s <= 3.2

        # Do not respond to the DTLS handshake in sequence 2. With
        # ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS=2100, the handshake attempt is:
        #   t = 0.0s  ClientHello, wait 1.0s for response
        #   t = 1.0s  ClientHello, wait 2.0s for response
        #   t = 3.0s  ClientHello, wait 2.1s for response, then timeout
        utils.expect_and_drop_dtls_client_hello(server)
        utils.expect_and_drop_dtls_client_hello(server)

        # wait for the third handshake to timeout before starting the
        # next sequence (ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS)
        time.sleep(2.1)

        sequence_3_delay_start = time.monotonic()
        _prepare_server_for_next_dtls_connection(server)
        _handle_register(server)
        # Add some tolerance for DTLS handshake processing time, calculation
        # don't need to be precise
        sequence_3_delay_s = time.monotonic() - sequence_3_delay_start
        assert 2.8 <= sequence_3_delay_s <= (3.2 + 0.5)


def _send_bootstrap_write_and_expect_changed(server, path, payload):
    write = msgs.Lwm2mWrite(
        path,
        format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
        content=payload)
    server.send(write)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), pkt)


def _handle_bootstrap_request(server, send_response=True):
    expected = msgs.Lwm2mRequestBootstrap(
        endpoint_name=ENDPOINT,
        preferred_content_format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR)
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    if send_response:
        server.send(msgs.Lwm2mChanged.matching(pkt)())


def _provision_regular_psk_server_via_bootstrap(bootstrap_server,
                                                regular_server):
    new_instance_ssid = 4
    security_payload = CBOR.serialize([
        {SenmlLabel.NAME: '/0/1/0',
         SenmlLabel.STRING: f'coaps://127.0.0.1:{regular_server.get_listen_port()}'},
        {SenmlLabel.NAME: '/0/1/2', SenmlLabel.VALUE: 0},
        {SenmlLabel.NAME: '/0/1/3', SenmlLabel.OPAQUE: PSK_IDENTITY.encode()},
        {SenmlLabel.NAME: '/0/1/5', SenmlLabel.OPAQUE: PSK_KEY.encode()},
        {SenmlLabel.NAME: '/0/1/10', SenmlLabel.VALUE: new_instance_ssid}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, '/0/1', security_payload)

    server_payload = CBOR.serialize([
        {SenmlLabel.NAME: '/1/0/0', SenmlLabel.VALUE: new_instance_ssid},
        {SenmlLabel.NAME: '/1/0/1', SenmlLabel.VALUE: LIFETIME},
        {SenmlLabel.NAME: '/1/0/6', SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: '/1/0/7', SenmlLabel.STRING: 'U'}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, '/1/0', server_payload)


def _finish_bootstrap_and_expect_close(server):
    finish = msgs.Lwm2mBootstrapFinish()
    server.send(finish)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(finish)(), pkt)

    # After Bootstrap Finish the bootstrap transport shall be closed before
    # connecting to the regular LwM2M Server.
    utils.expect_dtls_close_notify(server)


# Bootstrap accepted with valid DTLS PSK credentials and regular
# registration succeeds.
def test_bootstrap_with_psk_and_registration(app_spawner):
    bootstrap_server = _make_psk_server(psk_identity="bootstrap-test-identity",
                                        psk_key="bootstrap-test-key")
    regular_server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, bootstrap_server, bootstrap_server=True,
                                  psk_identity="bootstrap-test-identity",
                                  psk_key="bootstrap-test-key")

        _handle_bootstrap_request(bootstrap_server)
        _provision_regular_psk_server_via_bootstrap(
            bootstrap_server, regular_server)
        _finish_bootstrap_and_expect_close(bootstrap_server)

        _handle_register(regular_server)


def _trigger_bootstrap_request(server):
    # Bootstrap-Request Trigger is Server Object resource /1/0/9.
    trigger = msgs.Lwm2mExecute('/1/0/9')
    server.send(trigger)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(trigger)(), pkt)


def _handle_deregister(server):
    expected = msgs.Lwm2mDeregister(f'/rd/{ENDPOINT}')
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    server.send(msgs.Lwm2mDeleted.matching(pkt)())


# Bootstrap-Request Trigger causes the client to repeat the bootstrap
# procedure with the same PSK credentials.
def test_bootstrap_trigger_repeats_bootstrap_with_psk_and_registration(
        app_spawner):
    bootstrap_server = _make_psk_server(psk_identity="bootstrap-test-identity",
                                        psk_key="bootstrap-test-key")
    regular_server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, bootstrap_server,
                                  bootstrap_server=True,
                                  psk_identity="bootstrap-test-identity",
                                  psk_key="bootstrap-test-key")
        # First bootstrap procedure
        _handle_bootstrap_request(bootstrap_server)
        _provision_regular_psk_server_via_bootstrap(
            bootstrap_server, regular_server)
        _finish_bootstrap_and_expect_close(bootstrap_server)
        _prepare_server_for_next_dtls_connection(bootstrap_server)

        _handle_register(regular_server)
        # Trigger the bootstrap procedure again
        _trigger_bootstrap_request(regular_server)

        # Before switching back to the Bootstrap Server, the client de-registers
        # from the regular LwM2M Server and closes that DTLS connection.
        _handle_deregister(regular_server)
        utils.expect_dtls_close_notify(regular_server)
        _prepare_server_for_next_dtls_connection(regular_server)

        # Repeat the bootstrap procedure again with the same PSK credentials
        _handle_bootstrap_request(bootstrap_server)
        _provision_regular_psk_server_via_bootstrap(
            bootstrap_server, regular_server)
        _finish_bootstrap_and_expect_close(bootstrap_server)

        _handle_register(regular_server)


def test_bootstrap_request_retried_after_no_response(app_spawner):
    bootstrap_server = _make_psk_server(psk_identity="bootstrap-test-identity",
                                        psk_key="bootstrap-test-key")
    regular_server = _make_psk_server()
    BOOTSTRAP_RETRY_TIMEOUT_S = 2

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(
            app,
            bootstrap_server,
            bootstrap_server=True,
            psk_identity="bootstrap-test-identity",
            psk_key="bootstrap-test-key",
            udp_tx_params={
                "ack_timeout_s": 1,
                "ack_random_factor": 1.01,
                "max_retransmit": 0
            },
            bootstrap_config={
                "retry_count": 2,
                "retry_timeout_s": BOOTSTRAP_RETRY_TIMEOUT_S
            })

        # First bootstrap attempt: do not respond to the Bootstrap-Request
        _handle_bootstrap_request(bootstrap_server, send_response=False)
        utils.expect_dtls_close_notify(bootstrap_server)
        _prepare_server_for_next_dtls_connection(bootstrap_server)

        retry_delay_start = time.monotonic()
        # Second bootstrap attempt: do not respond to the Bootstrap-Request
        # again
        _handle_bootstrap_request(bootstrap_server, send_response=False)
        # Check that the retry is attempted after ~BOOTSTRAP_RETRY_TIMEOUT_S seconds
        # Add some tolerance for DTLS handshake processing time, calculation
        # don't need to be precise
        retry_delay_s = time.monotonic() - retry_delay_start
        assert (BOOTSTRAP_RETRY_TIMEOUT_S - 0.5 <=
                retry_delay_s <= BOOTSTRAP_RETRY_TIMEOUT_S + 0.5)
        utils.expect_dtls_close_notify(bootstrap_server)
        _prepare_server_for_next_dtls_connection(bootstrap_server)

        retry_delay_start = time.monotonic()
        # Third bootstrap attempt: respond to the Bootstrap-Request and
        # complete the bootstrap procedure
        _handle_bootstrap_request(bootstrap_server)
        retry_delay_s = time.monotonic() - retry_delay_start
        # Check that the retry is attempted after ~2*BOOTSTRAP_RETRY_TIMEOUT_S
        # seconds
        assert (
            2 * BOOTSTRAP_RETRY_TIMEOUT_S - 0.5
            <= retry_delay_s <=
            2 * BOOTSTRAP_RETRY_TIMEOUT_S + 0.5)
        _provision_regular_psk_server_via_bootstrap(
            bootstrap_server, regular_server)
        _finish_bootstrap_and_expect_close(bootstrap_server)

        _handle_register(regular_server)
