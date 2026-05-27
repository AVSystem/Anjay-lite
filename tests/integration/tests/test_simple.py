# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

from framework_tools.lwm2m.server import Lwm2mServer, coap
import framework_tools.lwm2m.messages as msgs

import utils


@utils.app_config({'ANJ_LOG_LEVEL_DEFAULT': 'L_TRACE'})
def test_app_sends_register(app_spawner):
    server = Lwm2mServer()
    port = server.get_listen_port()
    addr = f'coap://127.0.0.1:{port}'

    with app_spawner.spawn_app() as app:
        assert app.rpc.call("init", {
            "endpoint": "test-endpoint",
            "servers": [{
                "uri": addr,
                "security": {
                    "kind": "nosec"
                }
            }]
        }) == 0

        expected = msgs.Lwm2mRegister(
            '/rd?ep=test-endpoint&lt=50&lwm2m=1.2&b=U')
        pkt = server.recv()

        utils.assert_msg_equal(expected, pkt)
        assert pkt.content is not None


@utils.app_config({'ANJ_LOG_LEVEL_DEFAULT': 'L_TRACE'})
def test_app_sends_register_lifetime(app_spawner):
    server = Lwm2mServer()
    port = server.get_listen_port()
    addr = f'coap://127.0.0.1:{port}'

    with app_spawner.spawn_app() as app:
        assert app.rpc.call("init", {
            "endpoint": "test-endpoint",
            "servers": [{
                "uri": addr,
                "security": {
                    "kind": "nosec"
                },
                "lifetime": 10
            }]
        }) == 0

        expected = msgs.Lwm2mRegister(
            '/rd?ep=test-endpoint&lt=10&lwm2m=1.2&b=U')
        pkt = server.recv()

        utils.assert_msg_equal(expected, pkt)
        assert pkt.content is not None


@utils.app_config({'ANJ_LOG_LEVEL_DEFAULT': 'L_TRACE'})
def test_app_sends_register_psk(app_spawner):
    psk_identity = "identity"
    psk_key = "secret"

    server = Lwm2mServer(coap.TlsServer(
        psk_identity=psk_identity,
        psk_key=psk_key,
        transport=coap.transport.Transport.UDP
    ))
    port = server.get_listen_port()
    addr = f'coaps://127.0.0.1:{port}'

    with app_spawner.spawn_app() as app:
        assert app.rpc.call("init", {
            "endpoint": "test-endpoint",
            "servers": [{
                "uri": addr,
                "security": {
                    "kind": "psk",
                    "psk_identity": psk_identity,
                    "psk_key": psk_key
                }
            }]
        }) == 0

        expected = msgs.Lwm2mRegister(
            '/rd?ep=test-endpoint&lt=50&lwm2m=1.2&b=U')
        pkt = server.recv()

        utils.assert_msg_equal(expected, pkt)
        assert pkt.content is not None
