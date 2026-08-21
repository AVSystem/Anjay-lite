# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import time

import pytest

import certs

from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.transport import Transport
from framework_tools.lwm2m.coap.content_format import ContentFormat
from framework_tools.lwm2m.senml_cbor import CBOR, SenmlLabel
import framework_tools.lwm2m.messages as msgs

import utils


ENDPOINT = "test-endpoint"
LIFETIME = 100
REGULAR_SSID = 1

SECURITY_MODE_NOSEC = 3

def _make_nosec_server():
    return Lwm2mServer(coap.Server(transport=Transport.UDP))

def _server_uri(server):
    return f"coap://127.0.0.1:{server.get_listen_port()}"

def _init_app_nosec_server(app,
                           server,
                           endpoint=ENDPOINT,
                           lifetime=LIFETIME,
                           udp_tx_params=None,
                           queue_mode=False,
                           queue_mode_timeout_s=None):
    config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": _server_uri(server),
            "security": {
                "kind": "nosec"
            },
            "bootstrap": False,
            "lifetime": lifetime,
        }],
    }

    if udp_tx_params is not None:
        config["udp_tx_params"] = udp_tx_params
    if queue_mode:
        config["queue_mode"] = True
    if queue_mode_timeout_s is not None:
        config["queue_mode_timeout_s"] = queue_mode_timeout_s

    assert app.rpc.call("init", config) == 0


def _handle_register(server,
                     endpoint=ENDPOINT,
                     lifetime=LIFETIME,
                     accept_register=True,
                     timeout_s=None,
                     queue_mode=False):
    queue_mode_query = "&Q" if queue_mode else ""
    expected = msgs.Lwm2mRegister(
        f"/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U"
        f"{queue_mode_query}")
    pkt = server.recv(
        timeout_s=timeout_s) if timeout_s is not None else server.recv()
    utils.assert_msg_equal(expected, pkt)
    assert pkt.content is not None
    if accept_register:
        server.send(
            msgs.Lwm2mCreated.matching(pkt)(
                location=f"/rd/{endpoint}"))
    return pkt

def _handle_deregister(server, endpoint=ENDPOINT):
    expected = msgs.Lwm2mDeregister(f"/rd/{endpoint}")
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    server.send(msgs.Lwm2mDeleted.matching(pkt)())
    return pkt


def _handle_update(server, endpoint=ENDPOINT, query=None, content=b""):
    expected = msgs.Lwm2mUpdate(
        path=f"/rd/{endpoint}",
        content=content,
        query=query or [])
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    server.send(msgs.Lwm2mChanged.matching(pkt)())
    return pkt


def _send_confirmable_separate_response(server, request, response):
    server.send(msgs.Lwm2mEmpty.matching(request)())

    response.type = coap.Type.CONFIRMABLE
    response.msg_id = (request.msg_id + 1) % (2 ** 16)
    server.send(response)

    utils.assert_msg_equal(
        msgs.Lwm2mEmpty.matching(response)(), server.recv())


def _wait_for_conn_status(app, expected_status, timeout_s=2):
    deadline = time.monotonic() + timeout_s
    status = None
    while time.monotonic() < deadline:
        status = app.rpc.call("get_conn_status")
        if status == expected_status:
            return
        time.sleep(0.05)
    pytest.fail(f"connection status is {status}, expected {expected_status}")


def _enter_queue_mode(app, timeout_s=5):
    assert app.rpc.call(
        "add_monotonic_time_offset", (timeout_s + 1) * 1000) == 0
    _wait_for_conn_status(app, utils.ConnStatus.QUEUE_MODE)


def _send_lifetime(app):
    return app.rpc.call("send", {
        "content_format": "senml_cbor",
        "resources": [{"path": "/1/0/1", "type": "uint"}],
    })


def test_register_recovers_from_temporary_packet_loss(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(
            app,
            server,
            udp_tx_params={
                "ack_timeout_s": 1,
                "ack_random_factor": 1.01,
                "max_retransmit": 2,
            })

        first_register = _handle_register(server, accept_register=False)
        retransmission = server.recv(timeout_s=2)
        utils.assert_msg_equal(first_register, retransmission)
        server.send(
            msgs.Lwm2mCreated.matching(retransmission)(
                location=f"/rd/{ENDPOINT}"))
        _wait_for_conn_status(app, utils.ConnStatus.REGISTERED)

def test_register_disable_client(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        write = msgs.Lwm2mWrite(
            "/1/0/5", format=ContentFormat.TEXT_PLAIN, content=b"120")
        server.send(write)
        utils.assert_msg_equal(
            msgs.Lwm2mChanged.matching(write)(), server.recv())

        trigger = msgs.Lwm2mExecute("/1/0/4")
        server.send(trigger)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mChanged.matching(trigger)(), pkt)

        _handle_deregister(server)

        server.reset()
        app.rpc.call("add_monotonic_time_offset", 100*1000)

        # Check if there was no communication with the client
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.5)

        app.rpc.call("add_monotonic_time_offset", 20*1000)

        _handle_register(server)

def test_register_change_lifetime(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server, lifetime=20)
        _handle_register(server, lifetime=20)

        app.rpc.call("add_monotonic_time_offset", 5*1000)

        # Check if there was no communication with the client
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.5)

        app.rpc.call("add_monotonic_time_offset", 6*1000)

        _handle_update(server)

        write = msgs.Lwm2mWrite(
            "/1/0/1", format=ContentFormat.TEXT_PLAIN, content=b"50")
        server.send(write)
        utils.assert_msg_equal(
            msgs.Lwm2mChanged.matching(write)(), server.recv())

        _handle_update(server, query=["lt=50"])

        app.rpc.call("add_monotonic_time_offset", 20*1000)

        # Check if there was no communication with the client
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.5)

        app.rpc.call("add_monotonic_time_offset", 5*1000)

        _handle_update(server)


def test_register_user_suspend_and_early_restart(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        assert app.rpc.call("disable_server", 60) == 0
        _handle_deregister(server)
        _wait_for_conn_status(app, utils.ConnStatus.SUSPENDED)

        server.reset()
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.5)

        assert app.rpc.call("restart_client") == 0
        _handle_register(server)


def test_update_contains_changed_data_model(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(app, server)
        _handle_register(server)

        assert app.rpc.call("remove_test_object") == 0

        update = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mUpdate(
                path=f"/rd/{ENDPOINT}", content=update.content),
            update)
        assert update.get_content_format() == ContentFormat.APPLICATION_LINK
        assert b"</3/0>" in update.content
        assert b"</1234>" not in update.content
        assert b"</1234/0>" not in update.content
        server.send(msgs.Lwm2mChanged.matching(update)())

        # Once acknowledged, a forced Update no longer needs to carry the model.
        app.rpc.call("send_update")
        _handle_update(server)


def test_queue_mode_exits_for_update(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(
            app, server, queue_mode=True, queue_mode_timeout_s=5)
        _handle_register(server, queue_mode=True)
        _enter_queue_mode(app)

        app.rpc.call("send_update")
        _handle_update(server)
        _wait_for_conn_status(app, utils.ConnStatus.REGISTERED)


def test_queue_mode_exits_for_send(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(
            app, server, queue_mode=True, queue_mode_timeout_s=5)
        _handle_register(server, queue_mode=True)
        _enter_queue_mode(app)

        assert _send_lifetime(app) == 0
        send = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mSend(), send)
        records = CBOR.parse(send.content)
        assert records == [{
            SenmlLabel.BASE_NAME: "/1/0/1",
            SenmlLabel.VALUE: LIFETIME,
        }]
        server.send(msgs.Lwm2mChanged.matching(send)())
        _wait_for_conn_status(app, utils.ConnStatus.REGISTERED)


def test_queue_mode_exits_for_notification(app_spawner):
    server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_nosec_server(
            app, server, queue_mode=True, queue_mode_timeout_s=5)
        _handle_register(server, queue_mode=True)

        observe = msgs.Lwm2mObserve("/1234/0/0")
        server.send(observe)
        response = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mContent.matching(observe)(), response)
        assert len(response.get_options(coap.Option.OBSERVE)) == 1

        _enter_queue_mode(app)
        assert app.rpc.call("set_test_value", 0, 42.0) == 0

        notification = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mNotify(token=observe.token), notification)
        _wait_for_conn_status(app, utils.ConnStatus.REGISTERED)
