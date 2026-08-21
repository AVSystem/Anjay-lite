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
import framework_tools.lwm2m.messages as msgs


ENDPOINT = "test-endpoint"
LIFETIME = 100

#################### Helper functions ####################

def _make_cert_server(credentials):
    return Lwm2mServer(coap.TlsServer(
        ca_file=str(credentials.dtls_server_client_ca),
        crt_file=str(credentials.dtls_server_cert_chain),
        key_file=str(credentials.dtls_server_private_key),
        transport=Transport.UDP
    ))


def _test_connection_over_dtls_certificates(app_spawner,
                                            server,
                                            credentials,
                                            certificate_usage,
                                            failure,
                                            trust_store=None):
    with app_spawner.spawn_app() as app:
        config = {
            "endpoint": ENDPOINT,
            "servers": [{
                "uri": f"coaps://127.0.0.1:{server.get_listen_port()}",
                "security": {
                    "kind": "cert",
                    "client_cert_path": str(credentials.anjay_client_cert),
                    "client_key_path": str(credentials.anjay_client_private_key),
                    "server_public_key_path": str(credentials.anjay_server_public_key),
                    "certificate_usage": certificate_usage
                },
                "lifetime": LIFETIME,
            }]
        }
        if trust_store is not None:
            config["trust_store"] = trust_store

        assert app.rpc.call("init", config) == 0

        if not failure:
            expected = msgs.Lwm2mRegister(
                f"/rd?ep={ENDPOINT}&lt={LIFETIME}&lwm2m=1.2&b=U")
            pkt = server.recv()
            utils.assert_msg_equal(expected, pkt)
            server.send(msgs.Lwm2mCreated.matching(pkt)(
                location=f"/rd/{ENDPOINT}"))
        else:
            # Connection is failing during handshake
            utils.expect_dtls_handshake_rejected(
                server, utils.MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
            assert app.rpc.call(
                "get_conn_status") == utils.ConnStatus.REGISTERING


def _write_certificate_chain(out_path, *cert_paths):
    out_path.write_bytes(b"".join(path.read_bytes() for path in cert_paths))
    return out_path

###################################################################
####################### Certificate usage 3 #######################
###################################################################

# The Data Model contains the expected server leaf certificate.
# The handshake must present the same leaf certificate. Additional certificates
# in the handshake chain do not change the result, because the configured
# certificate is matched against the server certificate directly.


#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#
#       Result: Pass
def test_certificate_usage_3_dm_hs_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=False)

#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   x         x           x
#
#       Result: Pass
def test_certificate_usage_3_handshake_dm_leaf_hs_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=False)

#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   x         x           x
#
#       Result: Fail
def test_certificate_usage_3_dm_inter_hs_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=True)

#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#
#       Result: Fail
def test_certificate_usage_3_dm_inter_hs_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=True)

#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   x         x           x
#
#       Result: Fail
def test_certificate_usage_3_dm_root_hs_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=True)

#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         -           x
#
#       Result: Fail
def test_certificate_usage_3_dm_root_hs_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=True)

#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         x           x
#
#       Result: Fail
def test_certificate_usage_3_dm_root_hs_shorten_chain(
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
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=True)


#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   x         x           x
#
#       Result: Pass
#         Note: Trust store is provided but is ignored for certificate usage 3.
def test_certificate_usage_3_dm_hs_leaf_with_trust_store(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        dtls_server_cert_chain=credentials.server_leaf_cert)
    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#  Trust Store:   x         x           x
#
#       Result: Fail
#         Note: Trust store is provided but is ignored for certificate usage 3.
def test_certificate_usage_3_dm_inter_hs_leaf_with_trust_store(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)
    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=True,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 3
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         x           x
#
#       Result: Pass
def test_certificate_usage_3_dm_leaf_hs_shorten_chain(
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
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=3, failure=False)

###################################################################
####################### Certificate usage 2 #######################
###################################################################

# The Data Model contains a CA certificate that is used as the trust anchor
# for PKIX validation of the server certificate chain.
#
# The server leaf certificate must chain up to the CA certificate configured
# in the Data Model, using certificates provided in the handshake if needed.
# Configuring the server leaf certificate itself is not valid for this usage.


#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#
#       Result: Pass
def test_certificate_usage_2_dm_inter_hs_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=False)

#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   x         x           x
#
#       Result: Pass
def test_certificate_usage_2_dm_inter_hs_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=False)

#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   x         x           x
#
#       Result: Pass
def test_certificate_usage_2_dm_root_hs_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=False)

#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         x           x
#
#       Result: Pass
def test_certificate_usage_2_dm_root_hs_shorten_chain(
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
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=False)


#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         x           x
#
#       Result: Pass
def test_certificate_usage_2_dm_intermediate_hs_shorten_chain(
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
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=False)


#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#
#       Result: Fail
def test_certificate_usage_2_dm_hs_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=True)

#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         -           x
#
#       Result: Fail
def test_certificate_usage_2_dm_root_hs_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=True)


#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   x         x           x
#
#       Result: Fail
#         Note: Trust store is provided but is ignored for certificate usage 3.
def test_certificate_usage_2_dm_hs_leaf_with_trust_store(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        dtls_server_cert_chain=credentials.server_leaf_cert)
    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=True,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#  Trust Store:   x         x           x
#
#       Result: Pass
#         Note: Trust store is provided but is ignored for certificate usage 3.
def test_certificate_usage_2_dm_inter_hs_leaf_with_trust_store(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)
    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 2
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         x           x
#
#       Result: Fail
def test_certificate_usage_2_dm_leaf_hs_shorten_chain(
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
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=2, failure=True)

###################################################################
####################### Certificate usage 1 #######################
###################################################################

# The Data Model contains the expected server leaf certificate, and the
# handshake must present the same leaf certificate. The certificate chain must
# also be valid according to the trust store.
#
# The trust store must contain a CA certificate that allows the server leaf
# certificate to be validated. The server leaf certificate alone in the trust
# store is not enough.


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   x         x           x
#
#       Result: Pass
def test_certificate_usage_1_dm_leaf_hs_leaf_ts_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   -         x           x
#
#       Result: Pass
def test_certificate_usage_1_dm_leaf_hs_leaf_ts_shorten_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert)
            ])
    
#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   x         x           x
#  Trust Store:   x         x           x
#
#       Result: Pass
def test_certificate_usage_1_dm_leaf_hs_chain_ts_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])
    
#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   x         x           x
#  Trust Store:   -         x           x
#
#       Result: Pass
def test_certificate_usage_1_dm_leaf_hs_chain_ts_shorten_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert)
            ])

#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   -         x           -
#
#       Result: Pass
def test_certificate_usage_1_dm_leaf_hs_leaf_ts_intermediate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=False,
            trust_store=[
                str(credentials.intermediate_ca_cert)
            ])

#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   -         -           x
#
#       Result: Fail
#         Note: Trust store must contain CA certificate to build PKIX path
#               to the server leaf certificate.
def test_certificate_usage_1_dm_leaf_hs_leaf_ts_leaf(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.server_leaf_cert)
            ])
    
#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#  Trust Store:   -         x           -
#
#       Result: Fail
def test_certificate_usage_1_dm_intermediate_hs_leaf_ts_intermediate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.intermediate_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_1_dm_leaf_hs_leaf_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   x         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_1_dm_leaf_hs_chain_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert)
            ])

#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   x         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path, also the server leaf
#               certificate is not present in the Data Model
def test_certificate_usage_1_dm_inter_hs_chain_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   x         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_1_dm_root_hs_chain_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert),
                str(other_credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   x         x           x
#  Trust Store:   x         -           -
#
#       Result: Fail
def test_certificate_usage_1_dm_root_hs_chain_ts_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   x         x           x
#  Trust Store:   x         x           x
#
#       Result: Fail
def test_certificate_usage_1_dm_root_hs_chain_ts_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.root_ca_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.server_leaf_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_1_dm_root_hs_shorten_chain_ts_unrelated_certs(
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
        anjay_server_public_key=credentials.root_ca_cert,
        dtls_server_cert_chain=server_chain_without_root)
    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(other_credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         x           x
#  Trust Store:   x         -           -
#
#       Result: Fail
def test_certificate_usage_1_dm_root_hs_shorten_chain_ts_root(
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
        anjay_server_public_key=credentials.root_ca_cert,
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         -           x
#  Trust Store:   x         x           -
#
#       Result: Fail
def test_certificate_usage_1_dm_root_hs_leaf_ts_shorten_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   x         x           x
#  Trust Store:   x         -           -
#
#       Result: Pass
def test_certificate_usage_1_dm_intermediate_hs_chain_ts_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         x           x
#  Trust Store:   x         -           -
#
#       Result: Pass
def test_certificate_usage_1_dm_intermediate_hs_shorten_chain_ts_root(
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
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#  Trust Store:   x         -           -
#
#       Result: Fail
def test_certificate_usage_1_dm_intermediate_hs_leaf_ts_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   x         x           x
#  Trust Store:   x         -           -
#
#       Result: Pass
def test_certificate_usage_1_dm_leaf_hs_chain_ts_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=False,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_1_dm_leaf_hs_shorten_chain_ts_unrelated_certs(
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
        dtls_server_cert_chain=server_chain_without_root)
    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=True,
            trust_store=[
                str(other_credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         x           x
#  Trust Store:   x         -           -
#
#       Result: Pass
def test_certificate_usage_1_dm_leaf_hs_shorten_chain_ts_root(
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
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=False,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 1
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   x         x           -
#
#       Result: Pass
def test_certificate_usage_1_dm_leaf_hs_leaf_ts_root_inter(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=1, failure=False,
            trust_store=[
                str(credentials.root_ca_cert),
                str(credentials.intermediate_ca_cert)
            ])


###################################################################
####################### Certificate usage 0 #######################
###################################################################

# The connection is accepted only if the server certificate chain is
# valid and the configured CA certificate is present in the selected
# PKIX validation path.
#
# Some cases are implementation-dependent in practice. For example, MbedTLS
# may stop chain construction at the first trusted certificate, so a higher CA
# certificate from the Data Model may not be visible in the verification
# callback if an intermediate CA is already trusted (check
# `test_certificate_usage_0_dm_root_hs_chain_ts_chain` test for an example
# of this behavior).


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         x           x
#  Trust Store:   x         x           x
#
#       Result: Pass
def test_certificate_usage_0_dm_intermediate_hs_shorten_chain_ts_chain(
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
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   x         x           x
#  Trust Store:   x         x           x
#
#       Result: Pass
def test_certificate_usage_0_dm_intermediate_hs_chain_ts_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   x         x           x
#  Trust Store:   x         -           -
#
#       Result: Pass
def test_certificate_usage_0_dm_intermediate_hs_chain_ts_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=False,
            trust_store=[
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         x           x
#  Trust Store:   x         -           -
#
#       Result: Pass
#         Note: PKIX verification path contains root certificate, so the validation passes.
def test_certificate_usage_0_dm_root_hs_shorten_chain_ts_root(
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
        anjay_server_public_key=credentials.root_ca_cert,
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=False,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#  Trust Store:   x         x           x
#
#       Result: Pass
def test_certificate_usage_0_dm_intermediate_hs_leaf_ts_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=False,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   x         x           x
#  Trust Store:   x         x           x
#
#       Result: Fail
def test_certificate_usage_0_dm_leaf_hs_chain_ts_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   x         x           x
#  Trust Store:   x         x           x
#
#       Result: Fail
#         Note: This case is valid but mbedtls in callback function for certificate
#               verification does not present root certificate for this case, so the
#               validation fails. This happen because the MbedTLS ends chain validation
#               at the first certificate that is trusted, which is the intermediate
#               CA certificate in this case.
def test_certificate_usage_0_dm_root_hs_chain_ts_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   x         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path, also the data model
#               contains only the server leaf certificate
def test_certificate_usage_0_dm_leaf_hs_chain_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_0_dm_intermediate_hs_leaf_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert),
                str(other_credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   x         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_0_dm_intermediate_hs_chain_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert),
                str(other_credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   x         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_0_dm_root_hs_chain_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert),
                str(other_credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   x         x           x
#  Trust Store:   x         -           -
#
#       Result: Pass
def test_certificate_usage_0_dm_root_hs_chain_ts_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=False,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_0_dm_root_hs_shorten_chain_ts_unrelated_certs(
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
        anjay_server_public_key=credentials.root_ca_cert,
        dtls_server_cert_chain=server_chain_without_root)
    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(other_credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   x         -           -
#    Handshake:   -         -           x
#  Trust Store:   x         x           -
#
#       Result: Fail
#         Note: This case is valid but mbedtls in callback function for certificate
#               verification does not present root certificate for this case, so the
#               validation fails. This happen because the MbedTLS ends chain validation
#               at the first certificate that is trusted, which is the intermediate
#               CA certificate in this case.
def test_certificate_usage_0_dm_root_hs_leaf_ts_shorten_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.root_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         x           x
#  Trust Store:   x         -           -
#
#       Result: Pass
def test_certificate_usage_0_dm_intermediate_hs_shorten_chain_ts_root(
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
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=False,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#  Trust Store:   x         -           -
#
#       Result: Fail
def test_certificate_usage_0_dm_intermediate_hs_leaf_ts_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.root_ca_cert)
            ])

#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         x           -
#    Handshake:   -         -           x
#  Trust Store:   -         x           -
#
#       Result: Pass
def test_certificate_usage_0_dm_intermediate_hs_leaf_ts_intermediate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.intermediate_ca_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=False,
            trust_store=[
                str(credentials.intermediate_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   x         x           x
#  Trust Store:   x         -           -
#
#       Result: Fail
def test_certificate_usage_0_dm_leaf_hs_chain_ts_root(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         x           x
#  Trust Store:   -         -           -
#
#       Result: Fail
#         Note: Trust store is provided but does not contain any CA
#               certificate to build PKIX path
def test_certificate_usage_0_dm_leaf_hs_shorten_chain_ts_unrelated_certs(
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
        dtls_server_cert_chain=server_chain_without_root)
    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(other_credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         x           x
#  Trust Store:   x         -           -
#
#       Result: Fail
def test_certificate_usage_0_dm_leaf_hs_shorten_chain_ts_root(
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
        dtls_server_cert_chain=server_chain_without_root)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   -         -           -
#
#       Result: Fail
def test_certificate_usage_0_dm_leaf_hs_leaf_ts_unrelated_certs(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    other_credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        directory_name="other-server",
        endpoint=ENDPOINT)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(other_credentials.server_leaf_cert),
                str(other_credentials.intermediate_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   x         x           -
#
#       Result: Fail
def test_certificate_usage_0_dm_leaf_hs_leaf_ts_root_inter(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.root_ca_cert),
                str(credentials.intermediate_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   x         x           x
#
#       Result: Fail
def test_certificate_usage_0_dm_leaf_hs_leaf_ts_chain(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.server_leaf_cert),
                str(credentials.intermediate_ca_cert),
                str(credentials.root_ca_cert)
            ])


#  Certificate usage: 0
#
#               Root   Intermediate   Leaf
#   Data Model:   -         -           x
#    Handshake:   -         -           x
#  Trust Store:   -         x           -
#
#       Result: Fail
def test_certificate_usage_0_dm_leaf_hs_leaf_ts_intermediate(
        app_spawner,
        certificate_environment):
    credentials = certs.generate_certificate_chain_credentials(
        certificate_environment,
        endpoint=ENDPOINT)
    credentials = replace(
        credentials,
        anjay_server_public_key=credentials.server_leaf_cert,
        dtls_server_cert_chain=credentials.server_leaf_cert)

    server = _make_cert_server(credentials)
    _test_connection_over_dtls_certificates(
        app_spawner, server, credentials, certificate_usage=0, failure=True,
            trust_store=[
                str(credentials.intermediate_ca_cert)
            ])
