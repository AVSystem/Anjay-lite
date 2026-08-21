# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

"""
Integration tests for the Firmware Update Object (/5), in both Pull mode
(over CoAP and CoAPs) and Push mode.

The test app mocks the FOTA process (see app/src/fota_mock.c): for Pull
mode, it performs a real CoAP(s) download of the firmware package (using the
library's CoAP downloader against the coap_file_server started here); for
Push mode, the server writes the package directly to the client. In both
cases, "installing" the package and rebooting are mocked out.

The mocked firmware package format is a 4-byte big-endian Adler-32 checksum
of the payload, followed by the payload (see _build_package() below). The
app verifies this checksum itself, reporting
ANJ_DM_FW_UPDATE_RESULT_INTEGRITY_FAILURE on mismatch.
"""

import base64
import struct
import sys
import time
import zlib
import socket

import pytest

import framework_tools.lwm2m.messages as msgs
from framework_tools.coap_file_server import CoapFileServerThread
from framework_tools.lwm2m.coap.content_format import ContentFormat
from framework_tools.lwm2m.coap.transport import Transport
from framework_tools.lwm2m.senml_cbor import CBOR, SenmlLabel
from framework_tools.lwm2m.server import Lwm2mServer, coap

import utils

ENDPOINT = "test-fota-endpoint"
LIFETIME = 100

# PSK credentials for the CoAPs firmware download server; unrelated to the
# (nosec) LwM2M Server connection.
FOTA_PSK_IDENTITY = "fota-psk-identity"
FOTA_PSK_KEY = "fota-psk-key"

# PSK credentials for the LwM2M Server connection itself, used by the Push
# mode test below; unrelated to the FOTA download PSK credentials above.
LWM2M_PSK_IDENTITY = "test-identity"
LWM2M_PSK_KEY = "test-key"

# LwM2M Firmware Update Object (/5) resource paths.
FW_PACKAGE = "/5/0/0"
FW_PACKAGE_URI = "/5/0/1"
FW_UPDATE = "/5/0/2"
FW_STATE = "/5/0/3"
FW_UPDATE_RESULT = "/5/0/5"

STATE_IDLE = 0
STATE_DOWNLOADING = 1
STATE_DOWNLOADED = 2
STATE_UPDATING = 3

RESULT_INITIAL = 0
RESULT_SUCCESS = 1
RESULT_INTEGRITY_FAILURE = 5
RESULT_INVALID_URI = 7
RESULT_FAILED = 8

FIRMWARE_PATH = "/firmware"

CHECKSUM_HEADER_SIZE = 4

DEFAULT_BLOCK_SIZE = 1024


def _build_package(payload):
    checksum = zlib.adler32(payload) & 0xffffffff
    return struct.pack(">I", checksum) + payload


def _corrupt_checksum(package):
    corrupted = bytearray(package)
    corrupted[0] ^= 0xff
    return bytes(corrupted)


def _payload(size):
    return bytes((i % 256 for i in range(size)))


def _blocks_for_size(size):
    return max(1, -(-size // DEFAULT_BLOCK_SIZE))  # ceil division


class PropagatingThreadMixin:
    # anj_coap_downloader closes the DTLS connection right after a completed
    # download; CoapFileServerThread's background thread doesn't handle that
    # gracefully - its recv() loop just lets the resulting RuntimeError
    # (close_notify) propagate out of run(), which by default pytest reports
    # as an unhandled thread exception rather than failing the test. This
    # subclass instead captures that exception and re-raises it from join(),
    # so the test itself can assert on it like any other expected error.
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._thread_exception = None

    def run(self):
        try:
            super().run()
        except BaseException:
            self._thread_exception = sys.exception()

    def join(self):
        super().join()
        if self._thread_exception is not None:
            raise self._thread_exception


class ShortTimeoutCoapFileServerThread(CoapFileServerThread):
    # By default, CoapFileServerThread's handle_request loop uses a 5s timeout
    # which holds the mutex for a very long time, blocking the test thread
    # from interacting with the file server, which significantly slows tests down.
    #
    # TODO: just make it an argument of CoapFileServerThread instead of subclassing
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def run(self):
        while not self._shutdown:
            try:
                with self._mutex:
                    self._file_server.handle_request(timeout_s=0.1)
            except socket.timeout:
                self._timeout_occurred = True
            time.sleep(0.01)  # yield to the scheduler


class PropagatingShortTimeoutCoapFileServerThread(PropagatingThreadMixin, ShortTimeoutCoapFileServerThread):
    pass


def _make_file_server_thread(protocol):
    if protocol == "coaps":
        return PropagatingShortTimeoutCoapFileServerThread(coap_server=coap.DtlsServer(
            psk_identity=FOTA_PSK_IDENTITY,
            psk_key=FOTA_PSK_KEY,
        ))
    return PropagatingShortTimeoutCoapFileServerThread()


def _assert_download_requests(requests, expected_blocks):
    assert len(requests) == expected_blocks
    for i, req in enumerate(requests):
        assert req.code == coap.Code.REQ_GET
        assert req.get_uri_path() == FIRMWARE_PATH
        block2_options = req.get_options(coap.Option.BLOCK2)
        if i == 0:
            # The first GET request lets the server pick the block size, so
            # it never carries a Block2 option itself.
            assert not block2_options
        else:
            assert len(block2_options) == 1
            assert block2_options[0].seq_num() == i


def _read_resource(server, path):
    req = msgs.Lwm2mRead(path, accept=ContentFormat.TEXT_PLAIN)
    server.send(req)
    resp = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mContent.matching(req)(), resp)
    return resp.content


def _read_int_resource(server, path):
    return int(_read_resource(server, path))


def _wait_until_resource_equals(server, path, expected_value, timeout_s=10):
    # Cyclically (re-)reads the resource until it reaches the expected value,
    # driving forward state transitions that only happen lazily, e.g. once
    # the client has finished re-registering after a simulated reboot.
    deadline = time.time() + timeout_s
    value = None
    while time.time() < deadline:
        value = _read_int_resource(server, path)
        if value == expected_value:
            return value
        time.sleep(0.2)

    raise AssertionError(
        f"resource {path} did not reach value {expected_value} within "
        f"{timeout_s}s (last observed value: {value})")


def _register(app,
              server,
              endpoint=ENDPOINT,
              lifetime=LIFETIME,
              fota_reboot_required=False,
              protocol="coap",
              fota_coap_udp_tx_params=None):
    init_config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": f"coap://127.0.0.1:{server.get_listen_port()}",
            "security": {
                "kind": "nosec",
            },
            "lifetime": lifetime,
        }],
        "fota_reboot_required": fota_reboot_required,
    }
    if protocol == "coaps":
        init_config["fota_psk_identity"] = FOTA_PSK_IDENTITY
        init_config["fota_psk_key"] = FOTA_PSK_KEY
    if fota_coap_udp_tx_params is not None:
        init_config["fota_coap_udp_tx_params"] = fota_coap_udp_tx_params

    assert app.rpc.call("init", init_config) == 0

    _expect_register(server, endpoint, lifetime)


def _expect_register(server, endpoint=ENDPOINT, lifetime=LIFETIME):
    expected = msgs.Lwm2mRegister(
        f"/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U")
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    server.send(msgs.Lwm2mCreated.matching(pkt)(location=f"/rd/{endpoint}"))


def _enable_reuse(sock, enable=True):
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, int(enable))
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, int(enable))


def _reconnect_dtls_server(server):
    # After a Deregister, the client tears down its DTLS session (sending a
    # close_notify) and immediately opens a new one (with a new local port)
    # to re-register - often before this process even gets a chance to react
    # to the close_notify. server.reset() closes the current socket and only
    # *then* binds a new one, leaving a brief window where nothing is bound
    # to the port; if the client's first new ClientHello lands in that
    # window, it bounces off with an ICMP "port unreachable", forcing a slow
    # (60s by default) registration retry instead of an immediate reconnect.
    #
    # To avoid that, bind the *next* listening socket up front, before
    # waiting for the close_notify, with SO_REUSEADDR/SO_REUSEPORT so it can
    # coexist with the still-open old session socket (which, being connected
    # to a specific peer, keeps exclusively receiving that peer's traffic -
    # e.g. the close_notify - regardless). That way the new socket is
    # already there to catch the new ClientHello the instant it arrives.
    # Only once the close_notify has been read do we hand the new socket to
    # the server directly, instead of calling server.reset() (which would
    # otherwise discard it and bind yet another new socket from scratch).
    from pymbedtls import ServerSocket

    old_socket = server._raw_udp_socket
    new_socket = socket.socket(old_socket.family, socket.SOCK_DGRAM)
    # Both sockets need SO_REUSEADDR/SO_REUSEPORT to be allowed to bind to
    # the same port simultaneously.
    _enable_reuse(old_socket)
    _enable_reuse(new_socket)
    new_socket.bind(('', server.get_listen_port()))
    # Restore the server's usual reuse setting now that the bind is done -
    # old_socket is about to be closed, so there's no need to restore it.
    _enable_reuse(new_socket, server.reuse_port)

    utils.expect_dtls_close_notify(server)

    old_socket.close()
    server.socket = ServerSocket(server._pymbedtls_context, new_socket)
    server.accepted_connection = False


def _expect_deregister(server, endpoint=ENDPOINT, dtls=False):
    expected = msgs.Lwm2mDeregister(f"/rd/{endpoint}")
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    server.send(msgs.Lwm2mDeleted.matching(pkt)())
    if dtls:
        _reconnect_dtls_server(server)
    else:
        # The client opens a new connection (new local port) for the
        # subsequent Register, so the server socket needs to be reset to
        # accept it.
        server.reset()


@pytest.mark.parametrize("protocol", ["coap", "coaps"])
@pytest.mark.parametrize("reboot_required", [False, True],
                        ids=["immediate", "with_reboot"])
@pytest.mark.parametrize(
    "payload_size",
    [
        1,                                                  # single byte
        DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE,           # 1 full block
        DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE + 1,       # 2 blocks, 2nd tiny
        5 * DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE,       # 5 full blocks
        16 * DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE,      # 16 full blocks
    ],
    ids=["1B", "1_block", "2_blocks", "5_blocks", "16_blocks"])
def test_fota_coap_pull_mode_success(app_spawner, payload_size,
                                     reboot_required, protocol):
    server = Lwm2mServer()
    file_server_thread = _make_file_server_thread(protocol)
    file_server_thread.start()

    package = _build_package(_payload(payload_size))
    expected_blocks = _blocks_for_size(len(package))

    try:
        with app_spawner.spawn_app() as app:
            _register(app, server, fota_reboot_required=reboot_required,
                      protocol=protocol)

            with file_server_thread.file_server as file_server:
                file_server.set_resource(FIRMWARE_PATH, package)
                firmware_uri = file_server.get_resource_uri(FIRMWARE_PATH)

            # Write Package URI (/5/0/1) - starts the real CoAP(s) pull-mode
            # download performed by the app against the file server above.
            write = msgs.Lwm2mWrite(FW_PACKAGE_URI, firmware_uri)
            server.send(write)
            resp = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), resp)

            # Reaching Downloaded also confirms the app's own checksum
            # verification passed - see test_fota_coap_pull_mode_integrity_failure
            # for the mismatch case.
            _wait_until_resource_equals(server, FW_STATE, STATE_DOWNLOADED)
            assert _read_int_resource(
                server, FW_UPDATE_RESULT) == RESULT_INITIAL

            with file_server_thread.file_server as file_server:
                _assert_download_requests(file_server.requests,
                                         expected_blocks)

            # Execute Update (/5/0/2).
            execute = msgs.Lwm2mExecute(FW_UPDATE)
            server.send(execute)
            resp = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mChanged.matching(execute)(), resp)

            if reboot_required:
                # The app's FOTA mock simulates a device reboot: it
                # de-registers and registers again with the server before
                # reporting a successful update.
                _expect_deregister(server)
                _expect_register(server)
            # else: the app's FOTA mock does not actually install anything
            # or reboot - it reports success right away.

            _wait_until_resource_equals(server, FW_STATE, STATE_IDLE)
            assert _read_int_resource(
                server, FW_UPDATE_RESULT) == RESULT_SUCCESS
    finally:
        if protocol == "coaps":
            # See PropagatingCoapFileServerThread: the background thread's
            # recv() loop observes the client closing the DTLS connection
            # right after the completed download, surfacing it here as a
            # close_notify RuntimeError instead of an unhandled thread
            # exception.
            with pytest.raises(RuntimeError) as exc_info:
                file_server_thread.join()
            assert (utils.error_code_from_runtime_error(exc_info.value)
                    == utils.MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        else:
            file_server_thread.join()


def test_fota_coap_pull_mode_integrity_failure(app_spawner):
    server = Lwm2mServer()
    file_server_thread = _make_file_server_thread("coap")
    file_server_thread.start()

    package = _corrupt_checksum(_build_package(_payload(128)))

    try:
        with app_spawner.spawn_app() as app:
            _register(app, server)

            with file_server_thread.file_server as file_server:
                file_server.set_resource(FIRMWARE_PATH, package)
                firmware_uri = file_server.get_resource_uri(FIRMWARE_PATH)

            write = msgs.Lwm2mWrite(FW_PACKAGE_URI, firmware_uri)
            server.send(write)
            resp = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), resp)

            # The download completes at the CoAP level, but the app's own
            # checksum verification fails, so it resets to Idle with an
            # Integrity Failure result instead of moving to Downloaded.
            _wait_until_resource_equals(
                server, FW_UPDATE_RESULT, RESULT_INTEGRITY_FAILURE)
            assert _read_int_resource(server, FW_STATE) == STATE_IDLE
    finally:
        file_server_thread.join()


def test_fota_coap_pull_mode_invalid_uri(app_spawner):
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _register(app, server)

        # Not a coap:// or coaps:// URI; the downloader rejects it before
        # any network activity happens.
        write = msgs.Lwm2mWrite(FW_PACKAGE_URI, "not-a-valid-uri")
        server.send(write)
        resp = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(write)(
                code=coap.Code.RES_BAD_REQUEST),
            resp)

        # The object is left in a clean, unchanged state, and reports the
        # failure via the Update Result.
        assert _read_int_resource(server, FW_STATE) == STATE_IDLE
        assert _read_int_resource(
            server, FW_UPDATE_RESULT) == RESULT_INVALID_URI


def test_fota_coap_pull_mode_interrupted_by_empty_uri(app_spawner):
    server = Lwm2mServer()
    file_server_thread = _make_file_server_thread("coap")
    file_server_thread.start()

    # Large enough (many blocks) that the download is still in progress by
    # the time the empty-URI write below reaches the app.
    payload_size = 16 * DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE
    package = _build_package(_payload(payload_size))
    expected_blocks = _blocks_for_size(len(package))

    try:
        with app_spawner.spawn_app() as app:
            _register(app, server)

            with file_server_thread.file_server as file_server:
                file_server.set_resource(FIRMWARE_PATH, package)
                firmware_uri = file_server.get_resource_uri(FIRMWARE_PATH)

            write = msgs.Lwm2mWrite(FW_PACKAGE_URI, firmware_uri)
            server.send(write)
            resp = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), resp)

            assert _read_int_resource(server, FW_STATE) == STATE_DOWNLOADING

            # Writing an empty Package URI is a reset request (see
            # dm_fw_update.c): the library calls the reset handler
            # (aborting the download) and moves the state machine back to
            # Idle, synchronously within this same Write. A zero-length CoAP
            # payload accompanying a content-format option is legal per RFC
            # 7252, but the library currently has a bug where such a Write
            # is never dispatched to the resource write handler at all (the
            # exchange layer only invokes it when payload_size is nonzero).
            # As a workaround, the empty value is sent as SenML CBOR (with
            # an explicit zero-length string), which carries a nonzero
            # number of encoded bytes and so is unaffected by that bug.
            abort_payload = CBOR.serialize(
                [{ SenmlLabel.BASE_NAME: FW_PACKAGE_URI, SenmlLabel.STRING: "" }])
            abort_write = msgs.Lwm2mWrite(
                FW_PACKAGE_URI, abort_payload,
                format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR)
            server.send(abort_write)
            resp = server.recv()
            utils.assert_msg_equal(
                msgs.Lwm2mChanged.matching(abort_write)(), resp)

            assert _read_int_resource(server, FW_STATE) == STATE_IDLE
            assert _read_int_resource(
                server, FW_UPDATE_RESULT) == RESULT_INITIAL

            # Confirm the download was genuinely interrupted mid-flight,
            # rather than having already finished by the time it was
            # aborted.
            with file_server_thread.file_server as file_server:
                assert 0 < len(file_server.requests) < expected_blocks
    finally:
        file_server_thread.join()


# Number of Block2 GETs the file server answers normally before going silent,
# as if it (or the network) had suddenly died.
BLOCKS_BEFORE_SERVER_DEATH = 3


def test_fota_coap_pull_mode_server_dies_mid_transfer(app_spawner):
    server = Lwm2mServer()
    file_server_thread = _make_file_server_thread("coap")
    file_server_thread.start()

    payload_size = 16 * DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE
    package = _build_package(_payload(payload_size))

    try:
        with app_spawner.spawn_app() as app:
            _register(app, server, fota_coap_udp_tx_params={
                "ack_timeout_s": 1,
                "ack_random_factor": 1.0,
                "max_retransmit": 0,
            })

            with file_server_thread.file_server as file_server:
                file_server.set_resource(FIRMWARE_PATH, package)
                firmware_uri = file_server.get_resource_uri(FIRMWARE_PATH)
                # Answer the first few blocks normally, then go silent.
                file_server.should_ignore_request = (
                    lambda req: len(file_server.requests)
                    > BLOCKS_BEFORE_SERVER_DEATH)

            write = msgs.Lwm2mWrite(FW_PACKAGE_URI, firmware_uri)
            server.send(write)
            resp = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), resp)

            assert _read_int_resource(server, FW_STATE) == STATE_DOWNLOADING

            # The downloader aborts the download once the request for the
            # block after BLOCKS_BEFORE_SERVER_DEATH goes unanswered and
            # times out.
            _wait_until_resource_equals(
                server, FW_UPDATE_RESULT, RESULT_FAILED)
            assert _read_int_resource(server, FW_STATE) == STATE_IDLE

            with file_server_thread.file_server as file_server:
                assert len(file_server.requests) > BLOCKS_BEFORE_SERVER_DEATH
    finally:
        file_server_thread.join()


def _encode_push_package(package, content_format):
    if content_format == "plaintext_base64":
        return base64.b64encode(package), ContentFormat.TEXT_PLAIN
    return package, ContentFormat.APPLICATION_OCTET_STREAM


def _make_lwm2m_server(connection_security):
    if connection_security == "psk":
        return Lwm2mServer(coap.TlsServer(
            psk_identity=LWM2M_PSK_IDENTITY,
            psk_key=LWM2M_PSK_KEY,
            transport=Transport.UDP,
        ))
    return Lwm2mServer()


def _register_for_push_test(app,
                            server,
                            connection_security,
                            endpoint=ENDPOINT,
                            lifetime=LIFETIME,
                            fota_reboot_required=False):
    if connection_security == "psk":
        scheme = "coaps"
        security = {
            "kind": "psk",
            "psk_identity": LWM2M_PSK_IDENTITY,
            "psk_key": LWM2M_PSK_KEY,
        }
    else:
        scheme = "coap"
        security = {"kind": "nosec"}

    assert app.rpc.call("init", {
        "endpoint": endpoint,
        "servers": [{
            "uri": f"{scheme}://127.0.0.1:{server.get_listen_port()}",
            "security": security,
            "lifetime": lifetime,
        }],
        "fota_reboot_required": fota_reboot_required,
    }) == 0

    _expect_register(server, endpoint, lifetime)


def _split_into_blocks(data, block_size):
    return [data[i:i + block_size] for i in range(0, len(data), block_size)]


def _send_push_package_blockwise(server, content, format,
                                 final_response_matcher=None):
    chunks = _split_into_blocks(content, DEFAULT_BLOCK_SIZE)
    for idx, chunk in enumerate(chunks):
        has_more = idx != len(chunks) - 1
        write = msgs.Lwm2mWrite(
            FW_PACKAGE, chunk, format=format,
            options=[coap.Option.BLOCK1(seq_num=idx, has_more=has_more,
                                        block_size=DEFAULT_BLOCK_SIZE)])
        server.send(write)
        resp = server.recv()
        if has_more:
            utils.assert_msg_equal(msgs.Lwm2mContinue.matching(write)(), resp)
        elif final_response_matcher is not None:
            utils.assert_msg_equal(final_response_matcher(write), resp)
        else:
            utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), resp)
    return chunks


# plaintext_base64 grows the encoded content to 4/3 of the raw package
# size, so a payload sized for exactly one octet-stream block would spill
# into a second block once base64-encoded; using roughly 3/4 of a block's
# worth of payload keeps it within one block for both content formats.
ONE_BLOCK_PAYLOAD_SIZE = (DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE) * 3 // 4

PUSH_MODE_VARIANTS = [
    (1, False),
    (1, True),
    (ONE_BLOCK_PAYLOAD_SIZE, False),
    (ONE_BLOCK_PAYLOAD_SIZE, True),
    (DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE + 1, True),
    (5 * DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE, True),
    (16 * DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE, True),
]
PUSH_MODE_VARIANT_IDS = [
    "1B_single_write",
    "1B_blockwise",
    "1_block_single_write",
    "1_block_blockwise",
    "2_blocks",
    "5_blocks",
    "16_blocks",
]


@pytest.mark.parametrize("content_format", ["octet_stream", "plaintext_base64"])
@pytest.mark.parametrize("connection_security", ["nosec", "psk"])
@pytest.mark.parametrize("reboot_required", [False, True],
                        ids=["immediate", "with_reboot"])
@pytest.mark.parametrize("payload_size, blockwise", PUSH_MODE_VARIANTS,
                        ids=PUSH_MODE_VARIANT_IDS)
def test_fota_push_mode_success(app_spawner, payload_size, blockwise,
                                reboot_required, connection_security,
                                content_format):
    server = _make_lwm2m_server(connection_security)

    package = _build_package(_payload(payload_size))
    # plaintext_base64 grows the representation relative to the raw
    # (octet-stream) package, so the block count must be derived from the
    # encoded content below, not from payload_size/package size.
    content, format_ = _encode_push_package(package, content_format)

    with app_spawner.spawn_app() as app:
        _register_for_push_test(app, server, connection_security,
                                fota_reboot_required=reboot_required)

        if blockwise:
            expected_blocks = _blocks_for_size(len(content))
            chunks = _send_push_package_blockwise(server, content, format_)
            assert len(chunks) == expected_blocks
        else:
            write = msgs.Lwm2mWrite(FW_PACKAGE, content, format=format_)
            server.send(write)
            resp = server.recv()
            utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), resp)

        assert _read_int_resource(server, FW_STATE) == STATE_DOWNLOADED
        assert _read_int_resource(
            server, FW_UPDATE_RESULT) == RESULT_INITIAL

        # Execute Update (/5/0/2).
        execute = msgs.Lwm2mExecute(FW_UPDATE)
        server.send(execute)
        resp = server.recv()
        utils.assert_msg_equal(msgs.Lwm2mChanged.matching(execute)(), resp)

        if reboot_required:
            # The app's FOTA mock simulates a device reboot: it
            # de-registers and registers again with the server before
            # reporting a successful update.
            _expect_deregister(server, dtls=(connection_security == "psk"))
            _expect_register(server)
        # else: the app's FOTA mock does not actually install anything
        # or reboot - it reports success right away.

        _wait_until_resource_equals(server, FW_STATE, STATE_IDLE)
        assert _read_int_resource(
            server, FW_UPDATE_RESULT) == RESULT_SUCCESS


@pytest.mark.parametrize("content_format", ["octet_stream", "plaintext_base64"])
def test_fota_push_mode_integrity_failure(app_spawner, content_format):
    server = _make_lwm2m_server("nosec")
    package = _corrupt_checksum(_build_package(_payload(128)))
    content, format_ = _encode_push_package(package, content_format)

    with app_spawner.spawn_app() as app:
        _register_for_push_test(app, server, "nosec")

        # The checksum mismatch is detected while handling the single Write
        # that completes the package, so the library surfaces it as a
        # Write failure (5.00 Internal Server Error) rather than Changed.
        write = msgs.Lwm2mWrite(FW_PACKAGE, content, format=format_)
        server.send(write)
        resp = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(write)(
                code=coap.Code.RES_INTERNAL_SERVER_ERROR),
            resp)

        assert _read_int_resource(server, FW_STATE) == STATE_IDLE
        assert _read_int_resource(
            server, FW_UPDATE_RESULT) == RESULT_INTEGRITY_FAILURE


def test_fota_push_mode_blockwise_integrity_failure(app_spawner):
    server = _make_lwm2m_server("nosec")

    payload_size = 5 * DEFAULT_BLOCK_SIZE - CHECKSUM_HEADER_SIZE
    package = _corrupt_checksum(_build_package(_payload(payload_size)))

    with app_spawner.spawn_app() as app:
        _register_for_push_test(app, server, "nosec")

        # The checksum mismatch is only detected once the last block
        # completes the package, so all but the last block are Continue'd
        # normally, and only the final block's Write fails (5.00 Internal
        # Server Error) instead of Changed.
        _send_push_package_blockwise(
            server, package, ContentFormat.APPLICATION_OCTET_STREAM,
            final_response_matcher=lambda write: msgs.Lwm2mErrorResponse.matching(
                write)(code=coap.Code.RES_INTERNAL_SERVER_ERROR))

        assert _read_int_resource(server, FW_STATE) == STATE_IDLE
        assert _read_int_resource(
            server, FW_UPDATE_RESULT) == RESULT_INTEGRITY_FAILURE
