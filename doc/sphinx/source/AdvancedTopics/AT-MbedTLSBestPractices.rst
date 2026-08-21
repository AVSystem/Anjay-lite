..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Using Mbed TLS with Anjay Lite: configuration and troubleshooting
=================================================================

This article collects practical information and recommendations for using
Mbed TLS as the DTLS backend for Anjay Lite. It focuses on configuration issues
commonly encountered when integrating Anjay Lite with Mbed TLS on embedded platforms.

This article does not replace the general Anjay Lite security tutorials. If you are
looking for a complete introduction to the Security Object, PSK mode or
certificate mode, see :doc:`/BasicClient/BC-Security`, :doc:`AT-Certificates`
and :doc:`AT-CertificateUsage` first.

For Mbed TLS-specific topics that are not directly related to Anjay Lite, such as
footprint reduction, platform integration or low-level TLS configuration, also
refer to the official Mbed TLS knowledge base and tutorials.

Best practices
--------------

Connection ID
^^^^^^^^^^^^^

When using DTLS, consider enabling Connection ID (CID) if the server supports it.
CID allows the server to associate packets with an existing DTLS connection even
if the client's network endpoint as seen by the server changes. This is often
caused by NAT mapping expiration, which may change the source port or address
observed by the server, and does not necessarily require the device to reconnect
to a different network.

This functionality is controlled at build time. Enable the
``MBEDTLS_SSL_DTLS_CONNECTION_ID`` option in the Mbed TLS configuration. When
Anjay Lite is built with ``ANJ_WITH_MBEDTLS`` and ``ANJ_NET_WITH_DTLS``, the
default Mbed TLS DTLS socket integration enables CID automatically if this Mbed
TLS option is available.

If CID enabling succeeds, Anjay Lite will print a message like the following during the DTLS handshake:

.. code-block:: none

    INFO [mbedtls] [../anj_mbedtls_dtls_socket.c]: negotiated CID = 0011223344556677

RNG source
^^^^^^^^^^

Use a strong entropy source for the random number generator. Mbed TLS requires
a random number generator during DTLS handshakes. In Anjay Lite, the default
Mbed TLS integration installs an RNG callback that calls
``anj_rng_generate()`` declared in ``<anj/compat/rng.h>``.

The platform or application must provide ``anj_rng_generate()`` unless the
POSIX compatibility implementation is used. In the CMake-based configuration,
``ANJ_WITH_RNG_POSIX_COMPAT`` enables an implementation based on
``getentropy()``. For embedded or bare-metal targets, implement this hook using
a cryptographically secure source.

For more details about implementing this platform hook, see
:doc:`/PortingGuideForNonPOSIXPlatforms/RandomNumberGeneratorAPI`.

.. warning::

   When secure connections are enabled, ``anj_rng_generate()`` must return
   cryptographically secure random bytes. A weak PRNG or deterministic test
   implementation makes DTLS security undefined and must not be used in
   production deployments.

Buffer and credential size limits
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When reducing the Mbed TLS footprint, make sure that size-related configuration
options are still large enough for the credentials used by the deployment. This
is especially important for certificate-based security mode. The
``MBEDTLS_SSL_IN_CONTENT_LEN`` and ``MBEDTLS_SSL_OUT_CONTENT_LEN`` options define
the sizes of internal incoming and outgoing message buffers. They must be large
enough to receive and send the largest handshake messages expected during
connection setup, including certificate chains. If these buffers are reduced too
aggressively, certificate-based DTLS/TLS handshakes may fail.

The same applies to PSK-based deployments. Mbed TLS limits the maximum accepted
PSK size using ``MBEDTLS_PSK_MAX_LEN``. If the PSK configured in the Security
Object is longer than this limit, Mbed TLS will not be able to use it and the
connection will fail during setup.

Also verify Anjay Lite's own credential-related limits. Depending on the
security mode and credential storage method, the relevant build-time options
include ``ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE``,
``ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE``, ``ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE``,
``ANJ_SEC_OBJ_MAX_SNI_SIZE`` and ``ANJ_MBEDTLS_MAX_TRUST_STORE_CERTIFICATE_SIZE``.
Configure them so that the largest credentials provisioned in the Security Object
or trust store fit in the buffers used by the application and the Mbed TLS
compatibility layer.

Cipher Suites
^^^^^^^^^^^^^

The list of cipher suites used during the DTLS handshake is configured at build
time. The default Mbed TLS DTLS socket integration uses:

- ``ANJ_MBEDTLS_ALLOWED_PSK_CIPHERSUITES`` for PSK mode,
- ``ANJ_MBEDTLS_ALLOWED_CERT_CIPHERSUITES`` for certificate mode.

When using CMake, set the corresponding CMake variables. When using a custom
``anjay_config.h`` file, define these macros directly. Their values are
comma-separated lists of Mbed TLS cipher suite identifiers, for example
``MBEDTLS_TLS_PSK_WITH_AES_128_CCM_8``. The selected cipher suites are passed to
``mbedtls_ssl_conf_ciphersuites()`` and must also be supported by the Mbed TLS
configuration and by the target LwM2M Server.

As a best practice, configure only cipher suites that are considered secure and
are required by the target LwM2M Server. Avoid offering every cipher suite
supported by the Mbed TLS build. During the handshake, the server selects one of
the cipher suites offered by the client, so offering weak or legacy cipher
suites may allow the connection to use weaker cryptography than intended.

The same rule should be applied at the Mbed TLS configuration level. Remove
unwanted cipher suites from the Mbed TLS build to reduce code size and to make
sure that they cannot be negotiated accidentally. When enabling a cipher suite,
also enable all cryptographic primitives it depends on, such as the key exchange
method, cipher algorithm, hash/MAC algorithm and required elliptic curves.

Considerations
--------------

Server Name Indication
^^^^^^^^^^^^^^^^^^^^^^

The LwM2M Security Object defines resource ``/0/x/14`` (Server Name Indication, SNI),
which may be used to explicitly configure the hostname sent during the DTLS
Client Hello message in the Server Name Indication extension field.

Anjay Lite passes the selected server name to Mbed TLS using
``mbedtls_ssl_set_hostname()`` when X.509 parsing support is available
(``MBEDTLS_X509_CRT_PARSE_C``). If SNI support is enabled in Mbed TLS
(``MBEDTLS_SSL_SERVER_NAME_INDICATION``), Mbed TLS includes this name in the
Client Hello as the Server Name Indication extension.

The value is determined as follows:

- If SNI is explicitly provided in the Security Object instance (as
  ``anj_dm_security_instance_init_t::server_name_indication`` when using the
  default Security Object), it is used.
- Otherwise, the hostname part of
  ``anj_dm_security_instance_init_t::server_uri`` is used.

SNI is particularly important when connecting to servers that host multiple
virtual endpoints (e.g., cloud platforms), as it allows the server to select
the correct certificate and configuration. In certificate mode, the SNI value
is also relevant for hostname verification, so it needs to match the Subject
Alternative Name (SAN), or Common Name (CN) if applicable, in the server certificate.

Certificate time validation
^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the platform provides a valid real-time clock or another reliable source of
current time, enable ``MBEDTLS_HAVE_TIME_DATE`` in the Mbed TLS configuration.
This allows Mbed TLS to validate the ``notBefore`` and ``notAfter`` fields of
X.509 certificates during certificate verification.

On platforms without reliable timekeeping, certificate-based security requires
additional design decisions. Disabling ``MBEDTLS_HAVE_TIME_DATE`` may make DTLS
handshakes succeed, but it also prevents Mbed TLS from rejecting expired or
not-yet-valid certificates. Treat this as a security trade-off, not as a generic
workaround for handshake failures.

Certificate verification model
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When certificate mode is used, pay attention to the Certificate Usage setting.
Its detailed behavior is described in :doc:`AT-CertificateUsage`, but from the
Mbed TLS integration perspective the most important point is whether PKIX
verification is required and where the trust anchors come from.

Certificate Usage ``0`` (``ANJ_NET_CERTIFICATE_CA_CONSTRAINT``) and Certificate
Usage ``1`` (``ANJ_NET_CERTIFICATE_SERVICE_CERTIFICATE_CONSTRAINT``) require a
trust store. In Anjay Lite, this means providing trusted certificates through
``anj_configuration_t::trust_store`` before calling ``anj_core_init()``. There
is no default Mbed TLS integration setting that automatically enables an
operating-system trust store.

If no Certificate Usage value is configured in the default Security Object,
Anjay Lite uses ``ANJ_NET_CERTIFICATE_DOMAIN_ISSUED_CERTIFICATE``.

.. warning::

   By default, certificate-based DTLS connections require the Server Public Key
   resource (``/0/x/4``) to be configured. Missing server certificate material is
   treated as a configuration error. Enabling
   ``ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP`` disables this requirement and
   disables server certificate verification for such connections; this is
   insecure and must only be used in tests.

Mbed TLS clock integration
^^^^^^^^^^^^^^^^^^^^^^^^^^

When DTLS is used, Mbed TLS needs timer callbacks for handshake retransmissions.
Anjay Lite configures them internally through ``mbedtls_ssl_set_timer_cb()``, using
``mbedtls_timing_set_delay()`` and ``mbedtls_timing_get_delay()`` as the delay
callbacks. By default, the Mbed TLS timing module relies on POSIX
``gettimeofday()`` - if this is not available ``MBEDTLS_TIMING_ALT`` must be
enabled and the platform must provide its own implementation of these timing
functions.

Asynchronous network backends
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The default Mbed TLS integration uses the non-blocking BIO interface. A receive
callback that cannot currently provide data is reported to Mbed TLS as
``MBEDTLS_ERR_SSL_WANT_READ``.

This model works well with network backends where a receive call either completes
immediately or reports that no data is currently available, without leaving any
receive operation pending. A problem may occur with asynchronous backends where
``anj_net_recv()`` starts a receive operation and returns ``ANJ_NET_EINPROGRESS`` while
that operation is still ongoing.

``ANJ_NET_EINPROGRESS`` has stronger semantics than
``MBEDTLS_ERR_SSL_WANT_READ``. The Anjay Lite network API requires the same
operation to be retried with the same parameters before any other network
operation is performed. The Mbed TLS BIO API does not currently provide an
explicit guarantee that this requirement will be preserved across calls into
Mbed TLS.

This is especially relevant to DTLS handshake retransmissions. While waiting
for incoming handshake data, Mbed TLS may determine that the retransmission
timer has expired and attempt to send the previous handshake flight. If the
underlying ``anj_net_recv()`` operation is still in progress at that point,
this would violate the ``ANJ_NET_EINPROGRESS`` contract of the Anjay Lite
network API.

Blocking handshake operations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Anjay Lite is designed around a non-blocking ``anj_core_step()`` loop. The
default Mbed TLS integration follows this model for network progress: if
``mbedtls_ssl_handshake()`` needs more network I/O, Anjay Lite returns from the
current step and retries later.

However, some handshake stages may still spend noticeable CPU time inside Mbed
TLS before control returns to the application. This is most visible on small
devices when certificate-based handshakes use elliptic-curve operations. If this
is a problem for your event loop, consider Mbed TLS-level ECC tuning options.

``MBEDTLS_ECP_RESTARTABLE`` enables restartable ECC operations in Mbed TLS. With
an operation limit configured through ``mbedtls_ecp_set_max_ops()``, long ECC
computations may pause and make SSL functions such as ``mbedtls_ssl_handshake()``
return ``MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS``. Anjay Lite treats this result as
temporary progress and returns from the current step, so the application can run
other tasks before calling ``anj_core_step()`` again. Lower limits reduce the
maximum time spent in one cryptographic burst, but usually require more handshake
iterations.

``MBEDTLS_ECP_WINDOW_SIZE`` is another Mbed TLS option worth checking on
constrained targets. It controls a memory/performance trade-off in
elliptic-curve calculations. A smaller value makes Mbed TLS use less temporary
memory. When restartable ECC is enabled, it may also help Mbed TLS split some
calculations into shorter chunks, so control can return to Anjay Lite sooner.
The trade-off is performance: the same handshake may take longer overall.

These are Mbed TLS configuration choices, not Anjay Lite configuration fields.
They are only useful for handshakes that actually use the affected ECC code, and
the exact behavior depends on the Mbed TLS version, enabled crypto backend and
cipher suites. Treat them as tuning knobs and test the resulting handshake
latency, memory usage and total connection time on the target platform.

Debugging and troubleshooting
-----------------------------

When debugging Mbed TLS behavior in Anjay Lite, start by verifying the configuration
at all relevant layers:

- whether the relevant Anjay Lite build-time option or configuration field is
  set,
- whether the required Mbed TLS ``MBEDTLS_*`` option is enabled,
- whether the server supports the selected feature, such as CID, SNI or a given
  cipher suite.

If the configuration looks correct, continue with runtime diagnostics. Enable
Anjay Lite logs. For DTLS handshake issues, packet
captures are often the fastest way to confirm what is actually exchanged between
the client and the server.

Pay particular attention to certificate configuration. Many handshake failures
are caused by mismatched or incomplete certificate material, for example:

- the server certificate does not match the hostname used for verification,
- the SNI value does not match the certificate SAN/CN,
- the trust anchor is missing or does not match the selected Certificate Usage,
- the certificate is expired or not yet valid according to the device clock,
- the client certificate and private key do not match.

If debugging directly on the target device is not possible, try to reproduce the
same DTLS session from a development machine using equivalent credentials,
server URI, SNI value, cipher suites and trust anchors. This helps distinguish
server-side or credential-related issues from target-specific problems such as
network issues, memory limits or reduced Mbed TLS configuration.

.. note::

   For lossy or high-latency UDP networks, also check the handshake retransmission
   timeout values configured through
   ``ANJ_MBEDTLS_HS_INITIAL_TIMEOUT_VALUE_MS`` and
   ``ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS``. Incorrect retransmission timeouts
   may cause handshake failures due to excessive packet loss, even if the
   handshake would succeed in a more reliable network environment.
