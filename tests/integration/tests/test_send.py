# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import cbor2
import itertools

from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.content_format import ContentFormat
import framework_tools.lwm2m.messages as msgs
from framework_tools.lwm2m.senml_cbor import CBOR, SenmlLabel
import pytest

import utils
import itertools

ENDPOINT = "test-endpoint"
LIFETIME = 50


def _init_and_accept_register(app, server):
    assert app.rpc.call("init", {
        "endpoint": ENDPOINT,
        "servers": [{
            "uri": f"coap://127.0.0.1:{server.get_listen_port()}",
            "security": {
                "kind": "nosec",
            },
        }],
    }) == 0

    register = server.recv()
    utils.assert_msg_equal(
        msgs.Lwm2mRegister(
            f"/rd?ep={ENDPOINT}&lt={LIFETIME}&lwm2m=1.2&b=U"),
        register)
    server.send(msgs.Lwm2mCreated.matching(register)(location="/rd/1"))


def _send(app, resources, content_format="senml_cbor"):
    return app.rpc.call("send", {
        "content_format": content_format,
        "resources": resources,
    })


def _record(path, data_type):
    return {"path": path, "type": data_type}


def _senml_values(payload):
    values = {}
    base_name = ""
    for entry in CBOR.parse(payload):
        base_name = entry.get(SenmlLabel.BASE_NAME, base_name)
        path = base_name + entry.get(SenmlLabel.NAME, "")
        for label in (SenmlLabel.VALUE, SenmlLabel.STRING,
                      SenmlLabel.BOOL, SenmlLabel.OPAQUE,
                      SenmlLabel.OBJLNK):
            if label in entry:
                values[path] = entry[label]
                break
    return values


def _receive_send(
        server, content_format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR):
    packet = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mSend(format=content_format), packet)
    server.send(msgs.Lwm2mChanged.matching(packet)())
    return packet


def test_basic_send(app_spawner):
    """A client sends the requested resource in SenML CBOR."""
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        assert _send(app, [_record("/1/0/1", "uint")]) == 0
        packet = _receive_send(server)

        assert _senml_values(packet.content) == {"/1/0/1": LIFETIME}


@utils.app_config({"ANJ_OUT_PAYLOAD_BUFFER_SIZE": 128})
def test_send_with_block_transfer(app_spawner):
    """A Send payload larger than the output buffer is transferred using Block1."""
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        manufacturer = "M" * 128
        assert app.rpc.call("set_manufacturer", manufacturer) == 0

        assert _send(app, [_record("/3/0/0", "string"), _record("/3/0/1", "string"),
                           _record("/3/0/2", "string"), _record("/3/0/3", "string")]) == 0

        payload = bytearray()
        blocks = []
        for expected_seq_num in itertools.count():
            packet = server.recv()
            block1_options = packet.get_options(coap.Option.BLOCK1)
            assert len(block1_options) == 1
            block1 = block1_options[0]
            assert block1.seq_num() == expected_seq_num
            utils.assert_msg_equal(
                msgs.Lwm2mSend(options=[block1]), packet)

            blocks.append(block1)
            payload.extend(packet.content)
            if not block1.has_more():
                server.send(msgs.Lwm2mChanged.matching(packet)())
                break
            server.send(msgs.Lwm2mContinue.matching(packet)(options=[block1]))

        assert len(blocks) > 1
        assert _senml_values(payload) == {
            "/3/0/0": manufacturer,
            "/3/0/1": "Anjay Lite Test App",
            "/3/0/2": "123456789",
            "/3/0/3": "1.0",
        }


def test_send_respects_mute_send(app_spawner):
    ANJ_SEND_ERR_NOT_ALLOWED = -8
    """Mute Send prevents new Send operations until the resource is cleared."""
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        assert _send(app, [_record("/1/0/1", "uint")]) == 0
        _receive_send(server)

        write = msgs.Lwm2mWrite(
            "/1/0/23", format=ContentFormat.TEXT_PLAIN, content=b"1")
        server.send(write)
        utils.assert_msg_equal(
            msgs.Lwm2mChanged.matching(write)(), server.recv())

        assert _send(app, [_record("/1/0/1", "uint")]) == ANJ_SEND_ERR_NOT_ALLOWED
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.5)

        write = msgs.Lwm2mWrite(
            "/1/0/23", format=ContentFormat.TEXT_PLAIN, content=b"0")
        server.send(write)
        utils.assert_msg_equal(
            msgs.Lwm2mChanged.matching(write)(), server.recv())

        assert _send(app, [_record("/1/0/1", "uint")]) == 0
        _receive_send(server)


@utils.app_config({"ANJ_LWM2M_SEND_QUEUE_SIZE": 2})
def test_parallel_send_operations(app_spawner):
    """Two queued Send operations retain independent records and complete in valid order."""
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        assert _send(app, [_record("/1/0/1", "uint")]) == 0
        assert _send(app, [_record("/3/0/0", "string"),
                           _record("/3/0/1", "string")]) == 0

        first = _receive_send(server)
        second = _receive_send(server)

        assert _senml_values(first.content) == {"/1/0/1": LIFETIME}
        assert _senml_values(second.content) == {
            "/3/0/0": "AVSystem",
            "/3/0/1": "Anjay Lite Test App",
        }

# magic happens here


def _flatten_lwm2m_cbor(value, path=""):
    if not isinstance(value, dict):
        return {path: value}
    result = {}
    for key, nested_value in value.items():
        result.update(_flatten_lwm2m_cbor(
            nested_value, f"{path}/{key}"))
    return result


def test_send_with_lwm2m_cbor(app_spawner):
    """A Send operation encodes two resources using LwM2M CBOR."""
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        assert _send(
            app, [_record("/3/0/0", "string"),
                  _record("/3/0/1", "string")],
                  content_format="lwm2m_cbor") == 0
        packet = _receive_send(
            server, content_format=ContentFormat.APPLICATION_LWM2M_CBOR)

        assert _flatten_lwm2m_cbor(cbor2.loads(packet.content)) == {
            "/3/0/0": "AVSystem",
            "/3/0/1": "Anjay Lite Test App",
        }
