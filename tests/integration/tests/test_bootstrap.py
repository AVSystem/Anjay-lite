# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import time

import certs

from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.transport import Transport
from framework_tools.lwm2m.coap.content_format import ContentFormat
from framework_tools.lwm2m.senml_cbor import CBOR, SenmlLabel
import framework_tools.lwm2m.messages as msgs

import utils


ENDPOINT = "test-endpoint"
PSK_IDENTITY_BS = "bs-test-psk-identity"
PSK_KEY_BS = "bs-test-psk-key"
PSK_IDENTITY = "test-psk-identity"
PSK_KEY = "test-psk-key"
LIFETIME = 100
REGULAR_SSID = 1

SECURITY_MODE_NOSEC = 3
SECURITY_MODE_CERTIFICATE = 2
SECURITY_MODE_PSK = 0

def _make_nosec_server():
    return Lwm2mServer(coap.Server(transport=Transport.UDP))


def _server_uri(server):
    return f"coap://127.0.0.1:{server.get_listen_port()}"


def _init_app_with_bootstrap_nosec_server(app,
                                          bootstrap_server,
                                          endpoint=ENDPOINT,
                                          bootstrap_config=None,
                                          udp_tx_params=None):
    config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": _server_uri(bootstrap_server),
            "security": {
                "kind": "nosec"
            },
            "bootstrap": True
        }]
    }
    if bootstrap_config is not None:
        config["bootstrap_config"] = bootstrap_config
    if udp_tx_params is not None:
        config["udp_tx_params"] = udp_tx_params

    assert app.rpc.call("init", config) == 0


def _handle_bootstrap_request(server,
                              endpoint=ENDPOINT,
                              send_response=True):
    expected = msgs.Lwm2mRequestBootstrap(
        endpoint_name=endpoint,
        preferred_content_format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR)
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    if send_response:
        server.send(msgs.Lwm2mChanged.matching(pkt)())
    return pkt


def _send_bootstrap_write_and_expect_changed(server, path, payload):
    write = msgs.Lwm2mWrite(
        path,
        format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
        content=payload)
    server.send(write)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), pkt)
    return pkt


def _send_bootstrap_write_and_expect_bad_request(server, path, payload):
    write = msgs.Lwm2mWrite(
        path,
        format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
        content=payload)
    server.send(write)
    pkt = server.recv()
    utils.assert_msg_equal(
        msgs.Lwm2mErrorResponse.matching(write)(
            code=coap.Code.RES_BAD_REQUEST),
        pkt)
    return pkt


def _provision_regular_nosec_server_via_bootstrap(bootstrap_server,
                                                  regular_server,
                                                  security_iid=1,
                                                  server_iid=0,
                                                  ssid=REGULAR_SSID,
                                                  lifetime=LIFETIME):
    security_payload = CBOR.serialize([
        {SenmlLabel.NAME: f"/0/{security_iid}/0",
         SenmlLabel.STRING: _server_uri(regular_server)},
        {SenmlLabel.NAME: f"/0/{security_iid}/1", SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: f"/0/{security_iid}/2", SenmlLabel.VALUE: SECURITY_MODE_NOSEC},
        {SenmlLabel.NAME: f"/0/{security_iid}/10", SenmlLabel.VALUE: ssid}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, f"/0/{security_iid}", security_payload)

    server_payload = CBOR.serialize([
        {SenmlLabel.NAME: f"/1/{server_iid}/0", SenmlLabel.VALUE: ssid},
        {SenmlLabel.NAME: f"/1/{server_iid}/1", SenmlLabel.VALUE: lifetime},
        {SenmlLabel.NAME: f"/1/{server_iid}/6", SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: f"/1/{server_iid}/7", SenmlLabel.STRING: "U"}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, f"/1/{server_iid}", server_payload)


def _provision_bootstrap_nosec_server_via_bootstrap(bootstrap_server,
                                                    new_bootstrap_server,
                                                    security_iid=0):
    security_payload = CBOR.serialize([
        {SenmlLabel.NAME: f"/0/{security_iid}/0",
         SenmlLabel.STRING: _server_uri(new_bootstrap_server)},
        {SenmlLabel.NAME: f"/0/{security_iid}/1", SenmlLabel.BOOL: True},
        {SenmlLabel.NAME: f"/0/{security_iid}/2", SenmlLabel.VALUE: SECURITY_MODE_NOSEC}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, f"/0/{security_iid}", security_payload)


def _finish_bootstrap(server, expected_code=coap.Code.RES_CHANGED):
    finish = msgs.Lwm2mBootstrapFinish()
    server.send(finish)
    pkt = server.recv()

    if expected_code == coap.Code.RES_CHANGED:
        expected = msgs.Lwm2mChanged.matching(finish)()
    else:
        expected = msgs.Lwm2mErrorResponse.matching(finish)(
            code=expected_code)

    utils.assert_msg_equal(expected, pkt)
    return pkt


def _handle_register(server,
                     endpoint=ENDPOINT,
                     lifetime=LIFETIME,
                     accept_register=True,
                     timeout_s=None):
    expected = msgs.Lwm2mRegister(
        f"/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U")
    pkt = server.recv(
        timeout_s=timeout_s) if timeout_s is not None else server.recv()
    utils.assert_msg_equal(expected, pkt)
    assert pkt.content is not None
    if accept_register:
        server.send(
            msgs.Lwm2mCreated.matching(pkt)(
                location=f"/rd/{endpoint}"))
    return pkt


def _send_confirmable_separate_response(server, request, response):
    server.send(msgs.Lwm2mEmpty.matching(request)())

    response.type = coap.Type.CONFIRMABLE
    response.msg_id = (request.msg_id + 1) % (2 ** 16)
    server.send(response)

    utils.assert_msg_equal(
        msgs.Lwm2mEmpty.matching(response)(), server.recv())


def _handle_deregister(server, endpoint=ENDPOINT):
    expected = msgs.Lwm2mDeregister(f"/rd/{endpoint}")
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    server.send(msgs.Lwm2mDeleted.matching(pkt)())
    return pkt


def _trigger_bootstrap_request(server):
    trigger = msgs.Lwm2mExecute("/1/0/9")
    server.send(trigger)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(trigger)(), pkt)
    return pkt


def _complete_bootstrap_and_register_to_nosec_server(bootstrap_server, regular_server):
    _provision_regular_nosec_server_via_bootstrap(
        bootstrap_server, regular_server)
    _finish_bootstrap(bootstrap_server)
    _handle_register(regular_server)


def test_bootstrap_nosec_with_nosec_registration(app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(app, bootstrap_server)

        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server, regular_server)


def test_bootstrap_and_registration_with_confirmable_separate_responses(
        app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(app, bootstrap_server)

        bootstrap_request = _handle_bootstrap_request(
            bootstrap_server, send_response=False)
        _send_confirmable_separate_response(
            bootstrap_server,
            bootstrap_request,
            msgs.Lwm2mChanged.matching(bootstrap_request)())

        _provision_regular_nosec_server_via_bootstrap(
            bootstrap_server, regular_server)
        _finish_bootstrap(bootstrap_server)

        register = _handle_register(regular_server, accept_register=False)
        _send_confirmable_separate_response(
            regular_server,
            register,
            msgs.Lwm2mCreated.matching(register)(
                location=f"/rd/{ENDPOINT}"))


def test_bootstrap_and_registration_recover_from_temporary_packet_loss(
        app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(
            app,
            bootstrap_server,
            udp_tx_params={
                "ack_timeout_s": 1,
                "ack_random_factor": 1.01,
                "max_retransmit": 2,
            })

        first_bootstrap_request = _handle_bootstrap_request(
            bootstrap_server, send_response=False)
        bootstrap_retransmission = bootstrap_server.recv(timeout_s=2)
        utils.assert_msg_equal(
            first_bootstrap_request, bootstrap_retransmission)
        bootstrap_server.send(
            msgs.Lwm2mChanged.matching(bootstrap_retransmission)())

        _provision_regular_nosec_server_via_bootstrap(
            bootstrap_server, regular_server)
        _finish_bootstrap(bootstrap_server)

        first_register = _handle_register(
            regular_server, accept_register=False)
        register_retransmission = regular_server.recv(timeout_s=2)
        utils.assert_msg_equal(first_register, register_retransmission)
        regular_server.send(
            msgs.Lwm2mCreated.matching(register_retransmission)(
                location=f"/rd/{ENDPOINT}"))


def _send_bootstrap_write_text_and_expect_changed(server, path, content):
    write = msgs.Lwm2mWrite(
        path,
        format=ContentFormat.TEXT_PLAIN,
        content=content
    )
    server.send(write)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), pkt)
    return pkt


def test_bootstrap_with_failed_write_that_does_not_abort_whole_operation(
        app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(app, bootstrap_server)

        _handle_bootstrap_request(bootstrap_server)

        # A malformed Bootstrap Write shall fail, but it must not abort the
        # whole Bootstrap transaction. Valid Writes and Bootstrap Finish that
        # follow shall still succeed.
        _send_bootstrap_write_and_expect_bad_request(
            bootstrap_server, "/1/0", b"not-a-valid-senml-cbor-payload")

        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server, regular_server)


def test_bootstrap_without_finish_times_out_and_failure(app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(
            app,
            bootstrap_server,
            bootstrap_config={
                "retry_count": 0,
                "retry_timeout_s": 2,
                "bootstrap_timeout_s": 2
            })

        _handle_bootstrap_request(bootstrap_server)
        _provision_regular_nosec_server_via_bootstrap(
            bootstrap_server, regular_server)

        # Do not send Bootstrap Finish. The client shall abort this Bootstrap
        # attempt after bootstrap_timeout_s and goes to failure state.
        time.sleep(3)
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.FAILURE


def test_bootstrap_without_finish_times_out_and_retry_succeeds(app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(
            app,
            bootstrap_server,
            bootstrap_config={
                "retry_count": 1,
                "retry_timeout_s": 2,
                "bootstrap_timeout_s": 2
            })

        _handle_bootstrap_request(bootstrap_server)
        _provision_regular_nosec_server_via_bootstrap(
            bootstrap_server, regular_server)

        # Do not send Bootstrap Finish. The client shall abort this Bootstrap
        # attempt after bootstrap_timeout and then retry:
        #  - after 2 seconds (bootstrap_timeout_s) connection is closed
        #  - after 2 more seconds (retry_timeout_s) the client retries the Bootstrap
        time.sleep(3)
        bootstrap_server.reset()
        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server, regular_server)


def test_bootstrap_aborted_by_disable_server_and_resume_succeeds(app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(
            app,
            bootstrap_server,
            bootstrap_config={
                "retry_count": 1,
                "retry_timeout_s": 1
            })

        _handle_bootstrap_request(bootstrap_server)
        _provision_regular_nosec_server_via_bootstrap(
            bootstrap_server, regular_server)

        # Call anj_core_disable_server(). Calling it during
        # Bootstrap aborts the ongoing operation. After the disable timeout
        # expires, the client shall resume and Bootstrap shall succeed.
        assert app.rpc.call("disable_server", 2) == 0

        time.sleep(1)
        bootstrap_server.reset()
        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server, regular_server)


def test_bootstrap_do_nothing_check_bootstrap_finish(
        app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(
            app,
            bootstrap_server,
            bootstrap_config={
                "retry_count": 1,
                "retry_timeout_s": 2,
                "bootstrap_timeout_s": 2
            })

        _handle_bootstrap_request(bootstrap_server)
        _finish_bootstrap(
            bootstrap_server,
            expected_code=coap.Code.RES_NOT_ACCEPTABLE)

        time.sleep(1)
        bootstrap_server.reset()
        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server, regular_server)


def test_bootstrap_modifies_own_instance_and_next_bootstrap_uses_new_server(
        app_spawner):
    bootstrap_server_1 = _make_nosec_server()
    bootstrap_server_2 = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(app, bootstrap_server_1)

        _handle_bootstrap_request(bootstrap_server_1)
        _provision_bootstrap_nosec_server_via_bootstrap(
            bootstrap_server_1, bootstrap_server_2)
        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server_1, regular_server)

        _trigger_bootstrap_request(regular_server)
        _handle_deregister(regular_server)
        regular_server.reset()

        # The previous Bootstrap operation changed the Bootstrap Server
        # instance URI. The next Bootstrap attempt shall go to the new server.
        _handle_bootstrap_request(bootstrap_server_2)
        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server_2, regular_server)


def test_bootstrap_overwrites_only_bootstrap_server_uri_then_finish_fails_and_retry_succeeds(
        app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(
            app,
            bootstrap_server,
            bootstrap_config={
                "retry_count": 1,
                "retry_timeout_s": 2,
                "bootstrap_timeout_s": 2
            })
        _handle_bootstrap_request(bootstrap_server)
        _send_bootstrap_write_text_and_expect_changed(
            bootstrap_server, "/0/0/0", _server_uri(regular_server).encode())
        _finish_bootstrap(
            bootstrap_server,
            expected_code=coap.Code.RES_NOT_ACCEPTABLE)

        time.sleep(1)
        bootstrap_server.reset()
        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server, regular_server)


def test_bootstrap_timeout_rolls_back_dm_to_previous_successful_bootstrap_state(
        app_spawner):
    bootstrap_server = _make_nosec_server()
    management_server_a = _make_nosec_server()
    management_server_b = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(
            app,
            bootstrap_server,
            bootstrap_config={
                "retry_count": 1,
                "retry_timeout_s": 2,
                "bootstrap_timeout_s": 2
            })

        # First Bootstrap succeeds and provisions Management Server A.
        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_nosec_server(
            bootstrap_server,
            management_server_a)

        # Prepare the Bootstrap Server before triggering Bootstrap.
        bootstrap_server.reset()

        # Trigger the second Bootstrap from Management Server A.
        _trigger_bootstrap_request(management_server_a)
        _handle_deregister(management_server_a)
        management_server_a.reset()

        _handle_bootstrap_request(bootstrap_server)

        # The second Bootstrap modifies the Data Model:
        # - regular Security Object instance now points to Management Server B,
        # - Server Object instance for Management Server A is deleted.
        #
        # Bootstrap Finish is intentionally not sent. The Bootstrap timeout
        # shall roll back both changes.
        _send_bootstrap_write_text_and_expect_changed(
            bootstrap_server,
            "/0/1/0",
            _server_uri(management_server_b).encode())

        _send_bootstrap_delete_and_expect_deleted(
            bootstrap_server,
            "/1/0")

        # Wait for Bootstrap timeout and retry scheduling. The exact retry
        # timing is handled by the blocking Bootstrap-Request receive below.
        time.sleep(3)
        bootstrap_server.reset()

        # The retried Bootstrap succeeds without writing anything. This is valid
        # only if the previous timed-out Bootstrap attempt was rolled back and
        # Management Server A is still present in the Data Model.
        _handle_bootstrap_request(bootstrap_server)
        _finish_bootstrap(bootstrap_server)

        # The client shall register to Management Server A, not B.
        _handle_register(management_server_a)


#################### Certificate Bootstrap with external storage ##############


def _make_cert_server(credentials):
    return Lwm2mServer(coap.TlsServer(
        ca_file=str(credentials.dtls_server_client_ca),
        crt_file=str(credentials.dtls_server_cert_chain),
        key_file=str(credentials.dtls_server_private_key),
        transport=Transport.UDP
    ))


def _cert_server_uri(server):
    return f"coaps://127.0.0.1:{server.get_listen_port()}"


def _init_app_with_bootstrap_cert_server(app,
                                         bootstrap_server,
                                         bootstrap_credentials,
                                         endpoint=ENDPOINT,
                                         bootstrap_config=None,
                                         udp_tx_params=None):
    config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": _cert_server_uri(bootstrap_server),
            "security": {
                "kind": "cert",
                "client_cert_path": str(
                    bootstrap_credentials.anjay_client_cert),
                "client_key_path": str(
                    bootstrap_credentials.anjay_client_private_key),
                "server_public_key_path": str(
                    bootstrap_credentials.anjay_server_public_key)
            },
            "bootstrap": True
        }]
    }
    if bootstrap_config is not None:
        config["bootstrap_config"] = bootstrap_config
    if udp_tx_params is not None:
        config["udp_tx_params"] = udp_tx_params

    assert app.rpc.call("init", config) == 0


def _make_regular_cert_credentials_for_bootstrap(certificate_environment):
    regular_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="regular",
        endpoint=ENDPOINT)

    # Bootstrap writes Security Object credential resources as opaque bytes.
    # Use DER here to keep the payload compact and avoid PEM-specific handling
    # in the data model write path.
    return certs.convert_credentials_to_der(
        certificate_environment,
        regular_credentials)


def _provision_regular_cert_server_via_bootstrap(bootstrap_server,
                                                 regular_server,
                                                 regular_credentials,
                                                 security_iid=1,
                                                 server_iid=0,
                                                 ssid=REGULAR_SSID,
                                                 lifetime=LIFETIME):
    security_payload = CBOR.serialize([
        {SenmlLabel.NAME: f"/0/{security_iid}/0",
         SenmlLabel.STRING: _cert_server_uri(regular_server)},
        {SenmlLabel.NAME: f"/0/{security_iid}/1", SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: f"/0/{security_iid}/2",
         SenmlLabel.VALUE: SECURITY_MODE_CERTIFICATE},
        {SenmlLabel.NAME: f"/0/{security_iid}/3",
         SenmlLabel.OPAQUE:
             regular_credentials.anjay_client_cert.read_bytes()},
        {SenmlLabel.NAME: f"/0/{security_iid}/4",
         SenmlLabel.OPAQUE:
             regular_credentials.anjay_server_public_key.read_bytes()},
        {SenmlLabel.NAME: f"/0/{security_iid}/5",
         SenmlLabel.OPAQUE:
             regular_credentials.anjay_client_private_key.read_bytes()},
        {SenmlLabel.NAME: f"/0/{security_iid}/10", SenmlLabel.VALUE: ssid}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, f"/0/{security_iid}", security_payload)

    server_payload = CBOR.serialize([
        {SenmlLabel.NAME: f"/1/{server_iid}/0", SenmlLabel.VALUE: ssid},
        {SenmlLabel.NAME: f"/1/{server_iid}/1", SenmlLabel.VALUE: lifetime},
        {SenmlLabel.NAME: f"/1/{server_iid}/6", SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: f"/1/{server_iid}/7", SenmlLabel.STRING: "U"}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, f"/1/{server_iid}", server_payload)


def _finish_bootstrap_and_expect_dtls_close(server):
    _finish_bootstrap(server)
    utils.expect_dtls_close_notify(server)


def _complete_bootstrap_to_cert_server(bootstrap_server,
                                       regular_server,
                                       regular_credentials):
    _provision_regular_cert_server_via_bootstrap(
        bootstrap_server,
        regular_server,
        regular_credentials)
    _finish_bootstrap_and_expect_dtls_close(bootstrap_server)
    _handle_register(regular_server)


def _send_bootstrap_delete_and_expect_deleted(server, path):
    delete = msgs.Lwm2mDelete(path)
    server.send(delete)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mDeleted.matching(delete)(), pkt)
    return pkt


@utils.app_config({
    "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE": "ON",
    "ANJ_WITH_CRYPTO_STORAGE_DEFAULT": "OFF"
})
def test_bootstrap_certificates_with_certificates_external_crypto_storage(
        app_spawner,
        certificate_environment):
    bootstrap_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="bootstrap",
        endpoint=ENDPOINT)
    regular_credentials = _make_regular_cert_credentials_for_bootstrap(
        certificate_environment)

    bootstrap_server = _make_cert_server(bootstrap_credentials)
    regular_server = _make_cert_server(regular_credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_cert_server(
            app,
            bootstrap_server,
            bootstrap_credentials)

        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_to_cert_server(
            bootstrap_server,
            regular_server,
            regular_credentials)


@utils.app_config({
    "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE": "ON",
    "ANJ_WITH_CRYPTO_STORAGE_DEFAULT": "OFF"
})
def test_bootstrap_certificates_external_crypto_storage_timeout_then_retry_succeeds(
        app_spawner,
        certificate_environment):
    bootstrap_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="bootstrap",
        endpoint=ENDPOINT)
    regular_credentials = _make_regular_cert_credentials_for_bootstrap(
        certificate_environment)

    bootstrap_server = _make_cert_server(bootstrap_credentials)
    regular_server = _make_cert_server(regular_credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_cert_server(
            app,
            bootstrap_server,
            bootstrap_credentials,
            bootstrap_config={
                "retry_count": 1,
                "retry_timeout_s": 2,
                "bootstrap_timeout_s": 2
            })

        _handle_bootstrap_request(bootstrap_server)
        _provision_regular_cert_server_via_bootstrap(
            bootstrap_server,
            regular_server,
            regular_credentials)

        # Do not send Bootstrap Finish. The client shall abort the ongoing
        # Bootstrap transaction, close the DTLS connection and retry from the
        # previous Bootstrap Server configuration.
        time.sleep(3)
        utils.expect_dtls_close_notify(bootstrap_server)
        bootstrap_server.reset()

        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_to_cert_server(
            bootstrap_server,
            regular_server,
            regular_credentials)


@utils.app_config({
    "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE": "ON",
    "ANJ_WITH_CRYPTO_STORAGE_DEFAULT": "OFF"
})
def test_bootstrap_certificates_external_crypto_storage_delete_security_and_server_timeout_restores_instances(
        app_spawner,
        certificate_environment):
    bootstrap_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="bootstrap",
        endpoint=ENDPOINT)
    regular_credentials = _make_regular_cert_credentials_for_bootstrap(
        certificate_environment)

    bootstrap_server = _make_cert_server(bootstrap_credentials)
    regular_server = _make_cert_server(regular_credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_cert_server(
            app,
            bootstrap_server,
            bootstrap_credentials,
            bootstrap_config={
                "retry_count": 0,
                "retry_timeout_s": 2,
                "bootstrap_timeout_s": 2
            })

        # First Bootstrap succeeds and provisions a regular certificate-based
        # LwM2M Server. Credentials are offloaded to external storage after
        # successful Bootstrap Finish.
        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_to_cert_server(
            bootstrap_server,
            regular_server,
            regular_credentials)

        # Prepare the Bootstrap Server before triggering Bootstrap.
        bootstrap_server.reset()

        # Trigger Bootstrap from the regular server.
        _trigger_bootstrap_request(regular_server)
        _handle_deregister(regular_server)
        utils.expect_dtls_close_notify(regular_server)
        regular_server.reset()

        _handle_bootstrap_request(bootstrap_server)

        # Delete Security and Server Objects, but do not finish Bootstrap.
        # After timeout, the Bootstrap transaction shall be rolled back, so the
        # previously committed Security and Server Object instances remain
        # usable and the client can register again.
        _send_bootstrap_delete_and_expect_deleted(bootstrap_server, "/0")
        _send_bootstrap_delete_and_expect_deleted(bootstrap_server, "/1")

        time.sleep(3)
        utils.expect_dtls_close_notify(bootstrap_server)

        # anjay goes to Failure state
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.FAILURE
        # restart the client to trigger a new registration attempt
        assert app.rpc.call("restart_client") == 0

        # Server and Security Object instances should hold the previous
        # values, so the client can register again without bootstrap.
        _handle_register(regular_server, timeout_s=6)


#################### Bootstrap with persistence ##############


def _make_psk_server(psk_identity=PSK_IDENTITY,
                     psk_key=PSK_KEY):
    return Lwm2mServer(coap.TlsServer(
        psk_identity=psk_identity,
        psk_key=psk_key,
        transport=Transport.UDP
    ))

def _psk_server_uri(server):
    return f"coaps://127.0.0.1:{server.get_listen_port()}"

def _init_app_with_bootstrap_psk_server(app,
                                          bootstrap_server,
                                          endpoint=ENDPOINT,
                                          bootstrap_config=None,
                                          udp_tx_params=None):
    config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": _psk_server_uri(bootstrap_server),
            "security": {
                "kind": "psk",
                "psk_identity": PSK_IDENTITY_BS,
                "psk_key": PSK_KEY_BS
            },
            "bootstrap": True
        }]
    }
    if bootstrap_config is not None:
        config["bootstrap_config"] = bootstrap_config
    if udp_tx_params is not None:
        config["udp_tx_params"] = udp_tx_params

    assert app.rpc.call("init", config) == 0

def _provision_regular_psk_server_via_bootstrap(bootstrap_server,
                                                  regular_server,
                                                  security_iid=1,
                                                  server_iid=0,
                                                  ssid=REGULAR_SSID,
                                                  lifetime=LIFETIME):
    security_payload = CBOR.serialize([
        {SenmlLabel.NAME: f"/0/{security_iid}/0",
         SenmlLabel.STRING: _psk_server_uri(regular_server)},
        {SenmlLabel.NAME: f"/0/{security_iid}/1", SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: f"/0/{security_iid}/2", SenmlLabel.VALUE: SECURITY_MODE_PSK},
        {SenmlLabel.NAME: f"/0/{security_iid}/3", SenmlLabel.OPAQUE: PSK_IDENTITY.encode()},
        {SenmlLabel.NAME: f"/0/{security_iid}/5", SenmlLabel.OPAQUE: PSK_KEY.encode()},
        {SenmlLabel.NAME: f"/0/{security_iid}/10", SenmlLabel.VALUE: ssid}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, f"/0/{security_iid}", security_payload)

    server_payload = CBOR.serialize([
        {SenmlLabel.NAME: f"/1/{server_iid}/0", SenmlLabel.VALUE: ssid},
        {SenmlLabel.NAME: f"/1/{server_iid}/1", SenmlLabel.VALUE: lifetime},
        {SenmlLabel.NAME: f"/1/{server_iid}/6", SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: f"/1/{server_iid}/7", SenmlLabel.STRING: "U"}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, f"/1/{server_iid}", server_payload)


def _complete_bootstrap_and_register_to_psk_server(bootstrap_server, regular_server):
    _provision_regular_psk_server_via_bootstrap(
        bootstrap_server, regular_server)
    _finish_bootstrap(bootstrap_server)
    _handle_register(regular_server)

def _handle_blockwise_register(server,
                               endpoint=ENDPOINT,
                               lifetime=LIFETIME,
                               queue_mode=False):
    queue_mode_query = "&Q" if queue_mode else ""
    path = (f"/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U"
            f"{queue_mode_query}")
    payload = bytearray()
    expected_seq_num = 0

    while True:
        pkt = server.recv()
        block1_options = pkt.get_options(coap.Option.BLOCK1)
        assert len(block1_options) == 1
        block1 = block1_options[0]
        assert block1.seq_num() == expected_seq_num
        utils.assert_msg_equal(
            msgs.Lwm2mRegister(path, options=[block1]), pkt)
        payload.extend(pkt.content)

        if block1.has_more():
            server.send(
                msgs.Lwm2mContinue.matching(pkt)(options=[block1]))
            expected_seq_num += 1
        else:
            server.send(
                msgs.Lwm2mCreated.matching(pkt)(
                    location=f"/rd/{endpoint}"))
            break

    assert expected_seq_num > 0
    return bytes(payload)

def _complete_bootstrap_and_register_to_psk_server_blockwise(bootstrap_server, regular_server):
    _provision_regular_psk_server_via_bootstrap(
        bootstrap_server, regular_server)
    _finish_bootstrap(bootstrap_server)
    _handle_blockwise_register(regular_server)

@utils.app_config([
    {"ANJ_WITH_PERSISTENCE": "ON", "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE": "OFF"},
    {"ANJ_WITH_PERSISTENCE": "ON", "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE": "ON"}
]
)
def test_bootstrap_nosec_with_nosec_registration_persistence(app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(app, bootstrap_server)

        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_nosec_server(bootstrap_server, regular_server)

    regular_server.reset()

    with app_spawner.spawn_app() as app:
        app.rpc.call("init_persistence", ENDPOINT)
        _handle_register(regular_server)

@utils.app_config({"ANJ_WITH_PERSISTENCE": "ON"})
def test_bootstrap_sec_with_sec_registration_persistence(app_spawner):
    bootstrap_server = _make_psk_server(PSK_IDENTITY_BS, PSK_KEY_BS)
    regular_server = _make_psk_server(PSK_IDENTITY, PSK_KEY)

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_psk_server(app, bootstrap_server)

        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_psk_server(bootstrap_server, regular_server)

    regular_server.reset()

    with app_spawner.spawn_app() as app:
        app.rpc.call("init_persistence", ENDPOINT)
        _handle_register(regular_server)

@utils.app_config({"ANJ_WITH_PERSISTENCE": "ON", "ANJ_OUT_PAYLOAD_BUFFER_SIZE": 16})
def test_bootstrap_sec_with_sec_registration_persistence_blockwise(app_spawner):
    bootstrap_server = _make_psk_server(PSK_IDENTITY_BS, PSK_KEY_BS)
    regular_server = _make_psk_server(PSK_IDENTITY, PSK_KEY)

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_psk_server(app, bootstrap_server)

        _handle_bootstrap_request(bootstrap_server)
        _complete_bootstrap_and_register_to_psk_server_blockwise(bootstrap_server, regular_server)

    regular_server.reset()

    with app_spawner.spawn_app() as app:
        app.rpc.call("init_persistence", ENDPOINT)
        _handle_blockwise_register(regular_server)


@utils.app_config({"ANJ_OUT_PAYLOAD_BUFFER_SIZE": 16})
def test_bootstrap_with_blockwise_registration(app_spawner):
    bootstrap_server = _make_nosec_server()
    regular_server = _make_nosec_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(app, bootstrap_server)

        _handle_bootstrap_request(bootstrap_server)
        _provision_regular_nosec_server_via_bootstrap(
            bootstrap_server, regular_server)
        _finish_bootstrap(bootstrap_server)

        payload = _handle_blockwise_register(regular_server)
        assert payload == (
            b"</1>;ver=1.2,</1/0>,"
            b"</3>;ver=1.0,</3/0>,"
            b"</5>;ver=1.0,</5/0>,"
            b"</1234>;ver=1.1,</1234/0>,</1234/1>,</1234/2>"
        )
