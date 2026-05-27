# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.transport import Transport
import framework_tools.lwm2m.messages as msgs

import utils


PSK_IDENTITY = "test-identity"
PSK_KEY = "test-key"
ENDPOINT = "test-endpoint"


def _make_psk_server(psk_identity=PSK_IDENTITY,
                     psk_key=PSK_KEY,
                     transport=Transport.UDP):
    return Lwm2mServer(coap.TlsServer(
        psk_identity=psk_identity,
        psk_key=psk_key,
        transport=transport
    ))


def _init_app_with_psk_server(app,
                              server,
                              endpoint=ENDPOINT,
                              psk_identity=PSK_IDENTITY,
                              psk_key=PSK_KEY,
                              lifetime=100):
    assert app.rpc.call("init", {
        "endpoint": endpoint,
        "servers": [{
            "uri": f'coaps://127.0.0.1:{server.get_listen_port()}',
            "security": {
                "kind": "psk",
                "psk_identity": psk_identity,
                "psk_key": psk_key
            },
            "lifetime": lifetime
        }]
    }) == 0


def _expect_register(server, endpoint=ENDPOINT, lifetime=100):
    expected = msgs.Lwm2mRegister(
        f'/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U')
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    assert pkt.content is not None
    return pkt


def _init_and_accept_register(app,
                              server,
                              endpoint=ENDPOINT,
                              psk_identity=PSK_IDENTITY,
                              psk_key=PSK_KEY,
                              lifetime=100):
    _init_app_with_psk_server(app, server, endpoint, psk_identity, psk_key,
                              lifetime)
    pkt = _expect_register(server, endpoint, lifetime)
    server.send(msgs.Lwm2mCreated.matching(pkt)(location=f"/rd/{endpoint}"))
    return pkt


def _expect_update(server, endpoint=ENDPOINT, content=b''):
    pkt = server.recv()
    utils.assert_msg_equal(
        msgs.Lwm2mUpdate(
            path=f"/rd/{endpoint}",
            content=content),
        pkt)
    return pkt

# Test that the client sends an Update message after the lifetime/2
# timeout expires.
def test_update_sent_after_lifetime_timeout(app_spawner):
    server = _make_psk_server()
    lifetime = 5

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server, lifetime=lifetime)

        pkt = utils.assert_received_after(
            server, expected_delay_s=lifetime / 2, tolerance_s=0.5)
        utils.assert_msg_equal(
            msgs.Lwm2mUpdate(
                path=f"/rd/{ENDPOINT}",
                content=b''),
            pkt)
        server.send(msgs.Lwm2mChanged.matching(pkt)())

# Test that the client sends an Update message when triggered by the server.
def test_update_sent_on_registration_update_trigger(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        execute = msgs.Lwm2mExecute('/1/0/8')
        server.send(execute)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mChanged.matching(execute)(), pkt)

        pkt = _expect_update(server)
        server.send(msgs.Lwm2mChanged.matching(pkt)())

# Test that the Update message contains the updated lifetime value after
# the lifetime is changed.
def test_update_contains_lifetime_after_lifetime_change(app_spawner):
    server = _make_psk_server()
    lifetime = 5

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server, lifetime=lifetime)

        pkt = utils.assert_received_after(server, expected_delay_s=lifetime / 2)
        server.send(msgs.Lwm2mChanged.matching(pkt)())

        # Change Lifetime which should force registration update to be sent immediately 
        write = msgs.Lwm2mWrite('/1/0/1', format=coap.ContentFormat.TEXT_PLAIN,
                                content=b'7')
        server.send(write)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), pkt)

        # Expect the Update message to contain the new lifetime value.
        pkt = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mUpdate(
                path=f"/rd/{ENDPOINT}",
                query=['lt=7'],
                content=b''),
            pkt)
        server.send(msgs.Lwm2mChanged.matching(pkt)())

        # Another update should be sent after the new lifetime/2 timeout expires
        # and it should not contain the lifetime in the query, as it was
        # already sent in the previous update
        pkt = utils.assert_received_after(server, expected_delay_s=7 / 2)
        utils.assert_msg_equal(
            msgs.Lwm2mUpdate(
                path=f"/rd/{ENDPOINT}",
                content=b''),
            pkt)

# Test that the client retries registration after receiving an error
# response to the Update message. `nosec` is used because there is no
# easy way to handle reconnection when Connection ID is used
def test_register_after_bad_update_response(app_spawner):
    server = Lwm2mServer(coap.Server(transport=Transport.UDP))
    lifetime = 5

    with app_spawner.spawn_app() as app:
        assert app.rpc.call("init", {
            "endpoint": ENDPOINT,
            "servers": [{
                "uri": f'coap://127.0.0.1:{server.get_listen_port()}',
                "security": {
                    "kind": "nosec"
                },
                "lifetime": lifetime,
            }]
        }) == 0

        pkt = _expect_register(server, ENDPOINT, lifetime)
        server.send(msgs.Lwm2mCreated.matching(pkt)(location=f"/rd/{ENDPOINT}"))

        execute = msgs.Lwm2mExecute('/1/0/8')
        server.send(execute)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mChanged.matching(execute)(), pkt)

        pkt = _expect_update(server)
        server.send(msgs.Lwm2mErrorResponse.matching(pkt)(code=coap.Code.RES_BAD_REQUEST))

        server.reset()

        pkt = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mRegister(
                f'/rd?ep={ENDPOINT}&lt={lifetime}&lwm2m=1.2&b=U'),
            pkt)
        server.send(msgs.Lwm2mCreated.matching(pkt)(location=f"/rd/{ENDPOINT}"))


# Test that the client retransmits Update messages according
# to the CoAP specification and stops after receiving an ACK.
def test_update_retransmitted_and_acked_on_fourth_attempt(app_spawner):
    server = _make_psk_server()
    lifetime = 5

    with app_spawner.spawn_app() as app:
        assert app.rpc.call("init", {
            "endpoint": ENDPOINT,
            "udp_tx_params": {
                "ack_timeout_s": 1,
                "ack_random_factor": 1.01,
                "max_retransmit": 5
            },
            "servers": [{
                "uri": f'coaps://127.0.0.1:{server.get_listen_port()}',
                "security": {
                    "kind": "psk",
                    "psk_identity": PSK_IDENTITY,
                    "psk_key": PSK_KEY
                },
                "lifetime": lifetime
            }]
        }) == 0

        pkt = _expect_register(server, ENDPOINT, lifetime)
        server.send(msgs.Lwm2mCreated.matching(pkt)(location=f"/rd/{ENDPOINT}"))

        execute = msgs.Lwm2mExecute('/1/0/8')
        server.send(execute)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mChanged.matching(execute)(), pkt)

        # First Update attempt
        first_update = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mUpdate(
                path=f"/rd/{ENDPOINT}",
                content=b''),
            first_update)

        # Do not respond - wait for first retransmission after ~1 s
        retransmit_1 = utils.assert_received_after(server, expected_delay_s=1.0, tolerance_s=0.25)
        utils.assert_msg_equal(first_update, retransmit_1)

        # Do not respond - wait for second retransmission after ~2 s
        retransmit_2 = utils.assert_received_after(server, expected_delay_s=2.0, tolerance_s=0.25)
        utils.assert_msg_equal(first_update, retransmit_2)

        # Wait for third retransmission after ~4 s and respond with ACK
        retransmit_3 = utils.assert_received_after(server, expected_delay_s=4.0, tolerance_s=0.25)
        utils.assert_msg_equal(first_update, retransmit_3)
        server.send(msgs.Lwm2mChanged.matching(retransmit_3)())
