# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

from framework_tools.lwm2m.server import Lwm2mServer, coap
import framework_tools.lwm2m.messages as msgs

import utils


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


@utils.app_config(
    [
        {
            'ANJ_LOG_LEVEL_DEFAULT': 'L_TRACE',
            'MBEDTLS_VERSION': '3.6.5',
        },
        {
            'ANJ_LOG_LEVEL_DEFAULT': 'L_TRACE',
            'MBEDTLS_VERSION': '3.6.6'
        },
        {
            'ANJ_LOG_LEVEL_DEFAULT': 'L_TRACE',
            'MBEDTLS_VERSION': '3.6.7',
        },
        {
            'ANJ_LOG_LEVEL_DEFAULT': 'L_TRACE',
            'MBEDTLS_VERSION': '3.6.7',
            # TODO: implement elegant way to pass this config from special field of app_config in EMB#5581
            'MBEDTLS_CFG_STR': 'MBEDTLS_SSL_DTLS_CONNECTION_ID=OFF;MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT=OFF',

        },
    ]
)
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

# build minimal anjay and check if all #ifdefs are set correctly
@utils.app_config({'ANJ_WITH_SECURITY': 'OFF', 'ANJ_WITH_MBEDTLS': 'OFF',
                   'ANJ_NET_WITH_DTLS': 'OFF', 'ANJ_WITH_BOOTSTRAP': 'OFF',
                   'ANJ_WITH_COMPOSITE_OPERATIONS': 'OFF', 'ANJ_WITH_OBSERVE': 'OFF',
                   'ANJ_WITH_DEFAULT_FOTA_OBJ': 'OFF', 'ANJ_WITH_LWM2M_SEND': 'OFF',
                   'ANJ_LOG_FULL': 'OFF', 'ANJ_WITH_LWM2M12': 'OFF',
                   'ANJ_WITH_RST_AS_CANCEL_OBSERVE': 'OFF', 'ANJ_WITH_LWM2M_CBOR': 'OFF',
                   'ANJ_WITH_DISCOVER_ATTR': 'OFF', 'ANJ_WITH_CERTIFICATES': 'OFF',
                   'ANJ_WITH_BOOTSTRAP_DISCOVER': 'OFF', 'ANJ_WITH_DISCOVER': 'OFF',
                   'ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS': 'OFF'})
def test_app_sends_register_minimal(app_spawner):
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
            '/rd?ep=test-endpoint&lt=50&lwm2m=1.1&b=U')
        pkt = server.recv()

        utils.assert_msg_equal(expected, pkt)
        assert pkt.content is not None
