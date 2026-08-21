..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Crypto storage
==============

.. warning::
    This tutorial describes an experimental feature. The API might change
    in future releases without any notice.

Overview
--------

When security credentials are configured in the Security Object as in-memory
buffers, the certificate chains and private keys remain directly available in
process memory. This is convenient for examples and tests, but real products
often need stronger isolation. Private keys may need to be stored in a secure
element, HSM, or another protected storage backend instead of general-purpose
RAM or filesystem buffers.

Anjay Lite provides **external crypto storage** support for such integrations.
With this feature enabled, Security Object credentials may be represented as
**external identities**. The TLS integration layer then resolves those
identities into the actual certificate or key material when it needs them.

The external crypto storage API is declared in
``anj/compat/crypto/storage.h`` and is enabled with
``ANJ_WITH_EXTERNAL_CRYPTO_STORAGE``.

.. note::
   Code related to this tutorial can be found under
   `examples/tutorial/AT-CryptoStorage` in the Anjay Lite source directory.
   The example combines the connection flow from
   `AT-Certificates <AT-Certificates.html>`_,
   `AT-Bootstrap <AT-Bootstrap.html>`_ and
   `AT-Persistence <AT-Persistence.html>`_.


Running the example
-------------------

The example expects the following files in the current working directory:

- ``bootstrap_client_cert.der`` - client certificate used to connect to the
  Bootstrap Server,
- ``bootstrap_client_key.der`` - private key matching that certificate,
- ``bootstrap_server_cert.der`` - Bootstrap Server certificate used for server
  authentication.

Run the example with the endpoint name as an argument:

.. code-block:: bash

    ./anjay_lite_at_crypto_storage my-endpoint-name

After a successful Bootstrap, the client stores ``persistence_objs.bin`` together
with crypto ``crypto_record_*.dat`` storage records created by the default backend.
On the next startup, the application may restore previously provisioned server
configuration without repeating Bootstrap.


Why crypto storage matters
--------------------------

In production deployments, certificate handling is usually constrained by
security requirements rather than by convenience of the application code.
Typical requirements include:

- private keys shall not be kept in regular application memory longer than
  necessary,
- private keys shall not be exportable from secure hardware,
- device credentials provisioned through Bootstrap shall survive reboot,
- credentials shall be erasable when Security Object instances are deleted or
  reprovisioned.

Anjay Lite external crypto storage is designed to support such flows. The
default POSIX-backed implementation shown in this tutorial is intentionally
simple and useful as a **proof-of-concept** or test integration, but it is **not
production-ready**.

.. warning::
   The default crypto storage implementation stores credentials in regular
   files. This is suitable for development and demonstrations only. Real
   products should usually replace it with an integration backed by hardware
   security features such as a secure element or HSM.


Build-time configuration
------------------------

To use the tutorial example, the build needs certificates support, DTLS,
external crypto storage and persistence. The example also enables the default
POSIX crypto storage backend.

An example configuration can be found in
``examples/tutorial/AT-CryptoStorage/CMakeLists.txt``:

.. highlight:: cmake
.. snippet-source:: examples/tutorial/AT-CryptoStorage/CMakeLists.txt

    set(ANJ_WITH_MBEDTLS ON)
    set(ANJ_WITH_SECURITY ON)
    set(ANJ_NET_WITH_DTLS ON)

    set(ANJ_WITH_CERTIFICATES ON)
    set(ANJ_WITH_EXTERNAL_CRYPTO_STORAGE ON)
    set(ANJ_WITH_CRYPTO_STORAGE_DEFAULT ON)
    set(ANJ_WITH_PERSISTENCE ON)

    set(ANJ_MBEDTLS_ALLOWED_CIPHERSUITES "MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8")
    set(ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE 1500)
    set(ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE 1500)
    set(ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE 1500)

The most important flags are:

* ``ANJ_WITH_EXTERNAL_CRYPTO_STORAGE``:
  Enables the external crypto storage API.
* ``ANJ_WITH_CRYPTO_STORAGE_DEFAULT``:
  Enables the default POSIX/file-based implementation of that API.
* ``ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE``, ``ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE``, ``ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE``:
  Make sure these are adjusted to your fit keys and certificates sizes.


How credentials move through crypto storage
-------------------------------------------

When external crypto storage is enabled, Anjay Lite may move credentials out of
Security Object buffers in two main situations:

1. During ``anj_dm_security_obj_install()``, if the data source is set to
   ``ANJ_CRYPTO_DATA_SOURCE_BUFFER``.
2. After ``Bootstrap-Finish``, once the Bootstrap-written Security Object data
   has been validated.

This behavior is particularly important for Bootstrap flows. Credentials
received through Bootstrap Write initially are stored in the Security Object
internal buffers. After Bootstrap completes, Anjay Lite calls the crypto storage
offload logic so that the provisioned records can be moved into the storage
backend.

If object persistence is enabled, the Security Object does not need to store raw
certificate bytes in the persistence stream for external records. Instead, the
crypto storage backend serializes a small piece of persistence metadata that can
later be resolved back into the external identity.

The tutorial example demonstrates exactly this pattern:

- on first startup, the client uses filesystem paths as external identities for
  the Bootstrap Server certificate, device private key and device public certificate,
- after a successful Bootstrap, provisioned LwM2M Server credentials are
  offloaded into crypto storage,
- the Security and Server Objects are stored to persistence,
- on the next startup, the objects are restored from persistence and the
  restored external identities point to the already stored crypto records.


Using filesystem paths as external identities
---------------------------------------------

The example starts from certificate-based Bootstrap credentials that are not
loaded into memory buffers. Instead, it passes file paths as external crypto
identities, similarly to ``security_info_from_path()`` used in the integration
test application.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-CryptoStorage/src/main.c
    :emphasize-lines: 4-5

    static void security_info_from_path(anj_crypto_security_info_t *out_info,
                                        const char *path) {
        memset(out_info, 0, sizeof(*out_info));
        out_info->source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL;
        out_info->info.external.identity = path;
    }

In the tutorial example, these identities are plain paths to DER files used for
the Bootstrap Server account:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-CryptoStorage/src/main.c

    #define BOOTSTRAP_CLIENT_CERT_PATH "bootstrap_client_cert.der"
    #define BOOTSTRAP_CLIENT_KEY_PATH "bootstrap_client_key.der"
    #define BOOTSTRAP_SERVER_CERT_PATH "bootstrap_server_cert.der"

This is enough for the default POSIX crypto storage backend, because it treats
external identities as filesystem paths when resolving data on demand.


Bootstrap account configuration
-------------------------------

The Security Object installation helper builds a Bootstrap Server account using
certificate mode, but the credentials are provided as external identities
instead of in-memory buffers:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-CryptoStorage/src/main.c
    :emphasize-lines: 9-11,17-19

    static int install_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj,
                                    bool initial_bootstrap_configuration) {
        if (initial_bootstrap_configuration) {
            anj_crypto_security_info_t client_cert;
            anj_crypto_security_info_t client_key;
            anj_crypto_security_info_t server_cert;

            security_info_from_path(&client_cert, BOOTSTRAP_CLIENT_CERT_PATH);
            security_info_from_path(&client_key, BOOTSTRAP_CLIENT_KEY_PATH);
            security_info_from_path(&server_cert, BOOTSTRAP_SERVER_CERT_PATH);

            anj_dm_security_instance_init_t security_inst = {
                .server_uri = "coaps://eu.iot.avsystem.cloud:5694",
                .bootstrap_server = true,
                .security_mode = ANJ_DM_SECURITY_CERTIFICATE,
                .public_key_or_identity = client_cert,
                .secret_key = client_key,
                .server_public_key = server_cert
            };
            if (anj_dm_security_obj_add_instance(security_obj, &security_inst)) {
                log(L_ERROR, "Failed to add Bootstrap Server security instance");
                return -1;
            }
            log(L_INFO,
                "Using bootstrap credentials from file paths: '%s', '%s', '%s'",
                BOOTSTRAP_CLIENT_CERT_PATH, BOOTSTRAP_CLIENT_KEY_PATH,
                BOOTSTRAP_SERVER_CERT_PATH);
        }

        return anj_dm_security_obj_install(anj, security_obj);
    }

This differs from :doc:`AT-Certificates`, where the example uses
``ANJ_CRYPTO_DATA_SOURCE_BUFFER`` and copies file contents into local arrays
before passing them to the Security Object.


Persistence-aware bootstrap flow
--------------------------------

The example restores Security and Server Objects from persistence on startup.
If restoration fails or persistence files are missing, it falls back to the
default certificate-based Bootstrap account.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-CryptoStorage/src/main.c

    anj_dm_security_obj_init(&security_obj);
    anj_dm_server_obj_init(&server_obj);

    bool initial_bootstrap_configuration = true;
    FILE *file = fopen(PERSISTENCE_OBJS_FILE, "r");
    if (file) {
        anj_persistence_context_t ctx =
                anj_persistence_restore_context_create(persistence_read, file);
        if (!restore_security_obj(&anj, &security_obj, &ctx)
                && !restore_server_obj(&server_obj, &ctx)) {
            initial_bootstrap_configuration = false;
        } else {
            // If either restore fails, discard the partially restored state and
            // rebuild the default bootstrap account instead.
            anj_dm_security_obj_init(&security_obj);
            anj_dm_server_obj_init(&server_obj);
            remove(PERSISTENCE_OBJS_FILE);
        }
        fclose(file);
    }

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj,
                                    initial_bootstrap_configuration)
            || install_server_obj(&anj, &server_obj)) {
        return -1;
    }

After Bootstrap succeeds, the current Security and Server Objects are stored.
This keeps the bootstrapped server definition together with the external crypto
identities created during offload.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-CryptoStorage/src/main.c

    typedef struct {
        anj_dm_server_obj_t *server_obj;
        anj_dm_security_obj_t *security_obj;
    } persistent_objects_t;

    static void store_persistent_objects(anj_t *anj,
                                         persistent_objects_t *persistent_objects) {
        FILE *file = fopen(PERSISTENCE_OBJS_FILE, "w+");
        if (!file) {
            log(L_ERROR, "Could not open persistence file for writing");
            return;
        }

        anj_persistence_context_t persistence_ctx =
                anj_persistence_store_context_create(persistence_write, file);
        // Storing both objects together keeps the Security/Server instance pairing
        // consistent across restarts.
        if (anj_dm_security_obj_store(anj, persistent_objects->security_obj,
                                      &persistence_ctx)
                || anj_dm_server_obj_store(persistent_objects->server_obj,
                                           &persistence_ctx)) {
            log(L_ERROR, "Could not store persistent objects");
            fclose(file);
            remove(PERSISTENCE_OBJS_FILE);
            return;
        }

        fclose(file);
        log(L_INFO, "Persistent objects stored");
    }

    static void connection_status_callback(void *arg,
                                           anj_t *anj,
                                           anj_conn_status_t conn_status) {
        if (conn_status == ANJ_CONN_STATUS_BOOTSTRAPPED) {
            log(L_INFO,
                "Bootstrap finished successfully; storing bootstrapped objects");
            store_persistent_objects(anj, (persistent_objects_t *) arg);
        }
    }

In this setup, the persistence file stores the Security and Server Objects,
while the crypto storage backend keeps the credential records themselves.


How the default crypto storage implementation works
---------------------------------------------------

The default implementation included in Anjay Lite is POSIX-specific and file
based. It creates files named ``crypto_record_*.dat`` and uses them as storage
records for certificates and keys that were offloaded from Security Object
buffers.

For the example in this tutorial, this means:

- the initial Bootstrap Server credentials are referenced directly by
  ``bootstrap_client_cert.der``, ``bootstrap_client_key.der`` and
  ``bootstrap_server_cert.der``,
- credentials provisioned later through Bootstrap Write are stored by the
  default backend in ``crypto_record_*.dat`` files,
- Security Object persistence stores only the external identities needed to
  find those records later.

When the TLS backend needs the bytes for a certificate or key, it asks the
crypto storage layer to resolve the identity into actual data.


Record lifecycle
----------------

The storage backend is expected to remove records when they are no longer owned
by the Security Object. In the default Security Object implementation this
happens when a Security Object instance is deleted, for example due to
Bootstrap-Delete or another operation that removes the instance.

It is also possible that a provisioning flow overwrites credentials by creating
new records and then persisting the updated configuration. In such cases, the
old records should not remain as stale secrets in the storage backend.

The example itself does not implement a custom delete policy, because it relies
on the default Security Object and the default crypto storage backend.


Production integrations
-----------------------

For real deployments, crypto storage is typically backed by dedicated hardware.
A secure element or HSM may be used to:

- keep private keys non-exportable,
- perform signing operations without exposing private key material,
- generate private keys directly inside hardware,
- provide a source of random numbers.

.. warning::
    HSM operations are blocking. When Anjay Lite creates or deletes data records
    on an external crypto storage module, the ``anj_core_step()`` function will
    not return untill the operation is finished.

AVSystem maintains a
:ref:`bare-metal client integration <bare-metal-client-integration>`
using an NXP secure element. That integration supports private-key signing
without extracting the private key from the secure element, using
``ANJ_WITH_EXTERNAL_SIGNING``.

.. important::
    HSM integration in Anjay Lite Bare Metal Client does not support RSA keys.

Such deployments usually replace the default POSIX crypto storage completely.
Instead of storing files on disk, the external identity may refer to a
hardware-specific key slot, object identifier, certificate handle or another
backend-defined record descriptor.

.. important::
    If you want to see a production-ready HSM or secure-element integration,
    contact AVSystem. For commercial support options, see
    :doc:`/CommercialFeatures`.
