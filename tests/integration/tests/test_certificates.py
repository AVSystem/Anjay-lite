# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import certs
import utils
import time
from dataclasses import replace

from framework_tools.lwm2m.server import Lwm2mServer, coap
from framework_tools.lwm2m.coap.transport import Transport
from framework_tools.lwm2m.coap.content_format import ContentFormat
from framework_tools.lwm2m.senml_cbor import CBOR, SenmlLabel
import framework_tools.lwm2m.messages as msgs


ENDPOINT = "test-endpoint"
LIFETIME = 100

#################### Helper functions for certificate tests ##############


def _make_cert_server(credentials):
    return Lwm2mServer(coap.TlsServer(
        ca_file=str(credentials.dtls_server_client_ca),
        crt_file=str(credentials.dtls_server_cert_chain),
        key_file=str(credentials.dtls_server_private_key),
        transport=Transport.UDP
    ))


def _init_app_with_cert_server(app,
                               server,
                               credentials,
                               endpoint=ENDPOINT,
                               lifetime=LIFETIME,
                               with_anjay_server_public_key=True,
                               server_name_indication=None,
                               certificate_usage=None,
                               bootstrap_server=False,
                               communication_retry=None,
                               trust_store=None):
    config = {
        "endpoint": endpoint,
        "servers": [{
            "uri": f"coaps://127.0.0.1:{server.get_listen_port()}",
            "security": {
                "kind": "cert",
                "client_cert_path": str(credentials.anjay_client_cert),
                "client_key_path": str(credentials.anjay_client_private_key)
            },
            "lifetime": lifetime,
            "bootstrap": bootstrap_server
        }]
    }
    if with_anjay_server_public_key:
        config["servers"][0]["security"]["server_public_key_path"] = str(
            credentials.anjay_server_public_key)
    if server_name_indication:
        config["servers"][0]["security"]["server_name_indication"] = server_name_indication
    if certificate_usage is not None:
        config["servers"][0]["security"]["certificate_usage"] = certificate_usage
    if communication_retry is not None:
        config["servers"][0]["communication_retry"] = communication_retry

    if trust_store is not None:
        config["trust_store"] = trust_store

    assert app.rpc.call("init", config) == 0


def _handle_register(server,
                     endpoint=ENDPOINT,
                     lifetime=LIFETIME,
                     timeout_s=None,
                     accept_register=True):
    expected = msgs.Lwm2mRegister(
        f"/rd?ep={endpoint}&lt={lifetime}&lwm2m=1.2&b=U")
    pkt = server.recv(timeout_s=timeout_s) if timeout_s else server.recv()
    utils.assert_msg_equal(expected, pkt)
    if accept_register:
        server.send(msgs.Lwm2mCreated.matching(pkt)(
            location=f"/rd/{endpoint}"))
    return pkt


# Most basic test:connection over DTLS with certificate credentials,
# and successful registration.
def test_connection_over_dtls_certificates(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(app, server, credentials)
        _handle_register(server)


# Same as above, but with client credentials in DER format instead of PEM.
def test_connection_over_dtls_certificates_der(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    # Convert client cert and key to DER format for Anjay Lite,
    # but keep PEM files for the test server.
    credentials = certs.convert_credentials_to_der(certificate_environment,
                                                   credentials)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(app, server, credentials)
        _handle_register(server)


# The result should be identical like in the test_connection_over_dtls_certificates()
# test, but here we explicitly set certificate usage to 3
def test_connection_over_dtls_certificates_certificate_usage_explicit_set(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app, server, credentials, certificate_usage=3)
        _handle_register(server)


# Registration accepted when certificate credentials are passed to Anjay Lite
# without Server Public Key (/0/x/4) in the credentials.
@utils.app_config({
    "ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP": "ON",
})
def test_connection_over_dtls_certificates_no_server_cert(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app, server, credentials, with_anjay_server_public_key=False)
        _handle_register(server)


# Registration accepted when certificate credentials are passed to Anjay Lite
# without Server Public Key (/0/x/4) in the credentials, using DER files
# instead of PEM.
@utils.app_config({
    "ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP": "ON",
})
def test_connection_over_dtls_certificates_no_server_cert_der(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    # Convert client cert and key to DER format for Anjay Lite,
    # but keep PEM files for the test server.
    credentials = certs.convert_credentials_to_der(certificate_environment,
                                                   credentials)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app, server, credentials, with_anjay_server_public_key=False)
        _handle_register(server)


def _expect_connection_failure_after_single_retry(
        app_spawner,
        server,
        credentials,
        with_anjay_server_public_key=True,
        certificate_usage=3,
        trust_store=None):
    with app_spawner.spawn_app() as app:
        # communication_retry is set to fail in first attempt
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            with_anjay_server_public_key=with_anjay_server_public_key,
            certificate_usage=certificate_usage,
            trust_store=trust_store,
            communication_retry={
                "retry_count": 1,
                "retry_timer_s": 1,
                "seq_delay_timer_s": 1,
                "seq_retry_count": 1})
        # anjay should switch to FAILURE state in the first call of
        # anj_core_step()
        time.sleep(1)
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.FAILURE


# Test similar to the test_connection_over_dtls_certificates_no_server_cert,
# but ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP is not set, so the client
# should reject the connection due to missing Server Public Key (/0/x/4).
def test_connection_over_dtls_certificates_no_server_cert_reject(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    server = _make_cert_server(credentials)
    _expect_connection_failure_after_single_retry(
        app_spawner, server, credentials, with_anjay_server_public_key=False)


# Check that SNI is properly sent by Anjay Lite when configured
def test_connection_over_dtls_certificates_sni_provided(
        app_spawner,
        certificate_environment):
    SNI = "hostname-for-sni"
    # The SNI value must be present in the server certificate SAN. Otherwise the
    # client will send the expected SNI, but reject the server certificate during
    # hostname verification.
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT,
        server_name=SNI)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app, server, credentials, server_name_indication=SNI)
        # catch the ClientHello and check that it contains the SNI extension
        # with the expected value
        datagram = server._raw_udp_socket.recv(4096)
        assert SNI.encode() in datagram, "SNI value not found in ClientHello datagram"

        # after ~1 s, the client should retransmit the dtls ClientHello,
        # and we should be able to process the registration as usual
        _handle_register(server)


# Check that when SNI is not explicitly configured, Anjay Lite sets SNI
# value as the server hostname from the URI, which is IP address in this case.
def test_connection_over_dtls_certificates_default_sni(
        app_spawner,
        certificate_environment):
    # URI value from _init_app_with_cert_server()
    SNI = "127.0.0.1"
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app, server, credentials)
        # catch the ClientHello and check that it contains the SNI extension
        # with the expected value
        datagram = server._raw_udp_socket.recv(4096)
        assert SNI.encode() in datagram, "SNI value not found in ClientHello datagram"

        # after ~1 s, the client should retransmit the dtls ClientHello,
        # and we should be able to process the registration as usual
        _handle_register(server)


# Check that the client rejects the server certificate if configured SNI does
# not match the server certificate SAN.
def test_connection_over_dtls_certificates_sni_mismatched_with_server_san(
        app_spawner,
        certificate_environment):
    CERT_SERVER_NAME = "hostname-from-server-certificate"
    SNI = "different-hostname-for-sni"

    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT,
        server_name=CERT_SERVER_NAME)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app, server, credentials, server_name_indication=SNI)
        # Alert is sent by the client after it receives the server certificate and checks that
        # the SNI value does not match the server certificate SAN.
        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)


#################### Helper functions for bootstrap tests ##############


def _send_bootstrap_write_and_expect_changed(server, path, payload):
    write = msgs.Lwm2mWrite(
        path,
        format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR,
        content=payload)
    server.send(write)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(write)(), pkt)


def _handle_bootstrap_request(server):
    expected = msgs.Lwm2mRequestBootstrap(
        endpoint_name=ENDPOINT,
        preferred_content_format=ContentFormat.APPLICATION_LWM2M_SENML_CBOR)
    pkt = server.recv()
    utils.assert_msg_equal(expected, pkt)
    server.send(msgs.Lwm2mChanged.matching(pkt)())

def _provision_psk_server_via_bootstrap(bootstrap_server,
                                       regular_server,
                                       psk_identity,
                                       psk_key):
    new_instance_ssid = 4
    security_payload = CBOR.serialize([{SenmlLabel.NAME: "/0/1/0",
                                        SenmlLabel.STRING: f"coaps://127.0.0.1:{regular_server.get_listen_port()}"},
                                       {SenmlLabel.NAME: "/0/1/2",
                                        SenmlLabel.VALUE: 2},
                                       {SenmlLabel.NAME: "/0/1/3",
                                        SenmlLabel.OPAQUE: psk_identity.encode()},
                                       {SenmlLabel.NAME: "/0/1/4",
                                        SenmlLabel.OPAQUE: psk_key.encode()},
                                       {SenmlLabel.NAME: "/0/1/10",
                                        SenmlLabel.VALUE: new_instance_ssid}])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, "/0/1", security_payload)
    server_payload = CBOR.serialize([
        {SenmlLabel.NAME: "/1/0/0", SenmlLabel.VALUE: new_instance_ssid},
        {SenmlLabel.NAME: "/1/0/1", SenmlLabel.VALUE: LIFETIME},
        {SenmlLabel.NAME: "/1/0/6", SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: "/1/0/7", SenmlLabel.STRING: "U"}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, "/1/0", server_payload)


def _provision_regular_cert_server_via_bootstrap(bootstrap_server,
                                                 regular_server,
                                                 regular_credentials):
    new_instance_ssid = 4
    security_payload = CBOR.serialize([{SenmlLabel.NAME: "/0/1/0",
                                        SenmlLabel.STRING: f"coaps://127.0.0.1:{regular_server.get_listen_port()}"},
                                       {SenmlLabel.NAME: "/0/1/2",
                                        SenmlLabel.VALUE: 2},
                                       {SenmlLabel.NAME: "/0/1/3",
                                        SenmlLabel.OPAQUE: regular_credentials.anjay_client_cert.read_bytes()},
                                       {SenmlLabel.NAME: "/0/1/4",
                                        SenmlLabel.OPAQUE: regular_credentials.anjay_server_public_key.read_bytes()},
                                       {SenmlLabel.NAME: "/0/1/5",
                                        SenmlLabel.OPAQUE: regular_credentials.anjay_client_private_key.read_bytes()},
                                       {SenmlLabel.NAME: "/0/1/10",
                                        SenmlLabel.VALUE: new_instance_ssid}])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, "/0/1", security_payload)

    server_payload = CBOR.serialize([
        {SenmlLabel.NAME: "/1/0/0", SenmlLabel.VALUE: new_instance_ssid},
        {SenmlLabel.NAME: "/1/0/1", SenmlLabel.VALUE: LIFETIME},
        {SenmlLabel.NAME: "/1/0/6", SenmlLabel.BOOL: False},
        {SenmlLabel.NAME: "/1/0/7", SenmlLabel.STRING: "U"}
    ])
    _send_bootstrap_write_and_expect_changed(
        bootstrap_server, "/1/0", server_payload)


def _finish_bootstrap(server, expect_dtls_close_notify):
    finish = msgs.Lwm2mBootstrapFinish()
    server.send(finish)
    pkt = server.recv()
    utils.assert_msg_equal(msgs.Lwm2mChanged.matching(finish)(), pkt)

    if expect_dtls_close_notify:
        # After Bootstrap Finish the bootstrap DTLS transport shall be closed
        # before connecting to the regular LwM2M Server.
        utils.expect_dtls_close_notify(server)


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


def _make_nosec_server():
    return Lwm2mServer(coap.Server(transport=Transport.UDP))


def _make_psk_server(psk_identity="bootstrap-test-identity",
                     psk_key="bootstrap-test-key"):
    return Lwm2mServer(coap.TlsServer(
        psk_identity=psk_identity,
        psk_key=psk_key,
        transport=Transport.UDP
    ))


def _init_app_with_bootstrap_nosec_server(app,
                                          server,
                                          endpoint=ENDPOINT,
                                          lifetime=LIFETIME):
    assert app.rpc.call("init", {
        "endpoint": endpoint,
        "servers": [{
            "uri": f"coap://127.0.0.1:{server.get_listen_port()}",
            "security": {
                "kind": "nosec"
            },
            "lifetime": lifetime,
            "bootstrap": True
        }]
    }) == 0


def _init_app_with_bootstrap_psk_server(app,
                                        server,
                                        endpoint=ENDPOINT,
                                        psk_identity="bootstrap-test-identity",
                                        psk_key="bootstrap-test-key",
                                        lifetime=LIFETIME):
    assert app.rpc.call("init", {
        "endpoint": endpoint,
        "servers": [{
            "uri": f"coaps://127.0.0.1:{server.get_listen_port()}",
            "security": {
                "kind": "psk",
                "psk_identity": psk_identity,
                "psk_key": psk_key
            },
            "lifetime": lifetime,
            "bootstrap": True
        }]
    }) == 0

def _complete_bootstrap_with_psk_provisioning(bootstrap_server,
                                             regular_server,
                                             psk_identity,
                                              psk_key,
                                             expect_dtls_close_notify):
    _handle_bootstrap_request(bootstrap_server)
    _provision_psk_server_via_bootstrap(
        bootstrap_server, regular_server, psk_identity, psk_key)
    _finish_bootstrap(
        bootstrap_server,
        expect_dtls_close_notify=expect_dtls_close_notify)

def _complete_bootstrap_with_cert_provisioning(bootstrap_server,
                                               regular_server,
                                               regular_credentials,
                                               expect_dtls_close_notify):
    _handle_bootstrap_request(bootstrap_server)
    _provision_regular_cert_server_via_bootstrap(
        bootstrap_server, regular_server, regular_credentials)
    _finish_bootstrap(
        bootstrap_server,
        expect_dtls_close_notify=expect_dtls_close_notify)

    _handle_register(regular_server)


# Bootstrap over NoSec, then regular connection over DTLS with certificates.
def test_bootstrap_nosec_with_certificates_and_registration(
        app_spawner,
        certificate_environment):
    regular_credentials = _make_regular_cert_credentials_for_bootstrap(
        certificate_environment)

    bootstrap_server = _make_nosec_server()
    regular_server = _make_cert_server(regular_credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_nosec_server(app, bootstrap_server)
        _complete_bootstrap_with_cert_provisioning(
            bootstrap_server,
            regular_server,
            regular_credentials,
            expect_dtls_close_notify=False)


# Bootstrap over DTLS-PSK, then regular connection over DTLS with certificates.
def test_bootstrap_psk_with_certificates_and_registration(
        app_spawner,
        certificate_environment):
    regular_credentials = _make_regular_cert_credentials_for_bootstrap(
        certificate_environment)

    bootstrap_server = _make_psk_server()
    regular_server = _make_cert_server(regular_credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_bootstrap_psk_server(app, bootstrap_server)
        _complete_bootstrap_with_cert_provisioning(
            bootstrap_server,
            regular_server,
            regular_credentials,
            expect_dtls_close_notify=True)


def test_bootstrap_certificates_with_psk_and_registration(
        app_spawner,
        certificate_environment):
    bootstrap_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="bootstrap",
        endpoint=ENDPOINT)

    bootstrap_server = _make_cert_server(bootstrap_credentials)
    regular_server = _make_psk_server()

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            bootstrap_server,
            bootstrap_credentials,
            bootstrap_server=True)
        _complete_bootstrap_with_psk_provisioning(
            bootstrap_server,
            regular_server,
            "regular-psk-identity",
            "regular-psk-key",
            expect_dtls_close_notify=True)


# Bootstrap over DTLS with certificates, then regular connection over DTLS with
# certificates provisioned by the bootstrap server.
def test_bootstrap_certificates_with_certificates_and_registration(
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
        _init_app_with_cert_server(
            app,
            bootstrap_server,
            bootstrap_credentials,
            bootstrap_server=True)
        _complete_bootstrap_with_cert_provisioning(
            bootstrap_server,
            regular_server,
            regular_credentials,
            expect_dtls_close_notify=True)


# Client rejects the connection when the configured server certificate does not
# match the certificate presented by the server.
def test_connection_over_dtls_certificates_server_certificate_mismatch(
        app_spawner,
        certificate_environment):
    server_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="server",
        endpoint=ENDPOINT)

    other_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    # Keep the client certificate/key matching the test server trust setup, but
    # configure Anjay Lite with a different server certificate than the one
    # actually presented by the server.
    client_credentials = replace(
        server_credentials,
        anjay_server_public_key=other_credentials.anjay_server_public_key)

    server = _make_cert_server(server_credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(app, server, client_credentials)

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)


def _corrupt_file(source_path, target_path):
    data = bytearray(source_path.read_bytes())
    assert data
    data[1] += 1
    target_path.write_bytes(data)
    return target_path


# MbedTLS rejects malformed or non-parseable client certificate data.
# This happen before the DTLS handshake is performed, so we have to check
# the state of the anjay client to confirm the result.
def test_connection_over_dtls_certificates_invalid_client_certificate_data(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="invalid-client-cert",
        endpoint=ENDPOINT)

    credentials = certs.convert_credentials_to_der(
        certificate_environment,
        credentials)

    malformed_client_cert = _corrupt_file(
        credentials.anjay_client_cert,
        certificate_environment.certificates_dir / "malformed_client_cert.der")

    credentials = replace(
        credentials,
        anjay_client_cert=malformed_client_cert)

    server = _make_cert_server(credentials)
    _expect_connection_failure_after_single_retry(
        app_spawner, server, credentials)

# Server rejects the client when the client certificate is signed by an
# untrusted CA, checks if anjay lite can handle this situation properly
def test_connection_over_dtls_certificates_client_certificate_rejected_by_server(
        app_spawner, certificate_environment):
    server_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="trusted-by-server",
        endpoint=ENDPOINT)

    untrusted_client_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="untrusted-client",
        endpoint=ENDPOINT)

    # The server presents server_credentials.dtls_server_cert_chain and trusts
    # only server_credentials.root_ca_cert. The client is configured with a
    # valid anjay_server_public_key, but presents a client certificate signed
    # by a different CA.
    client_credentials = replace(
        server_credentials,
        anjay_client_cert=untrusted_client_credentials.anjay_client_cert,
        anjay_client_private_key=untrusted_client_credentials.anjay_client_private_key)

    server = _make_cert_server(server_credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(app, server, client_credentials)

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)

        # With default retry settings, Anjay Lite should keep trying to
        # register after the failed handshake.
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING


# Client rejects the server certificate when it is not valid yet.
def test_connection_over_dtls_certificates_server_certificate_not_yet_valid(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT,
        server_cert_not_yet_valid=True)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(app, server, credentials)

        # The server presents a certificate signed by the expected CA, but its
        # notBefore timestamp is in the future. Anjay Lite shall reject it
        # during server certificate verification and send a fatal alert.
        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING


# Similar to test_connection_over_dtls_certificates test, but with crypto
# storage enabled.
@utils.app_config({
    "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE": "ON",
    "ANJ_WITH_CRYPTO_STORAGE_DEFAULT": "OFF"
})
def test_connection_over_dtls_certificates_external_crypto_storage(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(app, server, credentials)
        _handle_register(server)

# Similar to test_bootstrap_certificates_with_certificates_and_registration test,
# but with crypto storage enabled.
@utils.app_config({
    "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE": "ON",
    "ANJ_WITH_CRYPTO_STORAGE_DEFAULT": "OFF"
})
def test_bootstrap_with_certificates_external_crypto_storage(
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
        _init_app_with_cert_server(
            app,
            bootstrap_server,
            bootstrap_credentials,
            bootstrap_server=True)
        _complete_bootstrap_with_cert_provisioning(
            bootstrap_server,
            regular_server,
            regular_credentials,
            expect_dtls_close_notify=True)

# Certificate Usage 3: client validates the server leaf certificate against the
# configured server certificate
def test_connection_over_dtls_certificate_chain_with_certificate_usage_3(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=3)

        _handle_register(server)


# Certificate Usage 3 requires the configured server certificate to match the
# server leaf certificate. If a CA/root certificate is configured instead, the
# client shall reject the connection.
def test_connection_over_dtls_certificate_usage_3_rejects_ca_certificate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    # The server still presents a valid chain, but the client is configured
    # with the root CA certificate instead of the expected leaf certificate.
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=3)

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING


# MbedTLS integration layer rejects anjay_server_public_key containing more than one
# certificate. The server may present a chain, but Security Object /0/x/4 must
# contain a single certificate. Buffer size is set to 3000 to be able to
# hold the whole chain.
@utils.app_config({
    "ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE": "3000",
})
def test_connection_over_dtls_certificates_anjay_server_public_key_chain_rejected(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    # credentials.dtls_server_cert_chain is the full chain used by the Python
    # DTLS server. Configure the same chain as Anjay Lite's
    # anjay_server_public_key. This shall be rejected because only a single
    # certificate is allowed there.
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.dtls_server_cert_chain)

    server = _make_cert_server(credentials)

    _expect_connection_failure_after_single_retry(
        app_spawner, server, credentials)


# Certificate Usage 2: client treats the configured Server Public Key (/0/x/4)
# as a trust anchor. A leaf certificate is rejected, while intermediate and root
# CA certificates from the server chain are accepted.
def test_connection_over_dtls_certificate_chain_with_certificate_usage_2(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    # generate_certificate_chain_credentials() sets anjay_server_public_key to
    # the server leaf certificate by default. For certificate_usage=2, /0/x/4
    # must contain a CA certificate used as a trust anchor, so this shall fail.
    server_1 = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server_1,
            credentials,
            certificate_usage=2)

        utils.expect_dtls_handshake_rejected(
            server_1, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

    # Configure the intermediate CA as the trust anchor. This shall be accepted,
    # for certificate_usage=2
    intermediate_credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server_2 = _make_cert_server(intermediate_credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server_2,
            intermediate_credentials,
            certificate_usage=2)
        _handle_register(server_2)

    # Configure the root CA as the trust anchor. This shall also be accepted.
    root_credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    server_3 = _make_cert_server(root_credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server_3,
            root_credentials,
            certificate_usage=2)
        _handle_register(server_3)


# Certificate Usage 2: Server Public Key (/0/x/4) does not match any certificate
# from the server chain.
def test_connection_over_dtls_certificate_chain_with_certificate_usage_2_invalid_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    invalid_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="invalid-chain",
        endpoint=ENDPOINT)

    credentials = replace(
        credentials,
        anjay_server_public_key=invalid_credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=2)

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 2: requires Server Public Key (/0/x/4) to contain a CA
# certificate used as a trust anchor. If the server presents a single leaf
# certificate and the client is configured with that leaf as the trust anchor,
# the connection shall be rejected.
def test_connection_over_dtls_certificate_usage_2_rejects_single_leaf_cert(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=2)

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 2: Server Public Key (/0/x/4) is used as a trust anchor,
# but the server still has to present a valid end-entity certificate. If the
# server presents a single CA certificate as its own certificate, mbedTLS rejects
# the connection, even if that CA certificate is configured as the trust
# anchor.
def test_connection_over_dtls_certificate_usage_2_rejects_single_ca_cert(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_single_ca_server_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=2)

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING


def _write_certificate_chain(out_path, *cert_paths):
    out_path.write_bytes(b"".join(path.read_bytes() for path in cert_paths))
    return out_path


# Certificate Usage 2: the configured Server Public Key (/0/x/4) is used as a
# trust anchor, but the peer certificate chain must still be cryptographically
# valid. The connection is rejected if the server sends a chain that contains
# the configured root CA, but the leaf certificate is not signed by the
# certificate above it.
# "The target certificate MUST pass PKIX certification path validation, with
# any certificate matching the Server Public Key Resource content considered
# to be a trust anchor for this certification path validation."
def test_connection_over_dtls_certificate_usage_2_rejects_broken_chain(
        app_spawner,
        certificate_environment):
    trusted_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="certificate-usage-2-broken-chain-trusted",
        endpoint=ENDPOINT)

    wrong_leaf_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="certificate-usage-2-broken-chain-wrong-leaf",
        endpoint=ENDPOINT)

    # wrong leaf -> valid intermediate -> valid root
    broken_server_chain = _write_certificate_chain(
        certificate_environment.certificates_dir / "broken_server_chain.pem",
        wrong_leaf_credentials.server_leaf_cert,
        trusted_credentials.intermediate_ca_cert,
        trusted_credentials.root_ca_cert)

    credentials = replace(
        trusted_credentials,
        # Client trusts this root as the usage 2 trust anchor.
        anjay_server_public_key=trusted_credentials.root_ca_cert,
        # Server presents a syntactically valid chain file, but the leaf comes
        # from another hierarchy and is not signed by trusted intermediate.
        dtls_server_cert_chain=broken_server_chain,
        # The private key must match the first certificate presented by the
        # server, i.e. wrong_leaf_credentials.server_leaf_cert.
        dtls_server_private_key=wrong_leaf_credentials.dtls_server_private_key)

    server_1 = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(app, server_1, credentials, certificate_usage=2)

        utils.expect_dtls_handshake_rejected(
            server_1, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

    # Situation similar to the above, but anjay_server_public_key is set to
    # the intermediate certificate instead of the root.
    # Server still presents the same broken chain:
    # wrong leaf -> valid intermediate -> valid root
    credentials = replace(credentials,
                          anjay_server_public_key=trusted_credentials.intermediate_ca_cert)
    server_2 = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(app, server_2, credentials, certificate_usage=2)

        utils.expect_dtls_handshake_rejected(
            server_2, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)


# Certificate Usage 2: /0/x/4 is the root CA trust anchor.
# Server sends: leaf -> intermediate
# Client has:   /0/x/4 = root CA
#
# Expected validation path: leaf -> intermediate -> root CA (/0/x/4)
#
# The root CA does not need to be sent by the server. It is configured locally
# as the cert_usage=2 Trust anchor assertion.
def test_connection_over_dtls_certificate_usage_2_accepts_root_ca_not_sent_by_server(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server_chain_without_root = _write_certificate_chain(
        certificate_environment.certificates_dir / "server_chain_without_root.pem",
        credentials.server_leaf_cert,
        credentials.intermediate_ca_cert)

    credentials = replace(
        credentials,
        dtls_server_cert_chain=server_chain_without_root,
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=2)

        _handle_register(server)


# Certificate Usage 2: /0/x/4 is the intermediate CA trust anchor.
#
# Server sends: leaf -> intermediate
# Client has: /0/x/4 = intermediate
#
# Expected validation path: leaf -> intermediate (/0/x/4)
#
# The configured trust anchor does not need to be self-signed. For cert_usage=2, the
# certification path may terminate at the configured intermediate CA.
def test_connection_over_dtls_certificate_usage_2_accepts_intermediate_ca_as_trust_anchor(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server_chain_without_root = _write_certificate_chain(
        certificate_environment.certificates_dir / "server_chain_without_root.pem",
        credentials.server_leaf_cert,
        credentials.intermediate_ca_cert)

    credentials = replace(
        credentials,
        dtls_server_cert_chain=server_chain_without_root,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=2)

        _handle_register(server)


# Certificate Usage 2: /0/x/4 is the intermediate CA trust anchor.
#
# Server sends: leaf
# Client has:   /0/x/4 = intermediate
#
# Expected validation path: leaf -> intermediate (/0/x/4)
#
# The intermediate does not need to be sent by the server because it is
# configured locally as the usage-2 trust anchor.
def test_connection_over_dtls_certificate_usage_2_accepts_leaf_only_with_intermediate_trust_anchor(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials = replace(
        credentials,
        dtls_server_cert_chain=credentials.server_leaf_cert,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=2)

        _handle_register(server)


# Certificate Usage 2: /0/x/4 is the root CA trust anchor.
#
# Server sends: leaf
# Client has:   /0/x/4 = root CA
#
# Expected validation path would be: leaf -> intermediate -> root CA (/0/x/4)
#
# The connection is rejected because the server does not send the intermediate
# certificate needed to build the path from leaf to the configured root trust
# anchor.
def test_connection_over_dtls_certificate_usage_2_rejects_missing_intermediate_for_root_ca(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials = replace(
        credentials,
        # The Python DTLS server presents only the leaf certificate.
        dtls_server_cert_chain=credentials.server_leaf_cert,
        # Anjay Lite is configured to use the root CA as the trust anchor.
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=2)

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 1 (PKIX-EE).
# /0/x/4 = server cert
# trust store = root cert
# 
# The server presents a single leaf certificate, /0/x/4 holds that same leaf,
# and the trust store contains the CA that signed it. The handshake shall succeed.
def test_connection_over_dtls_certificate_usage_1_pkix_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=1,
            trust_store=[str(credentials.dtls_server_client_ca)])

        _handle_register(server)

# Certificate Usage 1 (PKIX-EE).
# /0/x/4 = server intermediate cert
# trust store = root cert
#
# Certificate Usage 1 requires the configured Server Public Key (/0/x/4) to
# match the server leaf certificate. Here the server presents a valid
# leaf -> intermediate -> root chain, PKIX validation against the trust store
# would succeed, but /0/x/4 is set to the intermediate CA certificate instead
# of the leaf. The connection shall be rejected.
def test_connection_over_dtls_certificate_chain_usage_1_intermediate_instead_leaf_pkix_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=1,
            trust_store=[str(credentials.root_ca_cert)])

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 1 (PKIX-EE).
# /0/x/4 = server leaf cert but for different server
# trust store = root cert
#
# The server presents a valid leaf -> intermediate -> root
# chain that passes PKIX validation against the configured trust store, but
# /0/x/4 is set to a leaf certificate from an unrelated server. Because the
# configured Server Public Key does not match the leaf actually presented by
# the server, the connection shall be rejected.
def test_connection_over_dtls_certificate_chain_usage_1_different_server_cert_pkix_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    
    other_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)
    
    # configure Anjay Lite with a different server certificate than the one
    # actually presented by the server.
    credentials = replace(
        credentials,
        anjay_server_public_key=other_credentials.anjay_server_public_key)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=1,
            trust_store=[str(credentials.root_ca_cert)])

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 1 (PKIX-EE).
# /0/x/4 = server leaf cert
# trust store = intermediate cert
#
# /0/x/4 holds the server leaf certificate (matching the
# leaf actually presented by the server) and the trust store contains the
# intermediate CA. The PKIX validation path terminates at the intermediate
# trust anchor (the root CA does not need to be reachable from the trust
# store). The handshake shall succeed.
def test_connection_over_dtls_certificate_chain_usage_1_pkix_intermediate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=1,
            trust_store=[str(credentials.intermediate_ca_cert)])

        _handle_register(server)

# Certificate Usage 1 (PKIX-EE).
# /0/x/4 = server leaf cert
# trust store = root cert but for different server
#
# Correctly matches the server leaf certificate,
# but the trust store contains a CA from an unrelated PKI hierarchy. PKIX
# validation cannot build a path from the server's chain to any trusted
# anchor, so the connection shall be rejected.
def test_connection_over_dtls_certificate_chain_usage_1_wrong_pkix(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    
    other_credentials = certs.generate_certificate_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)

    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=1,
            trust_store=[str(other_credentials.dtls_server_client_ca)])

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 0 (PKIX-TA).
# /0/x/4 = root cert
# trust store = root cert
#
# The data model entry references the root CA, which is also present
# in the trust store and acts as the trust anchor, the connection shall be accepted.
def test_connection_over_dtls_certificate_usage_0_pkix_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials = replace(
    credentials,
    anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=0,
            trust_store=[str(credentials.root_ca_cert)])

        _handle_register(server)

# Certificate Usage 0 (PKIX-TA).
# /0/x/4 = intermediate ca cert
# trust store = intermediate ca cert
#
# The data model entry references the intermediate CA, which is also present
# in the trust store and acts as the trust anchor, the connection shall be accepted.
def test_connection_over_dtls_certificate_usage_0_pkix_intermediate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials = replace(
    credentials,
    anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=0,
            trust_store=[str(credentials.intermediate_ca_cert)])

        _handle_register(server)

# Certificate Usage 0 (PKIX-TA).
# /0/x/4 = other root cert
# trust store = root cert
#
# The data model entry references a root CA from an unrelated PKI hierarchy,
# while the trust store contains the correct root CA. PKIX validation
# requires the referenced CA to appear in the server's chain, which is not
# the case here, so the connection shall be rejected.
def test_connection_over_dtls_certificate_usage_0_cert_for_other_server_in_dm_pkix_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    
    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    credentials = replace(
    credentials,
    anjay_server_public_key=other_credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=0,
            trust_store=[str(credentials.root_ca_cert)])

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 0 (PKIX-TA).
# /0/x/4 = root cert
# trust store = other root cert
#
# The data model entry references the correct root CA, but the trust store
# contains a CA from an unrelated PKI hierarchy. PKIX validation cannot
# build a path from the server's chain to any trusted anchor, so the
# connection shall be rejected.
def test_connection_over_dtls_certificate_usage_0_pkix_cert_for_other_server(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    
    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    credentials = replace(
    credentials,
    anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=0,
            trust_store=[str(other_credentials.root_ca_cert)])

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 0 (PKIX-TA).
# /0/x/4 = server leaf cert
# trust store = intermediate ca cert
#
# The data model entry references the server's leaf certificate, but
# leaf certificate cannot act as a trust anchor, so the connection shall be rejected.
def test_connection_over_dtls_certificate_usage_0_leaf_in_dm_pkix_intermediate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=0,
            trust_store=[str(credentials.intermediate_ca_cert)])

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 0 (PKIX-TA).
# /0/x/4 = root cert
# trust store = intermediate ca and leaf cert
def test_connection_over_dtls_certificate_usage_0_dm_not_in_trust_store_pkix_intermediate_and_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials = replace(
    credentials,
    anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=0,
            trust_store=[str(credentials.intermediate_ca_cert),
                         str(credentials.server_leaf_cert)])

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# Certificate Usage 0 (PKIX-TA).
# /0/x/4 = root cert
# trust store = root, intermediate ca and leaf cert
def test_connection_over_dtls_certificate_usage_0_dm_not_in_hs_pkix_root_intermediate_and_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials.dtls_server_cert_chain.write_bytes(credentials.server_leaf_cert.read_bytes()
                                                   + credentials.intermediate_ca_cert.read_bytes())

    credentials = replace(
    credentials,
    anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=0,
            trust_store=[str(credentials.root_ca_cert),
                         str(credentials.intermediate_ca_cert),
                         str(credentials.server_leaf_cert)])

        utils.expect_dtls_handshake_rejected(
            server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)

        assert app.rpc.call("get_conn_status") == utils.ConnStatus.REGISTERING

# MbedTLS rejects malformed or non-parseable trust store certificate data.
# This happen before the DTLS handshake is performed, so we have to check
# the state of the anjay client to confirm the result.
def test_connection_over_dtls_certificates_usage_invalid_trust_store_certificate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials = replace(
    credentials,
    anjay_server_public_key=credentials.root_ca_cert)

    malformed_root_cert = _corrupt_file(
        credentials.root_ca_cert,
        certificate_environment.certificates_dir / "malformed_client_cert.der")

    server = _make_cert_server(credentials)
    _expect_connection_failure_after_single_retry(
        app_spawner, server, credentials, certificate_usage=0,
        trust_store=[str(malformed_root_cert)])

# Similar to test_connection_over_dtls_certificate_usage_0_pkix_root test, but with crypto
# storage enabled.
@utils.app_config({
    "ANJ_WITH_EXTERNAL_CRYPTO_STORAGE": "ON",
    "ANJ_WITH_CRYPTO_STORAGE_DEFAULT": "OFF"
})
def test_connection_over_dtls_certificates_external_storage(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    credentials = replace(
    credentials,
    anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    with app_spawner.spawn_app() as app:
        _init_app_with_cert_server(
            app,
            server,
            credentials,
            certificate_usage=0,
            trust_store=[str(credentials.root_ca_cert)])

        _handle_register(server)
