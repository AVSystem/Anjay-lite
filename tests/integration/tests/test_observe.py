# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import cbor2

from framework_tools.lwm2m.server import Lwm2mServer, coap
import framework_tools.lwm2m.messages as msgs
import pytest
import time

import utils


ENDPOINT = "test-endpoint"

DEVICE_OBJECT_PATH = "/3"
DEVICE_INSTANCE_PATH = "/3/0"
MANUFACTURER_PATH = "/3/0/0"
SERVER_LIFETIME_PATH = "/1/0/1"
TEST_VALUE_PATH = "/1234/0/0"
TEST_LABEL_PATH = "/1234/0/1"


def _init_and_accept_register(app, server, udp_tx_params=None):
    config = {
        "endpoint": ENDPOINT,
        "servers": [{
            "uri": f"coap://127.0.0.1:{server.get_listen_port()}",
            "security": {"kind": "nosec"},
        }],
    }
    if udp_tx_params is not None:
        config["udp_tx_params"] = udp_tx_params
    assert app.rpc.call("init", config) == 0

    register = server.recv()
    assert isinstance(register, msgs.Lwm2mRegister)
    server.send(msgs.Lwm2mCreated.matching(register)(location="/rd/1"))


def _observe(server, path):
    request = msgs.Lwm2mObserve(path)
    server.send(request)

    response = server.recv()
    utils.assert_msg_equal(
        msgs.Lwm2mContent.matching(request)(),
        response,
    )

    assert len(response.get_options(coap.Option.OBSERVE)) == 1
    return request


def _cancel_observation(server, observation):
    cancel = msgs.Lwm2mObserve(
        observation.get_uri_path(),
        observe=1,
        token=observation.token,
    )
    server.send(cancel)

    response = server.recv()
    utils.assert_msg_equal(
        msgs.Lwm2mContent.matching(cancel)(),
        response,
    )

    assert not response.get_options(coap.Option.OBSERVE)


def _receive_notifications(server, observations, confirmable=False):
    expected_tokens = {
        observation.token
        for observation in observations
    }
    received_tokens = set()

    for _ in observations:
        notification = server.recv()

        assert notification.token in expected_tokens
        assert notification.token not in received_tokens

        utils.assert_msg_equal(
            msgs.Lwm2mNotify(
                token=notification.token,
                confirmable=confirmable,
            ),
            notification,
        )
        received_tokens.add(notification.token)

        if confirmable:
            server.send(
                msgs.Lwm2mEmpty.matching(notification)()
            )

    assert received_tokens == expected_tokens


def _write_text_resource(server, path, value):
    write = msgs.Lwm2mWrite(
        path,
        format=coap.ContentFormat.TEXT_PLAIN,
        content=str(value).encode(),
    )
    server.send(write)

    utils.assert_msg_equal(
        msgs.Lwm2mChanged.matching(write)(),
        server.recv(),
    )


def _set_confirmable(server, path):
    write_attributes = msgs.Lwm2mWriteAttributes(
        path,
        query=["con=1"],
    )
    server.send(write_attributes)

    utils.assert_msg_equal(
        msgs.Lwm2mChanged.matching(write_attributes)(),
        server.recv(),
    )


def _write_attributes(server, path, **attributes):
    request = msgs.Lwm2mWriteAttributes(path, **attributes)
    server.send(request)
    utils.assert_msg_equal(
        msgs.Lwm2mChanged.matching(request)(), server.recv())


def _remove_attributes(server, path, *names):
    request = msgs.Lwm2mWriteAttributes(path, query=list(names))
    server.send(request)
    utils.assert_msg_equal(
        msgs.Lwm2mChanged.matching(request)(), server.recv())


def _set_value(app, value, iid=0):
    assert app.rpc.call("set_test_value", iid, value) == 0


def _expect_notify(server, observation, timeout_s=2, confirmable=False):
    notification = server.recv(timeout_s=timeout_s)
    utils.assert_msg_equal(
        msgs.Lwm2mNotify(
            token=observation.token, confirmable=confirmable),
        notification)
    if confirmable:
        server.send(msgs.Lwm2mEmpty.matching(notification)())
    return notification


def _receive_block2_payload(server, first_packet, path):
    payload = bytearray(first_packet.content)
    block2_options = first_packet.get_options(coap.Option.BLOCK2)
    assert len(block2_options) == 1
    block2 = block2_options[0]
    assert block2.seq_num() == 0
    assert block2.has_more()

    content_format = first_packet.get_content_format()
    while block2.has_more():
        request = msgs.Lwm2mRead(
            path,
            token=first_packet.token,
            accept=content_format,
            options=[coap.Option.BLOCK2(
                seq_num=block2.seq_num() + 1,
                has_more=False,
                block_size=block2.block_size(),
            )],
        )
        server.send(request)
        packet = server.recv()

        block2_options = packet.get_options(coap.Option.BLOCK2)
        assert len(block2_options) == 1
        next_block2 = block2_options[0]
        assert next_block2.seq_num() == block2.seq_num() + 1
        assert next_block2.block_size() == block2.block_size()
        assert packet.token == first_packet.token

        payload.extend(packet.content)
        block2 = next_block2

    return bytes(payload)


def _contains_cbor_value(value, expected):
    if isinstance(value, dict):
        return any(_contains_cbor_value(item, expected)
                   for item in value.values())
    if isinstance(value, list):
        return any(_contains_cbor_value(item, expected) for item in value)
    return value == expected


def test_observations_at_different_levels_can_be_cancelled(app_spawner):
    """
    Object, Instance and Resource observations independently report a Resource
    change and stop reporting after being cancelled.
    """
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        object_observation = _observe(server, DEVICE_OBJECT_PATH)
        instance_observation = _observe(server, DEVICE_INSTANCE_PATH)
        resource_observation = _observe(server, MANUFACTURER_PATH)

        assert app.rpc.call("set_manufacturer", "Manufacturer 1") == 0
        _receive_notifications(
            server,
            [
                object_observation,
                instance_observation,
                resource_observation,
            ],
        )

        _cancel_observation(server, instance_observation)

        assert app.rpc.call("set_manufacturer", "Manufacturer 2") == 0
        _receive_notifications(
            server,
            [
                object_observation,
                resource_observation,
            ],
        )

        _cancel_observation(server, object_observation)

        assert app.rpc.call("set_manufacturer", "Manufacturer 3") == 0
        _receive_notifications(
            server,
            [resource_observation],
        )

        _cancel_observation(server, resource_observation)

        assert app.rpc.call("set_manufacturer", "Manufacturer 4") == 0
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.2)


def test_pmin_and_pmax_control_notification_periods(app_spawner):
    """pmin delays a value-change notification and pmax reports unchanged data."""
    server = Lwm2mServer()
    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)
        _write_attributes(server, TEST_VALUE_PATH, pmin=1)
        _write_attributes(server, TEST_LABEL_PATH, pmax=2)
        value_observe = _observe(server, TEST_VALUE_PATH)
        label_observe = _observe(server, TEST_LABEL_PATH)

        started = time.monotonic()
        _set_value(app, 10)
        _expect_notify(server, value_observe, timeout_s=1.5)
        assert 0.7 <= time.monotonic() - started <= 1.5

        _expect_notify(server, label_observe, timeout_s=1.5)
        assert 1.7 <= time.monotonic() - started <= 2.5


def test_default_pmin_pmax_used(
        app_spawner):
    """Server Default Minimum and Maximum Period replace removed attributes."""
    server = Lwm2mServer()
    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)
        # do not set pmin and pmax, so that default values are used
        # _write_attributes(server, TEST_VALUE_PATH, pmin=3)
        # _write_attributes(server, TEST_LABEL_PATH, pmax=4)

        # set default pmin and pmax to 1 and 2 seconds respectively
        for path, value in (("/1/0/2", 1), ("/1/0/3", 2)):
            write = msgs.Lwm2mWrite(
                path, format=coap.ContentFormat.TEXT_PLAIN,
                content=str(value).encode())
            server.send(write)
            utils.assert_msg_equal(
                msgs.Lwm2mChanged.matching(write)(), server.recv())

        value_observe = _observe(server, TEST_VALUE_PATH)
        label_observe = _observe(server, TEST_LABEL_PATH)
        started = time.monotonic()
        _set_value(app, 11)
        _expect_notify(server, value_observe, timeout_s=1.5)
        assert 0.7 <= time.monotonic() - started <= 1.5
        _expect_notify(server, label_observe, timeout_s=1.5)
        assert 1.7 <= time.monotonic() - started <= 2.5


def test_numeric_threshold_attributes(app_spawner):
    """gt, lt and st notify only after their numeric conditions are met."""
    server = Lwm2mServer()
    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        _set_value(app, 0)
        _write_attributes(server, TEST_VALUE_PATH, gt=5)
        observation = _observe(server, TEST_VALUE_PATH)
        _set_value(app, 4)
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.2)
        _set_value(app, 6)
        _expect_notify(server, observation)
        _cancel_observation(server, observation)

        _remove_attributes(server, TEST_VALUE_PATH, "gt")
        _set_value(app, 10)
        _write_attributes(server, TEST_VALUE_PATH, lt=5)
        observation = _observe(server, TEST_VALUE_PATH)
        _set_value(app, 6)
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.2)
        _set_value(app, 4)
        _expect_notify(server, observation)
        _cancel_observation(server, observation)

        _remove_attributes(server, TEST_VALUE_PATH, "lt")
        _set_value(app, 0)
        _write_attributes(server, TEST_VALUE_PATH, st=2)
        observation = _observe(server, TEST_VALUE_PATH)
        _set_value(app, 1)
        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.2)
        _set_value(app, 2)
        _expect_notify(server, observation)


def test_invalid_edge_periods_are_rejected(app_spawner):
    """epmin greater than epmax is rejected with 4.00 Bad Request."""
    server = Lwm2mServer()
    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)
        request = msgs.Lwm2mWriteAttributes(
            TEST_VALUE_PATH, epmin=5, epmax=1)
        server.send(request)
        utils.assert_msg_equal(
            msgs.Lwm2mErrorResponse.matching(request)(
                code=coap.Code.RES_BAD_REQUEST),
            server.recv())

@utils.app_config({"ANJ_WITH_OBSERVE_COMPOSITE": "ON"})
def test_composite_observation_of_resources_from_different_objects(
        app_spawner):
    """
    A Composite Observation reports a change to either of its Resources, even
    when the Resources belong to different Objects.
    """
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        observe = msgs.Lwm2mObserveComposite(
            paths=[
                MANUFACTURER_PATH,
                SERVER_LIFETIME_PATH,
            ]
        )
        server.send(observe)

        response = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mContent.matching(observe)(),
            response,
        )
        assert len(response.get_options(coap.Option.OBSERVE)) == 1

        assert app.rpc.call(
            "set_manufacturer", "Composite Manufacturer"
        ) == 0

        notification = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mNotify(token=observe.token),
            notification,
        )


@utils.app_config({"ANJ_OUT_PAYLOAD_BUFFER_SIZE": 128})
def test_notification_with_block_transfer(app_spawner):
    """A notification larger than the output buffer is transferred using Block2."""
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)
        observation = _observe(server, MANUFACTURER_PATH)

        manufacturer = "M" * 128
        assert app.rpc.call("set_manufacturer", manufacturer) == 0

        notification = server.recv()
        block2_options = notification.get_options(coap.Option.BLOCK2)
        assert len(block2_options) == 1
        utils.assert_msg_equal(
            msgs.Lwm2mNotify(token=observation.token),
            notification,
        )
        payload = _receive_block2_payload(
            server, notification, MANUFACTURER_PATH)
        assert len(payload) > 128
        assert _contains_cbor_value(cbor2.loads(payload), manufacturer)


def test_default_notification_mode_makes_notifications_confirmable(
        app_spawner):
    """
    Default Notification Mode set to 1 makes Object, Instance and Resource
    notifications Confirmable.
    """
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        _write_text_resource(server, "/1/0/26", 1)

        observations = [
            _observe(server, DEVICE_OBJECT_PATH),
            _observe(server, DEVICE_INSTANCE_PATH),
            _observe(server, MANUFACTURER_PATH),
        ]

        assert app.rpc.call(
            "set_manufacturer", "Confirmable Manufacturer"
        ) == 0

        _receive_notifications(
            server,
            observations,
            confirmable=True,
        )


def test_deleting_instance_removes_its_observation(app_spawner):
    """Deleting one Instance removes only observations below that Instance."""
    server = Lwm2mServer()
    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)
        object_observe = _observe(server, "/1234")
        _observe(server, "/1234/1/0")
        remaining_observe = _observe(server, "/1234/2/0")

        delete = msgs.Lwm2mDelete("/1234/1")
        server.send(delete)
        utils.assert_msg_equal(
            msgs.Lwm2mDeleted.matching(delete)(), server.recv())

        _set_value(app, 20, iid=2)
        _receive_notifications(
            server,
            [object_observe, remaining_observe],
        )

        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.2)


def test_observe_cancel_and_rst_remove_only_selected_observations(
        app_spawner):
    """
    Observe Cancel and a CoAP RST independently remove two observations, while
    the remaining observation continues reporting changes.
    """
    server = Lwm2mServer()

    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server)

        object_observation = _observe(server, DEVICE_OBJECT_PATH)
        instance_observation = _observe(server, DEVICE_INSTANCE_PATH)

        _set_confirmable(server, MANUFACTURER_PATH)
        resource_observation = _observe(server, MANUFACTURER_PATH)

        assert app.rpc.call("set_manufacturer", "Before cancellation") == 0

        received_tokens = set()
        for _ in range(3):
            notification = server.recv()
            received_tokens.add(notification.token)

            if notification.token == resource_observation.token:
                utils.assert_msg_equal(
                    msgs.Lwm2mNotify(
                        token=resource_observation.token,
                        confirmable=True,
                    ),
                    notification,
                )
                server.send(
                    msgs.Lwm2mEmpty.matching(notification)()
                )

        assert received_tokens == {
            object_observation.token,
            instance_observation.token,
            resource_observation.token,
        }

        _cancel_observation(server, object_observation)

        assert app.rpc.call("set_manufacturer", "RST cancellation") == 0

        instance_notification = None
        resource_notification = None

        for _ in range(2):
            notification = server.recv()

            if notification.token == instance_observation.token:
                instance_notification = notification
            elif notification.token == resource_observation.token:
                resource_notification = notification

        assert instance_notification is not None
        assert resource_notification is not None

        utils.assert_msg_equal(
            msgs.Lwm2mNotify(
                token=resource_observation.token,
                confirmable=True,
            ),
            resource_notification,
        )
        server.send(
            msgs.Lwm2mReset.matching(resource_notification)()
        )

        assert app.rpc.call("set_manufacturer", "Only one remains") == 0

        notification = server.recv()
        utils.assert_msg_equal(
            msgs.Lwm2mNotify(token=instance_observation.token),
            notification,
        )

        with pytest.raises(TimeoutError):
            server.recv(timeout_s=0.2)


@utils.parametrize_with_app_config(
    "cancel_on_timeout",
    [
        ({"ANJ_OBSERVE_OBSERVATION_CANCEL_ON_TIMEOUT": "ON"}, True),
        ({"ANJ_OBSERVE_OBSERVATION_CANCEL_ON_TIMEOUT": "OFF"}, False),
    ],
)
def test_confirmable_notification_timeout_behavior(
        app_spawner, cancel_on_timeout):
    """A timed-out CON notification removes its observation only when enabled."""
    server = Lwm2mServer()
    with app_spawner.spawn_app() as app:
        _init_and_accept_register(app, server, udp_tx_params={
            "ack_timeout_s": 1,
            "ack_random_factor": 1.01,
            "max_retransmit": 0,
        })
        _write_attributes(server, TEST_VALUE_PATH, query=["con=1"])
        observation = _observe(server, TEST_VALUE_PATH)
        _set_value(app, 1)
        _expect_notify(
            server, observation, confirmable=True, timeout_s=2)

        # The helper ACKed it; trigger another CON and deliberately leave it
        # unacknowledged.
        _set_value(app, 2)
        notification = server.recv(timeout_s=2)
        utils.assert_msg_equal(
            msgs.Lwm2mNotify(
                token=observation.token, confirmable=True),
            notification)
        time.sleep(1.5)

        _set_value(app, 3)
        if cancel_on_timeout:
            with pytest.raises(TimeoutError):
                server.recv(timeout_s=0.3)
        else:
            _expect_notify(
                server, observation, confirmable=True, timeout_s=2)
