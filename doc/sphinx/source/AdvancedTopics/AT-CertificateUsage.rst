..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Certificate Usage
=================

The `Certificate Usage <https://www.openmobilealliance.org/release/LightweightM2M/V1_2-20201110-A/HTML-Version/OMA-TS-LightweightM2M_Transport-V1_2-20201110-A.html#5-2-9-7-0-5297-Certificate-Usage-Field>`_
resource in the LwM2M Security Object defines how Anjay Lite
interprets and applies the Server Public Key resource. This setting decides
whether the certificate provided in the Security Object is used as a trust
anchor, as the exact server certificate, or in some other way.

Configuring certificate usage correctly is crucial for secure communication.
Anjay Lite can also use an extra PKIX trust store, depending on the chosen
certificate usage, or it can ignore the trust store completely during server
certificate verification. By understanding each option, you can configure Anjay Lite
to establish safe and reliable connections to your servers.

.. warning::
    Anjay Lite doesn't support the "MatchingType" resource. The default value
    "Exact Match" is used.


Certificate Usage Settings in Anjay Lite
----------------------------------------

The `Certificate Usage <https://www.openmobilealliance.org/release/LightweightM2M/V1_2-20201110-A/HTML-Version/OMA-TS-LightweightM2M_Transport-V1_2-20201110-A.html#5-2-9-7-0-5297-Certificate-Usage-Field>`_
values in LwM2M correspond to the semantics from
`RFC 6698 (DANE TLSA) <https://www.rfc-editor.org/rfc/rfc6698.html>`_.
Each value specifies how the certificate supplied in the Security Object (the
Server Public Key resource) is used when validating the server’s identity.
During the TLS/DTLS handshake, depending on this setting, Anjay Lite behaves as
follows:

- DANE-EE (3 – Domain-issued certificate)
    Anjay Lite skips PKIX chain building and compares the handshake leaf directly to
    the certificate in "Server Public Key".

    - The **local trust store is not required** and is **ignored** for the
      accept/reject decision.
    - The value must be the leaf certificate; a CA/root will be rejected.

.. note::
   Example usage is shown in :doc:`AT-Certificates`

- DANE-TA (2 – Trust anchor assertion)
    Anjay Lite treats the "Server Public Key" certificate as the **DANE trust
    anchor** for this server. It attempts to build and verify the path from the
    handshake **leaf** to that **anchor** using the certificates received
    in the handshake.

    - The **local trust store is not required** and is **ignored** for the
      accept/reject decision.
    - The "Server Public Key" **must be a CA/root**. Using a leaf certificate
      is invalid for certificate usage set to 2.


- PKIX-EE (1 – Service certificate constraint)
    Anjay Lite performs standard PKIX verification and also requires that the
    handshake leaf certificate exactly matches the certificate stored in the
    "Server Public Key" resource.

    - A **trust store is required** to anchor PKIX.
    - Self-signed deployments can work if:

        - The "Server Public Key" contains the self-signed leaf.
        - The same leaf is present in the trust store to allow PKIX to succeed.

.. note::
    Example usage is shown in :doc:`AT-TrustStore`

- PKIX-TA (0 – CA constraint)
    Anjay Lite performs standard PKIX verification of the received certificate chain
    using the local trust store. Additionally, the CA named in the
    "Server Public Key" resource **must appear in the verified chain as well as
    in the trust store**.

    - A **trust store is required**. Without it, PKIX cannot anchor the chain
      and the handshake fails.
    - The TLSA value **must be a CA** (intermediate or root), not a leaf
      certificate.


.. important::

   Crypto backends and servers commonly omit the root certificate in the
   handshake. Validation typically proceeds as leaf → intermediates →
   (trusted root certificate from the trust store).

   Depending on what is present in the trust store, the backend may anchor the
   chain at the root CA or stop earlier at an intermediate CA that is already
   trusted. This matters for PKIX-TA, because the CA certificate configured in
   the "Server Public Key" resource must be present in the certificate chain
   selected by the backend. As a result, if both the root CA and an intermediate
   CA are trusted, a PKIX-TA constraint configured with the root CA may fail in
   Mbed TLS, because the backend may stop validation at the intermediate CA before
   the root CA becomes visible to the verification callback.

Skipping the verification of the server public key
--------------------------------------------------

Anjay Lite allows the application to skip the verification of the
server certificate using
``ANJ_ALLOW_INSECURE_SERVER_CERTIFICATE_SKIP`` configuration option. In that mode,
Anjay Lite accepts a empty Server Public Key resource in the Security object and
disables server certificate verification.

.. warning::
   This makes the connection vulnerable
   to man-in-the-middle attacks and must not be used in production environments.
