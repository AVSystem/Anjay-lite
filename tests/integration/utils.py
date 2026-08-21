# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import time
import re
from enum import IntEnum

from typing import Iterable, Sequence, Tuple
from typing_extensions import Unpack, TypeVarTuple
from framework_tools.lwm2m.messages import ANY
import pytest


class ConnStatus(IntEnum):
    INITIAL = 0
    INVALID = 1
    FAILURE = 2
    BOOTSTRAPPING = 3
    BOOTSTRAPPED = 4
    REGISTERING = 5
    REGISTERED = 6
    SUSPENDED = 7
    ENTERING_QUEUE_MODE = 8
    QUEUE_MODE = 9


# Error codes returned by mbedTLS, taken from mbedtls/ssl.h
MBEDTLS_ERR_SSL_INVALID_MAC = '-0x7180'
MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY = '-0x6c80'
MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE = '-0x7780'
MBEDTLS_ERR_X509_CERT_VERIFY_FAILED = '-0x2700'
MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY = '-0x7880'

Ts = TypeVarTuple("Ts")


"""
These helpers keep tests readable by hiding pytest's indirect parametrization
mechanics. The important bit:
  pytest.mark.parametrize("app", [...], indirect=True)
means the dict(s) are passed to the *fixture* named "app" via request.param.
"""

def app_config(config: dict | Iterable[dict]):
    # Sugar for:
    #   @pytest.mark.parametrize("app", [cfg1, cfg2, ...], indirect=True)
    #
    # Using indirect=True tells pytest: "don't pass cfg to the test function,
    # pass it to the 'app' fixture as request.param".
    configs = [config] if isinstance(config, dict) else list(config)
    return pytest.mark.parametrize("app", configs, indirect=True)


def parametrize_with_app_config(argnames: str | Sequence[str], argvalues: Iterable[Tuple[dict, Unpack[Ts]]]):
    # Same idea as app_config(), but for cases where each config comes with extra
    # parameters. Example:
    # @utils.parametrize_with_app_config(
    # "success_count",
    # [
    #     ({}, 3),
    #     ({'ANJ_DM_MAX_OBJECTS_NUMBER': 1}, 1),
    #     ({'ANJ_DM_MAX_OBJECTS_NUMBER': 2}, 2),
    # ]
    # )
    #
    # We still indirect only the "app" argument so the other args are delivered
    # normally to the test function.
    if isinstance(argnames, str):
        final_argnames = f"app, {argnames}"
    else:
        final_argnames = ["app"] + list(argnames)

    argvalues = [(config, *args) for config, *args in argvalues]
    return pytest.mark.parametrize(final_argnames, argvalues, indirect=["app"])


def assert_msg_equal(expected, actual, msg=None):
    """
    Assert that ACTUAL Lwm2mMsg object matches EXPECTED one.

    ACTUAL and EXPECTED may have their MSG_ID, TOKEN, OPTIONS or CONTENT
    fields set to ANY, in which case the value will not be checked.
    """
    mismatches = []

    def check(condition, description, exp_value=None, act_value=None):
        if not condition:
            if exp_value is not None or act_value is not None:
                mismatches.append(
                    f'{description}: expected {exp_value!r}, got {act_value!r}'
                )
            else:
                mismatches.append(description)

    if actual.version is not None:
        check(
            expected.version == actual.version,
            'unexpected CoAP version',
            expected.version,
            actual.version,
        )

    if actual.type is not None:
        check(
            expected.type == actual.type,
            'unexpected CoAP type',
            expected.type,
            actual.type,
        )

    check(
        expected.code == actual.code,
        'unexpected CoAP code',
        expected.code,
        actual.code,
    )

    if (
        expected.msg_id is not ANY
        and actual.msg_id is not ANY
        and actual.msg_id is not None
    ):
        check(
            expected.msg_id == actual.msg_id,
            'unexpected CoAP message ID',
            expected.msg_id,
            actual.msg_id,
        )

    if expected.token is not ANY and actual.token is not ANY:
        check(
            expected.token == actual.token,
            'unexpected CoAP token',
            expected.token,
            actual.token,
        )

    if expected.options is not ANY and actual.options is not ANY:
        check(
            expected.options == actual.options,
            'unexpected CoAP option list',
            expected.options,
            actual.options,
        )

    if expected.content is not ANY and actual.content is not ANY:
        check(
            expected.content == actual.content,
            'unexpected CoAP content',
            expected.content,
            actual.content,
        )

    if mismatches:
        prefix = f'{msg}: ' if msg else ''
        raise AssertionError(
            f'{prefix}LwM2mMsg mismatch:\n'
            + '\n'.join(f'  - {m}' for m in mismatches)
            + f'\n\n*** Expected ***\n{expected}'
            + f'\n*** Actual ***\n{actual}\n'
        )


def assert_received_after(server, expected_delay_s, tolerance_s=1.0):
    """
    Assert that a message is received from SERVER after approximately expected_delay_s seconds.
    Tolerance_s is the acceptable deviation from expected_delay_s. The function will wait for
    expected_delay_s + tolerance_s seconds before giving up and raising an AssertionError.
    """

    start = time.monotonic()
    timeout_s = expected_delay_s + tolerance_s

    try:
        pkt = server.recv(timeout_s=timeout_s)
    except TimeoutError as exc:
        raise AssertionError(
            f"no message received within {timeout_s:.3f}s, "
            f"expected one after {expected_delay_s:.3f}s ± {tolerance_s:.3f}s"
        ) from exc

    elapsed = time.monotonic() - start
    assert expected_delay_s - tolerance_s <= elapsed <= expected_delay_s + tolerance_s, \
        f"message arrived after {elapsed:.3f}s, expected {expected_delay_s:.3f}s ± {tolerance_s:.3f}s"
    return pkt


# extracts the error code from a RuntimeError raised by the test server for example:
# "RuntimeError: mbedtls_ssl_handshake failed: SSL - Verification of the message MAC failed (-0x7180)"
def error_code_from_runtime_error(error):
    match = re.search(r'\((-0x[0-9a-fA-F]+)\)$', str(error))
    assert match is not None
    return match.group(1).lower()


def expect_dtls_handshake_rejected(server, expected_error_code):
    # server.recv() performs the DTLS handshake before returning a CoAP packet.
    # If the handshake is rejected, it should raise instead.
    with pytest.raises(RuntimeError) as exc_info:
        server.recv()
    # check that the error code in the exception matches the expected one
    assert (error_code_from_runtime_error(exc_info.value)
            == expected_error_code.lower())

def expect_dtls_close_notify(server):
    with pytest.raises(RuntimeError) as exc_info:
        server.recv()
    # before closing the DTLS connection, the client must send a close_notify
    # alert, which causes the server's recv() to raise an exception with the
    # corresponding error code
    assert (error_code_from_runtime_error(exc_info.value)
            == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)

def _assert_dtls_client_hello(datagram):
    DTLS_CONTENT_TYPE_HANDSHAKE = 22
    DTLS_RECORD_HEADER_LENGTH = 13
    DTLS_HANDSHAKE_TYPE_CLIENT_HELLO = 1
    # DTLS record header is 13 bytes long. The first byte identifies the
    # record content type, and the first byte after the record header is the
    # handshake message type.
    assert len(datagram) > DTLS_RECORD_HEADER_LENGTH
    assert datagram[0] == DTLS_CONTENT_TYPE_HANDSHAKE
    assert (datagram[DTLS_RECORD_HEADER_LENGTH]
            == DTLS_HANDSHAKE_TYPE_CLIENT_HELLO)


def expect_and_drop_dtls_client_hello(server):
    # Read from the raw UDP socket instead of going through TlsServer.recv().
    # This consumes the datagram before the test DTLS server can process it,
    # which simulates packet loss during the DTLS handshake.
    datagram = server._raw_udp_socket.recv(4096)
    _assert_dtls_client_hello(datagram)
