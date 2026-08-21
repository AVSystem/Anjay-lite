..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

DTLS connection using certificates
==================================

In :doc:`/BasicClient/BC-Security` section you learned how to configure a
secure DTLS connection using Pre-Shared Keys (PSK). In this section, we will
show how to use X.509 certificates instead.

Using certificates in Anjay Lite follows a similar pattern to PSK-based
security. The main difference is that instead of providing raw key material
directly in the configuration, certificates and private key data are typically
loaded from files and passed to the Security Object as buffers.

.. note::
   Code related to this tutorial can be found under `examples/tutorial/AT-Certificates`
   in the Anjay Lite source directory and is based on `examples/tutorial/BC-Security`
   example.


Prerequisites & build flags
---------------------------

To enable secure communication using certificates, it is necessary to configure
appropriate build-time flags. These can be set either via the CMake build system
or by providing a custom ``anjay_config.h`` file.

An example configuration that enables support for EC-based certificates can be
found in ``examples/tutorial/AT-Certificates/CMakeLists.txt``:

.. highlight:: cmake
.. snippet-source:: examples/tutorial/AT-Certificates/CMakeLists.txt

    set(ANJ_WITH_CERTIFICATES ON)
    set(ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE 500)
    set(ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE 500)
    set(ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE 1000)

The flags shown above have the following meaning:

* ``ANJ_WITH_CERTIFICATES``:
  Enable support for certificates in Anjay Lite.
* ``ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE``:
  Configures the size of the buffer that holds Public Key or Identity.
* ``ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE``:
  Configures the size of the buffer that holds Secret Key.
* ``ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE``:
  Configures the size of the buffer that holds Server Public Key.

.. note::
   For more information about these flags, see the ``anjay_config.h.in`` file.

.. note::
   ``ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP`` may be enabled to allow
   certificate-based connections without configuring Server Public Key
   (``/0/x/4``). This disables server certificate verification and is intended
   only for tests. The option is disabled by default.

.. note::
   The set of supported certificate types and ciphersuites depends on the
   underlying Mbed TLS configuration. For example, RSA-based certificates can
   be used if corresponding ciphersuites and features are enabled in Mbed TLS.

   Allowed ciphersuites may be adjusted using
   ``ANJ_MBEDTLS_ALLOWED_CERT_CIPHERSUITES`` CMake variable which defines list
   of ciphersuites that Anjay Lite will allow when using certificates mode.
   See ``anjay_config.h.in`` for more details.

Loading certificate data from files
-----------------------------------

Since Anjay Lite does not perform certificate parsing on its own, the data must
be provided in a format supported by the underlying TLS backend (typically DER).
A helper function can be used to read the contents of certificate files into
memory buffers:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Certificates/src/main.c

    static int read_file_into_buffer(const char *path,
                                     uint8_t *out_buf,
                                     size_t out_buf_capacity,
                                     size_t *out_size) {
        FILE *f = fopen(path, "rb");
        if (!f) {
            log(L_ERROR, "Failed to open %s: %s", path, strerror(errno));
            char cwd[500];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                log(L_INFO, "Current working dir: %s", cwd);
            }
            return -1;
        }
        size_t total = fread(out_buf, 1, out_buf_capacity, f);
        if (ferror(f)) {
            log(L_ERROR, "Failed to read %s", path);
            fclose(f);
            return -1;
        }
        // If the buffer is filled, ensure the file does not contain more data
        if (total == out_buf_capacity) {
            int c = fgetc(f);
            if (c != EOF) {
                log(L_ERROR, "%s too large (>%lu bytes)", path,
                    (unsigned long) out_buf_capacity);
                fclose(f);
                return -1;
            }
        }
        fclose(f);
        *out_size = total;
        return 0;
    }

This function reads the entire contents of a file into a user-provided buffer
and returns its size. It also ensures that the file fits within the provided
buffer capacity.

Configuring the Security Object
-------------------------------

Once the certificates and private key are loaded into memory, they can be used
to configure the Security Object instance.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Certificates/src/main.c
    :emphasize-lines: 3-17,24-38

    static int install_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj) {
        uint8_t client_cert_der[ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE];
        uint8_t client_key_der[ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE];
        uint8_t server_cert_der[ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE];
        size_t client_cert_der_size = 0;
        size_t client_key_der_size = 0;
        size_t server_cert_der_size = 0;

        if (read_file_into_buffer("client_cert.der", client_cert_der,
                                sizeof(client_cert_der), &client_cert_der_size)
                || read_file_into_buffer("client_key.der", client_key_der,
                                        sizeof(client_key_der),
                                        &client_key_der_size)
                || read_file_into_buffer("server_cert.der", server_cert_der,
                                        sizeof(server_cert_der),
                                        &server_cert_der_size)) {
            return -1;
        }

        anj_dm_security_instance_init_t security_inst = {
            .ssid = 1,
            .server_uri = "coaps://eu.iot.avsystem.cloud:5684",
            .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
            .public_key_or_identity = {
                .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
                .info.buffer.data = client_cert_der,
                .info.buffer.data_size = client_cert_der_size
            },
            .secret_key = {
                .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
                .info.buffer.data = client_key_der,
                .info.buffer.data_size = client_key_der_size
            },
            .server_public_key = {
                .source = ANJ_CRYPTO_DATA_SOURCE_BUFFER,
                .info.buffer.data = server_cert_der,
                .info.buffer.data_size = server_cert_der_size
            },
        };
        anj_dm_security_obj_init(security_obj);
        if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
                || anj_dm_security_obj_install(anj, security_obj)) {
            return -1;
        }
        return 0;
    }

In this configuration:

- ``security_mode`` is set to ``ANJ_DM_SECURITY_CERTIFICATE`` to enable
  certificate-based DTLS.
- ``public_key_or_identity`` contains the client certificate.
- ``secret_key`` contains the corresponding private key.
- ``server_public_key`` contains the server certificate used for server
  authentication.
- All certificates and keys are provided as in-memory buffers using
  ``ANJ_CRYPTO_DATA_SOURCE_BUFFER``.

.. note::
   The buffers containing the certificates and keys passed to
   ``anj_dm_security_obj_add_instance()`` are copied internally.

.. note::
   If the ``certificate_usage`` field is not explicitly configured in the
   Security Object instance, Anjay Lite uses the default value ``3``
   (domain-issued certificate). In this mode, the certificate configured as
   ``server_public_key`` is matched directly against the server end-entity
   certificate presented during the DTLS handshake.

   The server may still send a full certificate chain, but the configured
   ``server_public_key`` shall contain only the server leaf certificate.

   For more information on certificate usage types see :doc:`AT-CertificateUsage`.

With the Security Object configured this way, the client will use the provided
certificates and private key to authenticate itself during the DTLS handshake
with the LwM2M server and will also attempt to validate the server's identity
using the provided server certificate.

.. warning::
   Certificate-based DTLS connections require Server Public Key (``/0/x/4``)
   to be configured by default. This requirement is checked when establishing
   the DTLS connection.

   It is possible to opt out of this requirement by enabling
   ``ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP`` at build time. In that mode,
   Anjay Lite accepts a missing Server Public Key during connection setup and
   disables server certificate verification. This makes the connection vulnerable
   to man-in-the-middle attacks and must not be used in production environments.


Server Name Indication
----------------------

The LwM2M Security Object defines resource ``/0/x/14`` (Server Name Indication, SNI),
which may be used to explicitly configure the hostname sent during the DTLS
handshake.

In Anjay Lite, the SNI TLS extension is **always sent** during the handshake.
Its value is determined as follows:

- If SNI is explicitly provided in the Security Object instance (as
  ``server_name_indication``), it is used.
- Otherwise, the hostname part of ``server_uri`` is used as SNI.

SNI is particularly important when connecting to servers that host multiple
virtual endpoints (e.g., cloud platforms), as it allows the server to select
the correct certificate and configuration.

Running the example
-------------------

The example expects the following files to be present in the current working
directory (i.e., the directory from which the application is launched):

- ``client_cert.der`` – the client certificate in DER format
- ``client_key.der`` – the corresponding private key in DER format
- ``server_cert.der`` – the server certificate in DER format

If these files are not found, the application will fail to start due to errors
when attempting to load the credentials.

You can generate an EC private key and a self-signed certificate for running this
example using ``openssl``:

.. code-block:: bash

    # Generate EC private key using the prime256v1 curve
    openssl ecparam -name prime256v1 -genkey -noout -outform der -out client_key.der

    # Generate a self-signed certificate using that key
    openssl req -x509 -new -key client_key.der -inform der \
        -out client_cert.der -outform der -days 365 \
        -subj "/CN=my-endpoint-name"

.. note::
   When connecting a device to Coiote DM, the endpoint name passed to the
   application (e.g., ``my-endpoint-name``) **must match** the Common Name (CN)
   used when generating the certificate. The Coiote DM server validates that
   these values match.

The server certificate can be obtained directly from the server using
``openssl s_client``. The command below extracts the first certificate from the
server-provided certificate chain, i.e. the leaf certificate. This is important
for the default certificate usage ``3``, because Anjay Lite expects
``server_public_key`` to contain the exact server leaf certificate. The command
then converts it to DER format expected by the example:

.. code-block:: bash

    openssl s_client -dtls -connect eu.iot.avsystem.cloud:5684 -showcerts </dev/null 2>/dev/null \
        | awk '
            /-----BEGIN CERTIFICATE-----/ { cert++ }
            cert == 1 { print }
            /-----END CERTIFICATE-----/ && cert == 1 { exit }
          ' > server_cert.pem

    openssl x509 -in server_cert.pem -outform DER -out server_cert.der

Because self-signed certificates are used, the LwM2M server trust store must be
updated to include the device's certificate before attempting to connect. Once
the certificate has been uploaded to the server, you can run the example by
executing the application with the endpoint name as an argument, for example:

.. code-block:: bash

    ./anjay_lite_cert_example my-endpoint-name

Make sure to run the command from the directory that contains the
``client_cert.der``, ``client_key.der`` and ``server_cert.der`` files,
or adjust the file paths in the source code accordingly.
