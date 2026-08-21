# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.transport import Transport
from framework_tools.lwm2m.coap.server_with_proxy import *
import framework_tools.lwm2m.messages as msgs

import utils
import time
import pytest

PSK_IDENTITY = "test-identity"
PSK_KEY = "test-key"
ENDPOINT = "test-endpoint"
CONNECTION_ID = "something"


def _make_psk_server(psk_identity=PSK_IDENTITY,
                     psk_key=PSK_KEY,
                     connection_id=CONNECTION_ID,
                     with_proxy=False):
    server_support_cid = True if len(connection_id) != 0 else False
    if with_proxy:
        return Lwm2mServer(CoapServerWithProxy(
            psk_identity=psk_identity,
            psk_key=psk_key,
            connection_id=connection_id,
            server_support_cid=server_support_cid
        ))
    else:
        return Lwm2mServer(coap.TlsServer(
            psk_identity=psk_identity,
            psk_key=psk_key,
            connection_id=connection_id,
            server_support_cid=server_support_cid,
            transport=Transport.UDP
        ))


def _init_app_with_psk_server(app,
                              server,
                              endpoint=ENDPOINT,
                              psk_identity=PSK_IDENTITY,
                              psk_key=PSK_KEY,
                              lifetime=100,
                              communication_retry=None):
    config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": f'coaps://127.0.0.1:{server.get_listen_port()}',
            "security": {
                "kind": "psk",
                "psk_identity": psk_identity,
                "psk_key": psk_key
            },
            "lifetime": lifetime
        }],
    }

    if communication_retry is not None:
        config["servers"][0]["communication_retry"] = communication_retry

    assert app.rpc.call("init", config) == 0


def _handel_register(server, endpoint=ENDPOINT, lifetime=100, send_bad_request=False):
    expected = msgs.Lwm2mRegister(
        f'/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U')
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    assert pkt.content is not None
    if send_bad_request:
        server.send(msgs.Lwm2mErrorResponse.matching(pkt)(
            code=coap.Code.RES_BAD_REQUEST))
    else:
        server.send(msgs.Lwm2mCreated.matching(
            pkt)(location=f"/rd/{endpoint}"))


# At the beginning of the test, things look like this:
#
# .------.      .-----------------------.      .-----------------------.      .--------------.
# | demo | <--> | client proxy (port A) |      |                       |      | Lwm2m Server |
# '------'      '-----------------------'      '-----------------------'      '--------------'
#
# That is, there's no path from app to the actual Lwm2m Server.
#
# The path is created when one calls with server.server_proxy(). The call creates
# a "server-side" proxy socket (filling the empty box), forwarding packets from client
# proxy to a backend Lwm2m Server:
#
# .------.      .-----------------------.      .-----------------------.      .--------------.
# | demo | <--> | client proxy (port A) | <--> | server proxy (port B) | <--> | Lwm2m Server |
# '------'      '-----------------------'      '-----------------------'      '--------------'
#
# From the Lwm2m Server perspective, the communication then looks like this (note that
# it sees the client using port B, due to packets being passed-through the "server proxy"):
#
# .--------------.                                       .--------------.
# | demo (port B)|                                       | Lwm2m Server |
# '--------------'                                       '--------------'
#       |    ----------- Client Hello connection_id() -------->   |
#       |                                                         |
#       |                             ...                         |
#       |                                                         |
#       |    <---- Server Hello + connection_id("something") --   |
#       |                                                         |
#       |                             ...                         |
#       |                                                         |
#       |    <--------------- regular LwM2M stuff ------------>   |
#
# After a while, the Client's port changes for some reason. This is represented as
# another call to with server.server_proxy(), which basically creates a new
# "server proxy" socket:
#                                         note the port change -.
#                                                               v
# .------.      .-----------------------.      .-----------------------.      .--------------.
# | demo | <--> | client proxy (port A) | <--> | server proxy (port C) | <--> | Lwm2m Server |
# '------'      '-----------------------'      '-----------------------'      '--------------'
#
# In normal circumstances it'd confuse the Server, and a re-registration or at least
# re-handshake would have happened. However, with connection_id extension, the Server
# recognizes the client and no additional communication is performed.
#
# .--------------.                                       .--------------.
# | demo (port C)|                                       | Lwm2m Server |
# '--------------'                                       '--------------'
#       |  ---- Lwm2M Update + connection_id("something") ---->   |
#       |                                                         |
#       |    <---------------- 2.04 Changed -------------------   |
#
def test_dtls_connection_id(app_spawner):
    server = _make_psk_server(with_proxy=True)

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)

        # Creat a "server-side" proxy socket
        with server.server_proxy():
            _handel_register(server, ENDPOINT)

            read = msgs.Lwm2mRead('/1/0')
            server.send(read)
            pkt = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mContent.matching(read)(), pkt)

        # Receive messages from all addresses and ports in pymbedtls
        disconnect_socket(server.socket.py_socket)

        # Creat a new "server-side" proxy socket with different port
        with server.server_proxy():
            # Force demo to send Update operation to connect
            # pymbedtls socket with a new proxy port address
            app.rpc.call("send_update")

            update = server.recv()
            server.send(msgs.Lwm2mChanged.matching(update)())


def test_dtls_without_connection_id_on_server_side(app_spawner):
    server = _make_psk_server(with_proxy=True, connection_id='')

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)

        # Creat a "server-side" proxy socket
        with server.server_proxy():
            _handel_register(server, ENDPOINT)

            read = msgs.Lwm2mRead('/1/0')
            server.send(read)
            pkt = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mContent.matching(read)(), pkt)

        # Receive messages from all addresses and ports in pymbedtls
        disconnect_socket(server.socket.py_socket)

        # Nonetheless, connection_id was not used, so we should expect that the server
        # ignores Update messages messages.
        with server.server_proxy():
            app.rpc.call("send_update")

            with pytest.raises(socket.timeout):
                server.recv()


@utils.app_config(
    {
        'MBEDTLS_CFG_STR': 'MBEDTLS_SSL_DTLS_CONNECTION_ID=OFF;MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT=OFF',
    }
)
def test_dtls_without_connection_id_on_client_side(app_spawner):
    server = _make_psk_server(with_proxy=True)

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)

        # Creat a "server-side" proxy socket
        with server.server_proxy():
            _handel_register(server, ENDPOINT)

            read = msgs.Lwm2mRead('/1/0')
            server.send(read)
            pkt = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mContent.matching(read)(), pkt)

        # Receive messages from all addresses and ports in pymbedtls
        disconnect_socket(server.socket.py_socket)

        # Nonetheless, connection_id was not used, so we should expect that the server
        # ignores Update messages messages.
        with server.server_proxy():
            app.rpc.call("send_update")

            with pytest.raises(socket.timeout):
                server.recv()


def test_dtls_with_connection_id_communication_sequence(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server, communication_retry={
            "retry_count": 2,
            "retry_timer_s": 1,
            "seq_delay_timer_s": 1,
            "seq_retry_count": 2
        })

        _handel_register(server, send_bad_request=True)

        # Disconnect socket because next register operation will come from different port
        disconnect_socket(server.socket.py_socket)

        _handel_register(server, send_bad_request=True)

        # Anjay will clean DTLS connection and do a new handshake
        utils.expect_dtls_close_notify(server)
        server.reset()

        _handel_register(server, send_bad_request=True)

        # Disconnect socket because next register operation will come from different port
        disconnect_socket(server.socket.py_socket)

        # Happy ending
        _handel_register(server)

def _dtls_client_hello_received(server):
    # Verify that Anjay send a new client hello msg,
    # Unfortunately, this check means that we have
    # to reject this message, but that’s not a problem,
    # because it will be retransmitted.
    utils.expect_and_drop_dtls_client_hello(server)

def test_dtls_without_connection_id_on_server_side_communication_sequence(app_spawner):
    server = _make_psk_server(connection_id='')

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server, communication_retry={
            "retry_count": 2,
            "retry_timer_s": 1,
            "seq_delay_timer_s": 1,
            "seq_retry_count": 2
        })

        _handel_register(server, send_bad_request=True)

        # Anjay will close socket and do a new handshake
        server.reset()
        _dtls_client_hello_received(server)

        _handel_register(server, send_bad_request=True)

        # Anjay will clean DTLS connection and do a new handshake
        utils.expect_dtls_close_notify(server)
        server.reset()

        _handel_register(server, send_bad_request=True)

        # Anjay will close socket and do a new handshake
        server.reset()
        _dtls_client_hello_received(server)

        # Happy ending
        _handel_register(server)


@utils.app_config(
    {
        'MBEDTLS_CFG_STR': 'MBEDTLS_SSL_DTLS_CONNECTION_ID=OFF;MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT=OFF',
    }
)
def test_dtls_without_connection_id_on_client_side_communication_sequence(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server, communication_retry={
            "retry_count": 2,
            "retry_timer_s": 1,
            "seq_delay_timer_s": 1,
            "seq_retry_count": 2
        })

        _handel_register(server, send_bad_request=True)

        # Anjay will close socket and do a new handshake
        server.reset()
        _dtls_client_hello_received(server)

        _handel_register(server, send_bad_request=True)

        # Anjay will clean DTLS connection and do a new handshake
        utils.expect_dtls_close_notify(server)
        server.reset()

        _handel_register(server, send_bad_request=True)

        # Anjay will close socket and do a new handshake
        server.reset()
        _dtls_client_hello_received(server)

        # Happy ending
        _handel_register(server)


def test_dtls_with_connection_id_suspend(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)

        _handel_register(server, ENDPOINT)

        assert app.rpc.call("disable_server", 1) == 0
        expected = msgs.Lwm2mDeregister(f'/rd/{ENDPOINT}')
        pkt = server.recv()
        utils.assert_msg_equal(expected, pkt)
        server.send(msgs.Lwm2mDeleted.matching(pkt)())

        # Disconnect socket because next register operation will come from different port
        disconnect_socket(server.socket.py_socket)
        _handel_register(server, ENDPOINT)

        read = msgs.Lwm2mRead('/1/0')
        server.send(read)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mContent.matching(read)(), pkt)


def test_dtls_without_connection_id_on_server_side_suspend(app_spawner):
    server = _make_psk_server(connection_id='')

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)

        _handel_register(server, ENDPOINT)

        assert app.rpc.call("disable_server", 1) == 0
        expected = msgs.Lwm2mDeregister(f'/rd/{ENDPOINT}')
        pkt = server.recv()
        utils.assert_msg_equal(expected, pkt)
        server.send(msgs.Lwm2mDeleted.matching(pkt)())

        # Anjay will close socket and do a new handshake
        server.reset()
        _dtls_client_hello_received(server)

        _handel_register(server, ENDPOINT)

        read = msgs.Lwm2mRead('/1/0')
        server.send(read)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mContent.matching(read)(), pkt)


@utils.app_config(
    {
        'MBEDTLS_CFG_STR': 'MBEDTLS_SSL_DTLS_CONNECTION_ID=OFF;MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT=OFF',
    }
)
def test_dtls_without_connection_id_on_client_side_suspend(app_spawner):
    server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_psk_server(app, server)

        _handel_register(server, ENDPOINT)

        assert app.rpc.call("disable_server", 1) == 0
        expected = msgs.Lwm2mDeregister(f'/rd/{ENDPOINT}')
        pkt = server.recv()
        utils.assert_msg_equal(expected, pkt)
        server.send(msgs.Lwm2mDeleted.matching(pkt)())

        # Anjay will close socket and do a new handshake
        server.reset()
        _dtls_client_hello_received(server)

        _handel_register(server, ENDPOINT)

        read = msgs.Lwm2mRead('/1/0')
        server.send(read)
        pkt = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mContent.matching(read)(), pkt)
