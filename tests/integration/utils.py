# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import time

from typing import Iterable, Sequence, Tuple
from typing_extensions import Unpack, TypeVarTuple
from framework_tools.lwm2m.messages import ANY
import pytest

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
