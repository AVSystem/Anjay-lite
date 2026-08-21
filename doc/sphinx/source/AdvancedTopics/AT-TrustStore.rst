..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

DTLS connection using a trust store
===================================

:doc:`AT-Certificates` uses the simplest server certificate verification mode.
This tutorial uses a stricter mode that combines two checks:

* the server certificate configured in the Security Object must match the leaf
  certificate presented by the server, as in the previous tutorial,
* the certificate chain presented by the server must also be verifiable using a
  CA certificate provided in the trust store.

.. note::
   Code related to this tutorial can be found under
   `examples/tutorial/AT-TrustStore` in the Anjay Lite source directory.


Prerequisites & build flags
---------------------------

The example uses certificate-based DTLS and external crypto storage.
External crypto storage allows the Security Object credentials to be represented
as external identities. The TLS integration layer then resolves those identities
into the actual certificate or key material when it needs them. For details, see
:doc:`AT-CryptoStorage`.

The required build-time configuration can be found in
``examples/tutorial/AT-TrustStore/CMakeLists.txt``:

.. highlight:: cmake
.. snippet-source:: examples/tutorial/AT-TrustStore/CMakeLists.txt

    set(ANJ_WITH_CERTIFICATES ON)
    set(ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE 1500)
    set(ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE 1500)
    set(ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE 1500)

    set(ANJ_WITH_EXTERNAL_CRYPTO_STORAGE ON)
    set(ANJ_WITH_CRYPTO_STORAGE_DEFAULT ON)

The flags shown above have the following meaning:

* ``ANJ_WITH_CERTIFICATES``:
  Enable support for certificates in Anjay Lite.
* ``ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE``:
  Configures the size of the buffer that holds Public Key or Identity.
* ``ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE``:
  Configures the size of the buffer that holds Secret Key.
* ``ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE``:
  Configures the size of the buffer that holds Server Public Key.
* ``ANJ_WITH_EXTERNAL_CRYPTO_STORAGE``:
  Enables the external crypto storage API.
* ``ANJ_WITH_CRYPTO_STORAGE_DEFAULT``:
  Enables the default POSIX/file-based implementation of that API.


External credential identities
------------------------------

The example stores the file names in ``anj_crypto_security_info_t`` structures
as external identities:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TrustStore/src/main.c
    :emphasize-lines: 4-5

    static void security_info_from_path(anj_crypto_security_info_t *out_info,
                                        const char *path) {
        memset(out_info, 0, sizeof(*out_info));
        out_info->source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL;
        out_info->info.external.identity = path;
    }

With the default POSIX crypto storage backend, these identities are plain file
paths resolved when the TLS backend needs the certificate or key material. For a
more complete explanation of this mechanism, see :doc:`AT-CryptoStorage`.


Configuring the Security Object
-------------------------------

The Security Object still contains the credentials specific to the LwM2M Server
account. In this example these are the client certificate, the matching private
key and the server leaf certificate:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TrustStore/src/main.c
    :emphasize-lines: 7-9,14,20-23

    static int install_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj) {
        anj_crypto_security_info_t client_cert;
        anj_crypto_security_info_t client_key;
        anj_crypto_security_info_t server_cert;
        static anj_net_certificate_usage_t certificate_usage =
                ANJ_NET_CERTIFICATE_SERVICE_CERTIFICATE_CONSTRAINT;

        security_info_from_path(&client_cert, CLIENT_CERT_PATH);
        security_info_from_path(&client_key, CLIENT_KEY_PATH);
        security_info_from_path(&server_cert, SERVER_LEAF_CERT_PATH);

        anj_dm_security_instance_init_t security_inst = {
            .ssid = 1,
            .server_uri = "coaps://eu.iot.avsystem.cloud:5684",
            .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
            .public_key_or_identity = client_cert,
            .secret_key = client_key,
            .server_public_key = server_cert,
            .certificate_usage = &certificate_usage
        };
        anj_dm_security_obj_init(security_obj);
        if (anj_dm_security_obj_add_instance(security_obj, &security_inst)
                || anj_dm_security_obj_install(anj, security_obj)) {
            return -1;
        }

        // ...
        return 0;
    }

The important difference from the basic certificate tutorial is
``certificate_usage``. ``ANJ_NET_CERTIFICATE_SERVICE_CERTIFICATE_CONSTRAINT``
means that the server certificate configured as ``server_public_key`` identifies
the expected service certificate, while the chain is validated using the global
trust store.

In other words, ``server_public_key`` still constrains the exact server leaf
certificate, while the trust store supplies the CA certificates used for chain
validation. This is why the example uses both ``leaf_cert.pem`` and
``ca_cert.pem``.

.. note::
    For more information on certificate usage types see :doc:`AT-CertificateUsage`.


Configuring the trust store
---------------------------

The trust store is configured in ``anj_configuration_t`` before
``anj_core_init()``. It is global for the client instance and contains CA
certificates used during certificate-chain validation:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TrustStore/src/main.c
    :emphasize-lines: 7-13,17

    int main(int argc, char *argv[]) {
        // ...
        anj_t anj;
        anj_dm_device_obj_t device_obj;
        anj_dm_server_obj_t server_obj;
        anj_dm_security_obj_t security_obj;
        anj_crypto_security_info_t trust_store_ca[1];

        security_info_from_path(&trust_store_ca[0], TRUST_STORE_CA_PATH);
        anj_net_trust_store_t trust_store = {
            .ca_certs = trust_store_ca,
            .ca_certs_count = 1
        };

        anj_configuration_t config = {
            .endpoint_name = argv[1],
            .trust_store = &trust_store
        };
        if (anj_core_init(&anj, &config)) {
            log(L_ERROR, "Failed to initialize Anjay Lite");
            return -1;
        }

        // ...
    }

``ca_certs`` points to an array of trust anchors. This tutorial uses a single
CA certificate, but additional CA certificates can be added by extending the
array and updating ``ca_certs_count``.


Running the example
-------------------

The example expects the following files to be present in the current working
directory (i.e., the directory from which the application is launched):

* ``client_cert.der`` - client certificate in DER format,
* ``client_key.der`` - corresponding private key in DER format,
* ``leaf_cert.pem`` - server leaf certificate in PEM format,
* ``ca_cert.pem`` - CA certificate in PEM format, used to verify the
  certificate chain sent by the server.

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
server-provided certificate chain, i.e. the leaf certificate, into
``leaf_cert.pem``.

.. code-block:: bash

    openssl s_client -dtls -connect eu.iot.avsystem.cloud:5684 -showcerts </dev/null 2>/dev/null \
        | awk '
            /-----BEGIN CERTIFICATE-----/ { cert++ }
            cert == 1 { print }
            /-----END CERTIFICATE-----/ && cert == 1 { exit }
          ' > leaf_cert.pem

To populate the trust store for this example, the last certificate from the
chain sent by the server during the DTLS handshake can be extracted and saved as
``ca_cert.pem`` using the command below:

.. code-block:: bash

    openssl s_client -dtls -connect eu.iot.avsystem.cloud:5684 -showcerts </dev/null 2>/dev/null \
        | awk '
            /-----BEGIN CERTIFICATE-----/ { cert = ""; in_cert = 1 }
            in_cert { cert = cert $0 "\n" }
            /-----END CERTIFICATE-----/ { in_cert = 0; last = cert }
            END { printf "%s", last }
          ' > ca_cert.pem

.. warning::
   For this tutorial, the last certificate from the server-provided chain is
   used as the trust store certificate. In a production environment, the trust
   store should be provisioned with CA certificates that are explicitly trusted
   by the device operator and expected to remain valid throughout the device
   lifetime. The certificate chain sent by the server may change, for example
   when server certificates are renewed or the server configuration is updated.

Because self-signed certificates are used, the LwM2M server trust store must be
updated to include the device's certificate before attempting to connect. Once
the certificate has been uploaded to the server, you can run the example by
executing the application with the endpoint name as an argument, for example:

.. code-block:: bash

    ./anjay_lite_at_trust_store my-endpoint-name

Make sure to run the command from the directory that contains the
``client_cert.der``, ``client_key.der``, ``leaf_cert.pem`` and ``ca_cert.pem``
files, or adjust the file paths in the source code accordingly.
