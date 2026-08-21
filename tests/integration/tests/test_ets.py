# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

# The tests defined here follow loosely following specification:
# https://www.openmobilealliance.org/release/LightweightM2M/ETS/OMA-ETS-LightweightM2M_INT-V1_2_1-20240312-C.pdf

import time
import pytest

from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.transport import Transport
from framework_tools.lwm2m.senml_cbor import CBOR, SenmlLabel
from framework_tools.lwm2m.coap.content_format import ContentFormat
import framework_tools.lwm2m.messages as msgs

import utils


PSK_IDENTITY = "test-identity"
PSK_KEY = "test-key"
BOOTSTRAP_PSK_KEY = "bootstrap-test-key"
ENDPOINT = "test-endpoint"
LWM2M_VERSION = "1.1"


def _make_psk_server(psk_identity=PSK_IDENTITY,
                     psk_key=PSK_KEY,
                     transport=Transport.UDP):
    return Lwm2mServer(coap.TlsServer(
        psk_identity=psk_identity,
        psk_key=psk_key,
        transport=transport
    ))


def _server_uri(server):
    return f'coaps://127.0.0.1:{server.get_listen_port()}'


def _psk_security(psk_identity=PSK_IDENTITY, psk_key=PSK_KEY):
    return {
        "kind": "psk",
        "psk_identity": psk_identity,
        "psk_key": psk_key
    }


def _init_app_with_psk_server(app,
                              server,
                              endpoint=ENDPOINT,
                              psk_identity=PSK_IDENTITY,
                              psk_key=PSK_KEY,
                              lifetime=86400):
    assert app.rpc.call("init", {
        "endpoint": endpoint,
        "servers": [{
            "uri": _server_uri(server),
            "security": _psk_security(psk_identity, psk_key),
            "lifetime": lifetime
        }]
    }) == 0


def _expect_register(server, endpoint=ENDPOINT, lifetime=86400):
    expected = msgs.Lwm2mRegister(
        f'/rd?ep={endpoint}&lt={lifetime}&lwm2m={LWM2M_VERSION}&b=U')
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    assert pkt.content is not None
    return pkt


def _accept_register(server, pkt, endpoint=ENDPOINT):
    server.send(msgs.Lwm2mCreated.matching(pkt)(location=f"/rd/{endpoint}"))


def _init_and_accept_register(app, server, lifetime=86400):
    _init_app_with_psk_server(app, server, lifetime=lifetime)
    pkt = _expect_register(server, lifetime=lifetime)
    _accept_register(server, pkt)


def _read_text_resource(server, path, content):
    read = msgs.Lwm2mRead(path, accept=ContentFormat.TEXT_PLAIN)
    server.send(read)
    pkt = server.recv()
    utils.assert_msg_equal(
        msgs.Lwm2mContent.matching(read)(content=content),
        pkt)


def _write_text_resource(server, path, content):
    write = msgs.Lwm2mWrite(
        path, format=ContentFormat.TEXT_PLAIN, content=content)
    server.send(write)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), pkt)


def _expect_send_lifetime(server, lifetime=86400):
    expected = msgs.Lwm2mSend()
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)

    content = CBOR.parse(pkt.content)
    assert len(content) == 1
    assert content[0][SenmlLabel.BASE_NAME] == '/1/0/1'
    assert content[0][SenmlLabel.VALUE] == lifetime

    server.send(msgs.Lwm2mChanged.matching(pkt)())
    return pkt


def _send_lifetime(app):
    return app.rpc.call("send", {
        "content_format": "senml_cbor",
        "resources": [{"path": "/1/0/1", "type": "uint"}]
        })

def _send_write_and_expect_changed(server, path, payload,
                                   format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR):
    write = msgs.Lwm2mWrite(path, format=format, content=payload)
    server.send(write)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), pkt)


@utils.app_config({'ANJ_WITH_LWM2M12': 'OFF', 'ANJ_WITH_LWM2M_CBOR': 'OFF'})
def test_int_0(app_spawner):
    bootstrap_server = _make_psk_server(psk_key=BOOTSTRAP_PSK_KEY)
    server = _make_psk_server()

    bootstrap_addr = _server_uri(bootstrap_server)
    addr = _server_uri(server)

    with app_spawner.spawn_app() as app:
        assert app.rpc.call("init", {
            "endpoint": ENDPOINT,
            "servers": [{
                "bootstrap": True,
                "uri": bootstrap_addr,
                "security": _psk_security(PSK_IDENTITY, BOOTSTRAP_PSK_KEY),
            }]
        }) == 0

        expected = msgs.Lwm2mRequestBootstrap(
            endpoint_name=ENDPOINT,
            preferred_content_format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR
        )
        pkt = bootstrap_server.recv()
        utils.assert_msg_equal(expected, pkt)

        bootstrap_server.send(msgs.Lwm2mChanged.matching(pkt)())

        payload = CBOR.serialize([
            {SenmlLabel.NAME: '/0/0/0', SenmlLabel.STRING: addr},
            {SenmlLabel.NAME: '/0/0/2', SenmlLabel.VALUE: 0},
            {SenmlLabel.NAME: '/0/0/3', SenmlLabel.OPAQUE: PSK_IDENTITY.encode()},
            {SenmlLabel.NAME: '/0/0/5', SenmlLabel.OPAQUE: PSK_KEY.encode()},
            {SenmlLabel.NAME: '/0/0/10', SenmlLabel.VALUE: 67}
        ])
        _send_write_and_expect_changed(bootstrap_server, '/0/0', payload)

        payload = CBOR.serialize([
            {SenmlLabel.NAME: '/1/0/0', SenmlLabel.VALUE: 67},
            {SenmlLabel.NAME: '/1/0/1', SenmlLabel.VALUE: 60},
            {SenmlLabel.NAME: '/1/0/7', SenmlLabel.STRING: 'U'},
            {SenmlLabel.NAME: '/1/0/6', SenmlLabel.BOOL: False}
        ])
        _send_write_and_expect_changed(bootstrap_server, '/1/0', payload)

        finish = msgs.Lwm2mBootstrapFinish()
        bootstrap_server.send(finish)
        pkt = bootstrap_server.recv()
        utils.assert_msg_equal(msgs.Lwm2mChanged.matching(finish)(), pkt)

        pkt = _expect_register(server, lifetime=60)
        assert pkt.content is not None


@utils.app_config({'ANJ_WITH_LWM2M12': 'OFF', 'ANJ_WITH_LWM2M_CBOR': 'OFF'})
def test_int_101(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)
        pkt = _expect_register(server)

        assert b'</1>' in pkt.content
        assert b'</0>' not in pkt.content
        assert b'</21>' not in pkt.content


@utils.app_config({'ANJ_WITH_LWM2M12': 'OFF', 'ANJ_WITH_LWM2M_CBOR': 'OFF'})
def test_int_104(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server, lifetime=20)

        time.sleep(5)

        execute = msgs.Lwm2mExecute('/1/0/8')
        server.send(execute)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mChanged.matching(execute)(), pkt)

        pkt = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mUpdate(path=f"/rd/{ENDPOINT}", content=b''),
            pkt)
        server.send(msgs.Lwm2mChanged.matching(pkt)())

        with pytest.raises(TimeoutError):
            server.recv(timeout_s=8)


@utils.app_config({'ANJ_WITH_LWM2M12': 'OFF', 'ANJ_WITH_LWM2M_CBOR': 'OFF'})
def test_int_201(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        _read_text_resource(server, '/3/0/0', b'AVSystem')
        _read_text_resource(server, '/3/0/1', b'Anjay Lite Test App')
        _read_text_resource(server, '/3/0/2', b'123456789')


@utils.app_config({'ANJ_WITH_LWM2M12': 'OFF', 'ANJ_WITH_LWM2M_CBOR': 'OFF'})
def test_int_223(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        read = msgs.Lwm2mRead('/1/0')
        server.send(read)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mContent.matching(read)(), pkt)

        read = msgs.Lwm2mRead('/3/0')
        server.send(read)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mContent.matching(read)(), pkt)


@utils.app_config({'ANJ_WITH_LWM2M12': 'OFF', 'ANJ_WITH_LWM2M_CBOR': 'OFF'})
def test_int_306(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        assert _send_lifetime(app) == 0
        _expect_send_lifetime(server)


@utils.app_config({'ANJ_WITH_LWM2M12': 'OFF', 'ANJ_WITH_LWM2M_CBOR': 'OFF'})
def test_int_307(app_spawner):
    ANJ_SEND_ERR_NOT_ALLOWED = -8
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        assert _send_lifetime(app) == 0
        _expect_send_lifetime(server)

        _write_text_resource(server, '/1/0/23', b'1')
        assert _send_lifetime(app) == ANJ_SEND_ERR_NOT_ALLOWED

        _write_text_resource(server, '/1/0/23', b'0')
        assert _send_lifetime(app) == 0

        _expect_send_lifetime(server)
