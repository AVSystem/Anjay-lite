# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
import subprocess
from typing import Optional


@dataclass(frozen=True)
class CertificateCredentials:
    # Files passed to Anjay Lite via SecurityConfig.
    anjay_client_cert: Path
    anjay_client_private_key: Path
    anjay_server_public_key: Path

    # PEM files used by the Python test DTLS server.
    dtls_server_cert_chain: Path
    dtls_server_private_key: Path
    dtls_server_client_ca: Path

    # Optional fields for tests that require access to specific certificates in the chain.
    server_leaf_cert: Optional[Path] = None
    root_ca_cert: Optional[Path] = None
    intermediate_ca_cert: Optional[Path] = None


def _run(openssl_path, *args):
    cmd = [openssl_path, *map(str, args)]
    result = subprocess.run(
        cmd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    if result.returncode != 0:
        raise RuntimeError(
            f"OpenSSL command failed: {' '.join(cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}")


def _write(path, content):
    path.write_text(content.strip() + "\n", encoding="utf-8")


def _openssl_time(dt):
    return dt.strftime("%Y%m%d%H%M%SZ")


def _sign_cert_with_ca_custom_validity(openssl_path,
                                       cert_dir,
                                       *,
                                       csr,
                                       ca_cert,
                                       ca_key,
                                       out_cert,
                                       extfile,
                                       not_before,
                                       not_after):
    ca_dir = Path(cert_dir) / "ca"
    ca_dir.mkdir(parents=True, exist_ok=True)
    (ca_dir / "newcerts").mkdir(exist_ok=True)
    (ca_dir / "index.txt").write_text("", encoding="utf-8")
    (ca_dir / "serial").write_text("01\n", encoding="utf-8")

    ca_config = ca_dir / "openssl-ca.cnf"
    _write(ca_config, f"""
        [ ca ]
        default_ca = CA_default

        [ CA_default ]
        database = {ca_dir / "index.txt"}
        serial = {ca_dir / "serial"}
        new_certs_dir = {ca_dir / "newcerts"}
        certificate = {ca_cert}
        private_key = {ca_key}
        default_md = sha256
        default_days = 365
        policy = policy_any
        email_in_dn = no
        copy_extensions = copy
        unique_subject = no

        [ policy_any ]
        commonName = supplied
    """)

    _run(openssl_path, "ca",
         "-batch",
         "-notext",
         "-config", ca_config,
         "-in", csr,
         "-out", out_cert,
         "-extfile", extfile,
         "-startdate", _openssl_time(not_before),
         "-enddate", _openssl_time(not_after))


def _generate_pem_files(openssl_path,
                        cert_dir,
                        *,
                        endpoint="test-endpoint",
                        server_name="localhost",
                        days=365,
                        server_cert_not_yet_valid=False):
    cert_dir = Path(cert_dir)
    cert_dir.mkdir(parents=True, exist_ok=True)

    root_key = cert_dir / "root_key.pem"
    root_csr = cert_dir / "root.csr"
    root_cert = cert_dir / "root_cert.pem"
    root_ext = cert_dir / "root.ext"

    client_key = cert_dir / "client_key.pem"
    client_csr = cert_dir / "client.csr"
    client_cert = cert_dir / "client_cert.pem"
    client_ext = cert_dir / "client.ext"

    server_key = cert_dir / "server_key.pem"
    server_csr = cert_dir / "server.csr"
    server_cert = cert_dir / "server_cert.pem"
    server_ext = cert_dir / "server.ext"

    _write(root_ext, """
        basicConstraints = critical, CA:TRUE
        keyUsage = critical, keyCertSign, cRLSign
    """)
    _write(client_ext, """
        basicConstraints = critical, CA:FALSE
        keyUsage = critical, digitalSignature
        extendedKeyUsage = clientAuth
    """)
    _write(server_ext, f"""
        basicConstraints = critical, CA:FALSE
        keyUsage = critical, digitalSignature
        extendedKeyUsage = serverAuth
        subjectAltName = DNS:{server_name},IP:127.0.0.1
    """)

    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", root_key)
    _run(openssl_path, "req", "-new", "-key", root_key, "-sha256", "-out",
         root_csr, "-subj", "/CN=integration-test-root")
    _run(openssl_path, "x509", "-req", "-in", root_csr, "-signkey", root_key,
         "-out", root_cert, "-days", days, "-sha256", "-extfile", root_ext)

    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", client_key)
    _run(openssl_path, "req", "-new", "-key", client_key, "-sha256", "-out",
         client_csr, "-subj", f"/CN={endpoint}")

    _run(openssl_path, "x509", "-req", "-in", client_csr, "-CA", root_cert,
            "-CAkey", root_key, "-CAcreateserial", "-out", client_cert, "-days",
            days, "-sha256", "-extfile", client_ext)

    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", server_key)
    _run(openssl_path, "req", "-new", "-key", server_key, "-sha256", "-out",
         server_csr, "-subj", f"/CN={server_name}")

    if server_cert_not_yet_valid:
        not_before = datetime.now(timezone.utc) + timedelta(days=1)
        not_after = not_before + timedelta(days=days)

        _sign_cert_with_ca_custom_validity(
            openssl_path,
            cert_dir,
            csr=server_csr,
            ca_cert=root_cert,
            ca_key=root_key,
            out_cert=server_cert,
            extfile=server_ext,
            not_before=not_before,
            not_after=not_after)
    else:
        _run(openssl_path, "x509", "-req", "-in", server_csr, "-CA", root_cert,
            "-CAkey", root_key, "-CAcreateserial", "-out", server_cert, "-days",
         days, "-sha256", "-extfile", server_ext)

    return CertificateCredentials(
        anjay_client_cert=client_cert,
        anjay_client_private_key=client_key,
        anjay_server_public_key=server_cert,
        dtls_server_cert_chain=server_cert,
        dtls_server_private_key=server_key,
        dtls_server_client_ca=root_cert)


def _get_credentials_dir(certificate_environment, directory_name):
    if directory_name is None:
        return certificate_environment.certificates_dir
    return Path(certificate_environment.certificates_dir) / directory_name


def generate_certificate_credentials(certificate_environment,
                                     *,
                                     directory_name=None,
                                     endpoint="test-endpoint",
                                     server_name="localhost",
                                     days=365,
                                     server_cert_not_yet_valid=False):
    # Generate credentials passed to Anjay Lite and the test server in PEM format.
    return _generate_pem_files(
        certificate_environment.openssl_path,
        _get_credentials_dir(certificate_environment, directory_name),
        endpoint=endpoint,
        server_name=server_name,
        days=days,
        server_cert_not_yet_valid=server_cert_not_yet_valid)


def _cert_pem_to_der(openssl_path, pem_path, der_path):
    _run(openssl_path, "x509", "-in", pem_path, "-outform", "DER", "-out",
         der_path)


def _key_pem_to_der(openssl_path, pem_path, der_path):
    _run(openssl_path, "pkcs8", "-topk8", "-in", pem_path, "-outform", "DER",
         "-nocrypt", "-out", der_path)


def convert_credentials_to_der(certificate_environment, credentials):
    # Return a copy of credentials for Anjay Lite inputs converted to DER.
    # The PEM files are still needed for the test server, so we keep them in the
    # returned structure as well.

    der_dir = Path(credentials.anjay_client_cert).parent / "der"
    der_dir.mkdir(parents=True, exist_ok=True)

    client_cert_der = der_dir / "client_cert.der"
    client_key_der = der_dir / "client_key.der"
    server_public_key_der = der_dir / "server_public_key.der"

    _cert_pem_to_der(
        certificate_environment.openssl_path,
        credentials.anjay_client_cert,
        client_cert_der)
    _key_pem_to_der(
        certificate_environment.openssl_path,
        credentials.anjay_client_private_key,
        client_key_der)
    _cert_pem_to_der(
        certificate_environment.openssl_path,
        credentials.anjay_server_public_key,
        server_public_key_der)

    return CertificateCredentials(
        anjay_client_cert=client_cert_der,
        anjay_client_private_key=client_key_der,
        anjay_server_public_key=server_public_key_der,
        dtls_server_cert_chain=credentials.dtls_server_cert_chain,
        dtls_server_private_key=credentials.dtls_server_private_key,
        dtls_server_client_ca=credentials.dtls_server_client_ca,
        server_leaf_cert=credentials.server_leaf_cert,
        root_ca_cert=credentials.root_ca_cert,
        intermediate_ca_cert=credentials.intermediate_ca_cert)


def generate_certificate_chain_credentials(certificate_environment,
                                           *,
                                           directory_name=None,
                                           endpoint="test-endpoint",
                                           server_name="localhost",
                                           days=365):
    cert_dir = _get_credentials_dir(certificate_environment, directory_name)
    cert_dir.mkdir(parents=True, exist_ok=True)

    openssl_path = certificate_environment.openssl_path

    root_key = cert_dir / "root_key.pem"
    root_csr = cert_dir / "root.csr"
    root_cert = cert_dir / "root_cert.pem"
    root_ext = cert_dir / "root.ext"

    intermediate_key = cert_dir / "intermediate_key.pem"
    intermediate_csr = cert_dir / "intermediate.csr"
    intermediate_cert = cert_dir / "intermediate_cert.pem"
    intermediate_ext = cert_dir / "intermediate.ext"

    client_key = cert_dir / "client_key.pem"
    client_csr = cert_dir / "client.csr"
    client_cert = cert_dir / "client_cert.pem"
    client_ext = cert_dir / "client.ext"

    server_key = cert_dir / "server_leaf_key.pem"
    server_csr = cert_dir / "server_leaf.csr"
    server_leaf_cert = cert_dir / "server_leaf_cert.pem"
    server_ext = cert_dir / "server.ext"
    server_chain = cert_dir / "server_chain.pem"

    _write(root_ext, """
        basicConstraints = critical, CA:TRUE
        keyUsage = critical, keyCertSign, cRLSign
    """)
    _write(intermediate_ext, """
        basicConstraints = critical, CA:TRUE, pathlen:0
        keyUsage = critical, keyCertSign, cRLSign
    """)
    _write(client_ext, """
        basicConstraints = critical, CA:FALSE
        keyUsage = critical, digitalSignature
        extendedKeyUsage = clientAuth
    """)
    _write(server_ext, f"""
        basicConstraints = critical, CA:FALSE
        keyUsage = critical, digitalSignature
        extendedKeyUsage = serverAuth
        subjectAltName = DNS:{server_name},IP:127.0.0.1
    """)

    # Root CA.
    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", root_key)
    _run(openssl_path, "req", "-new", "-key", root_key, "-sha256", "-out",
         root_csr, "-subj", "/CN=integration-test-root")
    _run(openssl_path, "x509", "-req", "-in", root_csr, "-signkey", root_key,
         "-out", root_cert, "-days", days, "-sha256", "-extfile", root_ext)

    # Intermediate CA signed by root.
    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", intermediate_key)
    _run(openssl_path, "req", "-new", "-key", intermediate_key, "-sha256",
         "-out", intermediate_csr, "-subj", "/CN=integration-test-intermediate")
    _run(openssl_path, "x509", "-req", "-in", intermediate_csr, "-CA",
         root_cert, "-CAkey", root_key, "-CAcreateserial", "-out",
         intermediate_cert, "-days", days, "-sha256", "-extfile",
         intermediate_ext)

    # Client certificate signed by root; used by Anjay Lite.
    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", client_key)
    _run(openssl_path, "req", "-new", "-key", client_key, "-sha256", "-out",
         client_csr, "-subj", f"/CN={endpoint}")
    _run(openssl_path, "x509", "-req", "-in", client_csr, "-CA", root_cert,
         "-CAkey", root_key, "-CAcreateserial", "-out", client_cert, "-days",
         days, "-sha256", "-extfile", client_ext)

    # Server leaf certificate signed by intermediate.
    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", server_key)
    _run(openssl_path, "req", "-new", "-key", server_key, "-sha256", "-out",
         server_csr, "-subj", f"/CN={server_name}")
    _run(openssl_path, "x509", "-req", "-in", server_csr, "-CA",
         intermediate_cert, "-CAkey", intermediate_key, "-CAcreateserial",
         "-out", server_leaf_cert, "-days", days, "-sha256", "-extfile",
         server_ext)

    # Chain presented by the Python DTLS server: leaf + intermediate + root.
    server_chain.write_bytes(
        server_leaf_cert.read_bytes()
        + intermediate_cert.read_bytes()
        + root_cert.read_bytes())

    return CertificateCredentials(
        anjay_client_cert=client_cert,
        anjay_client_private_key=client_key,
        anjay_server_public_key=server_leaf_cert,
        dtls_server_cert_chain=server_chain,
        dtls_server_private_key=server_key,
        dtls_server_client_ca=root_cert,
        server_leaf_cert=server_leaf_cert,
        root_ca_cert=root_cert,
        intermediate_ca_cert=intermediate_cert)


def generate_single_ca_server_certificate_credentials(certificate_environment,
                                                      *,
                                                      directory_name=None,
                                                      endpoint="test-endpoint",
                                                      server_name="localhost",
                                                      days=365):
    cert_dir = _get_credentials_dir(certificate_environment, directory_name)
    cert_dir.mkdir(parents=True, exist_ok=True)

    openssl_path = certificate_environment.openssl_path

    ca_key = cert_dir / "server_ca_key.pem"
    ca_csr = cert_dir / "server_ca.csr"
    ca_cert = cert_dir / "server_ca_cert.pem"
    ca_ext = cert_dir / "server_ca.ext"

    client_key = cert_dir / "client_key.pem"
    client_csr = cert_dir / "client.csr"
    client_cert = cert_dir / "client_cert.pem"
    client_ext = cert_dir / "client.ext"

    # This certificate is intentionally a CA certificate, but it also contains
    # fields that make it usable by the Python DTLS server as its endpoint
    # certificate. Anjay Lite shall still reject it for certificate_usage=2
    # because the peer end-entity certificate must not be the trust anchor.
    _write(ca_ext, f"""
        basicConstraints = critical, CA:TRUE
        keyUsage = critical, digitalSignature, keyCertSign, cRLSign
        extendedKeyUsage = serverAuth, clientAuth
        subjectAltName = DNS:{server_name},IP:127.0.0.1
    """)

    _write(client_ext, """
        basicConstraints = critical, CA:FALSE
        keyUsage = critical, digitalSignature
        extendedKeyUsage = clientAuth
    """)

    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", ca_key)
    _run(openssl_path, "req", "-new", "-key", ca_key, "-sha256", "-out",
         ca_csr, "-subj", f"/CN={server_name}")
    _run(openssl_path, "x509", "-req", "-in", ca_csr, "-signkey", ca_key,
         "-out", ca_cert, "-days", days, "-sha256", "-extfile", ca_ext)

    _run(openssl_path, "ecparam", "-name", "prime256v1", "-genkey", "-noout",
         "-out", client_key)
    _run(openssl_path, "req", "-new", "-key", client_key, "-sha256", "-out",
         client_csr, "-subj", f"/CN={endpoint}")
    _run(openssl_path, "x509", "-req", "-in", client_csr, "-CA", ca_cert,
         "-CAkey", ca_key, "-CAcreateserial", "-out", client_cert, "-days",
         days, "-sha256", "-extfile", client_ext)

    return CertificateCredentials(
        anjay_client_cert=client_cert,
        anjay_client_private_key=client_key,
        anjay_server_public_key=ca_cert,
        dtls_server_cert_chain=ca_cert,
        dtls_server_private_key=ca_key,
        dtls_server_client_ca=ca_cert)
