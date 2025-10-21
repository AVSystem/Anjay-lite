..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Configuration and Build Size Impact
===================================

Anjay Lite is highly configurable at build time. By enabling or disabling
specific compile-time flags, you can reduce the footprint for constrained
environments or enable advanced features such as FOTA or Composite operations.

This document summarizes the most important configuration options, grouped by
category. Use it as a practical guide for setting up your client.

.. note::

   The main way to configure Anjay Lite is by providing your own
   ``anj/anj_config.h`` file with the desired set of flags.
   When using CMake, this header can also be generated automatically during the
   build.

Configuration
-------------

Minimal configuration
^^^^^^^^^^^^^^^^^^^^^

These **enabler flags** are enough to create a basic LwM2M client that connects
to a server and supports standard operations on the Data Model, including the
**Client Registration Interface** and the **Device Management and Service Enablement Interface**
(without composite operations).

- ``ANJ_WITH_DEFAULT_SECURITY_OBJ`` - enables the built-in Security Object (/0)
- ``ANJ_WITH_DEFAULT_SERVER_OBJ`` - enables the built-in Server Object (/1)
- ``ANJ_WITH_DEFAULT_DEVICE_OBJ`` - enables the built-in Device Object (/3)
- ``ANJ_WITH_DISCOVER`` - enables support for the Discover operation
- ``ANJ_COAP_WITH_UDP`` - enables CoAP over UDP
- ``ANJ_WITH_SENML_CBOR`` - enables SenML CBOR as a payload format

SenML CBOR is the most complete content format, supporting composite
operations, timestamps in Send messages and notifications. It can replace any other format.

.. note::

   In addition to these feature flags, you also need to configure the
   *compatibility layers*.  
   For example, if you want to run a minimal client on a POSIX system, define:

   .. code-block:: c

      #define ANJ_WITH_TIME_POSIX_COMPAT
      #define ANJ_WITH_RNG_POSIX_COMPAT
      #define ANJ_WITH_SOCKET_POSIX_COMPAT
      #define ANJ_NET_WITH_IPV4
      #define ANJ_NET_WITH_UDP

   Also note that all **numeric configuration options** (such as buffer sizes,
   and limits for objects, observations, or attributes) directly affect **RAM
   usage**.
   Memory footprint considerations are covered later in this document.

Commonly used
^^^^^^^^^^^^^

Most production deployments require additional features beyond the minimal
configuration.
These options significantly improve interoperability with real-world LwM2M
servers, enable more robust operation, and cover scenarios that almost every
deployment requires in practice.

**Security**

- ``ANJ_WITH_SECURITY`` - enables support for secure connections
- ``ANJ_NET_WITH_DTLS`` - enables DTLS transport
- ``ANJ_WITH_MBEDTLS`` - enables built-in MbedTLS integration
  (you can provide a custom TLS backend if needed)

**Bootstrap support**

- ``ANJ_WITH_BOOTSTRAP`` - enables the Bootstrap Interface
- ``ANJ_WITH_BOOTSTRAP_DISCOVER`` - enables Bootstrap Discover operation,
  not all servers require it

**Caching**

- ``ANJ_WITH_CACHE`` - enables response caching to prevent processing
  retransmitted UDP messages multiple times.

.. warning::

   Caching is essential for reliable operation over UDP. Without it,
   retransmitted messages may be processed multiple times, leading to
   duplicate operations and inconsistent state.

**Content formats and CBOR extensions**

You can often configure or enforce the preferred formats on the server side.
If you are not certain how your server encodes messages, it is generally safer
to enable the additional CBOR decoder extensions so that the client can handle
a wider range of payloads.

- ``ANJ_WITH_PLAINTEXT`` - allows plaintext encoding for single resources.
- ``ANJ_WITH_OPAQUE`` - used in FOTA Push mode
- ``ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS``
- ``ANJ_WITH_CBOR_DECODE_HALF_FLOAT``
- ``ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES``
- ``ANJ_WITH_CBOR_DECODE_STRING_TIME``

**Firmware Update (FOTA)**

- ``ANJ_WITH_DEFAULT_FOTA_OBJ`` - enables the built-in Firmware Update Object (/5)
- ``ANJ_FOTA_WITH_PUSH_METHOD`` - enables firmware delivery via PUSH (Write operation)
- ``ANJ_FOTA_WITH_PULL_METHOD`` - enables firmware delivery via PULL (download)

Additional features
^^^^^^^^^^^^^^^^^^^

Beyond the minimal and recommended configurations, Anjay Lite provides
a number of **additional features** that can be enabled depending on
the needs of your application.

**CoAP Downloader**

- ``ANJ_WITH_COAP_DOWNLOADER`` - generic CoAP/CoAPS downloader used by FOTA Pull
- ``ANJ_FOTA_WITH_COAPS`` / ``ANJ_FOTA_WITH_COAP`` - transport options for firmware downloads

**Persistence**

- ``ANJ_WITH_PERSISTENCE`` - allows storing Security and Server object state to
  a non-volatile storage. See :doc:`tutorial <../AdvancedTopics/AT-Persistence>` for details.

**Information Reporting Interface**

- ``ANJ_WITH_LWM2M_SEND`` - enables the LwM2M Send operation
- ``ANJ_WITH_OBSERVE`` - enables Observation support

**Logging**

Anjay Lite has flexible logging options. Check out
:doc:`the article <../AdvancedTopics/AT-Logger>` for details. The most relevant flags are:

- ``ANJ_LOG_FULL`` - full log mode with file and line numbers
- ``ANJ_LOG_LEVEL_DEFAULT`` - sets the default log level (can be overridden per-module)

Optional / advanced
^^^^^^^^^^^^^^^^^^^

Some options are rarely needed but can be enabled for specific scenarios:

- ``ANJ_WITH_COMPOSITE_OPERATIONS`` - enables Composite Read and Composite Write operations
- ``ANJ_WITH_OBSERVE_COMPOSITE`` - enables Observe-Composite and Cancel-Composite operations
- ``ANJ_WITH_DISCOVER_ATTR`` - reports attributes in Discover responses
  In practice this is rarely needed, because Discover is usually performed
  right after Register, which in turn forces the server to reapply attributes
  using Write-Attributes.
- ``ANJ_WITH_EXTERNAL_CRYPTO_STORAGE`` - integration with external secure key storage (e.g., HSM)
- ``ANJ_WITH_EXTERNAL_DATA`` - use external data sources (see :doc:`tutorial <../AdvancedTopics/AT-ExternalDataTypes>` for details)
- ``ANJ_WITH_LWM2M12`` enables selected features from the LwM2M 1.2 specification
  It adds support for new attributes (`edge`, `con`, `hqmax`), extends the Server Object with
  a `default notification mode` resource, and allows passing attributes directly in Observe
  requests instead of separate Write-Attributes. Although LwM2M CBOR is formally defined in 1.2,
  Anjay Lite makes it available regardless of the negotiated version.
- ``ANJ_WITH_TLV`` - legacy TLV decoder
- ``ANJ_WITH_CBOR`` - CBOR encoder/decoder (content-format **60**) - rarely used
- ``ANJ_WITH_LWM2M_CBOR`` - LwM2M CBOR encoder/decoder (content-format **11544**) - SenML CBOR is generally preferred
- ``ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS`` - replaces ``sprintf``/``sscanf`` with minimal converters
  This can be useful in very resource-constrained environments, since linking
  the standard ``stdio`` conversion functions may add more than **20 kB** of
  extra code size overhead.

Library size impact
-------------------

To better understand how each module affects the final footprint, we measured
binary sizes when building the
`Anjay Lite Bare-Metal Client <https://github.com/AVSystem/Anjay-lite-bare-metal-client>`_
with various configurations. Size analysis was performed with **puncover**.

Note that **puncover** does not include data from the **.rodata** section (i.e., `const` variables),
so these values were manually extracted and added to the overall result to provide a complete
representation of the final memory footprint.

The build environment was:

- Compiler: ``arm-none-eabi-gcc-14.2.1``
- Anjay Lite version: ``1.0.0``
- Build type: ``RelWithDebInfo`` (``-DCMAKE_BUILD_TYPE=RelWithDebInfo``)

.. note::

   The sizes include Anjay Lite core and built-in modules
   without external integration layers.
   An exception is the default MbedTLS DTLS integration layer
   (``anj_mbedtls_dtls_socket.c``), which is included in the measurements,
   but the full **MbedTLS library itself is not**.
   For details about its size and configuration, refer to the
   `MbedTLS repository <https://github.com/Mbed-TLS/mbedtls>`_.

   Actual numbers depend on compiler version, optimization flags and enabled
   modules. These measurements should be treated as a guideline only.

.. important::

   Building with the ``MinSizeRel`` configuration typically reduces the final
   binary size by around **5%** compared to ``RelWithDebInfo``.
   However, we do not use this configuration for size breakdowns in this
   document, because it prevents accurate estimation of the size of individual components.

Overall size depending on enabled modules
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Approximate binary sizes (in kilobytes) with logs disabled:

+----------------------------------------------------------------------------------------------------+-------------------------+
| Configuration                                                                                      | Size [kB]               |
+====================================================================================================+=========================+
| **Minimal build (must-have only)**                                                                 | **33.2**                |
+----------------------------------------------------------------------------------------------------+-------------------------+
| Security (PSK) - MbedTLS not included                                                              | \+ 1.7                  |
+----------------------------------------------------------------------------------------------------+-------------------------+
| Bootstrap support                                                                                  | \+ 4.4                  |
+----------------------------------------------------------------------------------------------------+-------------------------+
| Caching mechanism                                                                                  | \+ 1.2                  |
+----------------------------------------------------------------------------------------------------+-------------------------+
| FOTA                                                                                               | \+ 1.3                  |
+----------------------------------------------------------------------------------------------------+-------------------------+
| Additional recommended content-formats  (Plaintext + Opaque + CBOR extensions)                     | \+ 5.8 (2.8 + 0.6 + 2.4)|
+----------------------------------------------------------------------------------------------------+-------------------------+
| **Minimal build + all commonly used options**                                                      | **47.6**                |
+----------------------------------------------------------------------------------------------------+-------------------------+
| LwM2M Send                                                                                         | \+ 0.9                  |
+----------------------------------------------------------------------------------------------------+-------------------------+
| Observations                                                                                       | \+ 5.4                  |
+----------------------------------------------------------------------------------------------------+-------------------------+
| Persistence                                                                                        | \+ 0.8                  |
+----------------------------------------------------------------------------------------------------+-------------------------+
| CoAP Downloader                                                                                    | \+ 1.7                  |
+----------------------------------------------------------------------------------------------------+-------------------------+
| **Minimal build + all commonly used options + all additional options (excluding logs)**            | **56.4**                |
+----------------------------------------------------------------------------------------------------+-------------------------+
| **All of the above + LwM2M 1.2 + TLV + LwM2M CBOR + CBOR + composite operations + discover attr.** | **65.5**                |
+----------------------------------------------------------------------------------------------------+-------------------------+

Impact of logging
^^^^^^^^^^^^^^^^^

Logging has a measurable impact on final binary size. The table below shows how
much the size increases when enabling logs at different levels.

.. note::

   The absolute size increase depends on which modules are enabled,
   since each of them may add its own log messages.  
   For this reason, the **percentage growth** is often a better metric to
   consider when evaluating the impact of logging on your build.

+-------------------------------------------------------------------+---------------------+-----------------------------------+---------------------+
| Build variant                                                     | Error logs only [kB]| Error, warning and info logs [kB] | Full logs [kB]      |
+===================================================================+=====================+===================================+=====================+
| Minimal build                                                     | \+ 7.3  (~22%)      | \+ 9.7  (~29%)                    | \+ 12.1  (~36%)     |
+-------------------------------------------------------------------+---------------------+-----------------------------------+---------------------+
| Minimal build + all commonly used options + all additional options| \+ 13.2 (~23%)      | \+ 15.2  (~27%)                   | \+ 19.1 (~34%)      |
+-------------------------------------------------------------------+---------------------+-----------------------------------+---------------------+

Static RAM usage
^^^^^^^^^^^^^^^^

In addition to flash size, Anjay Lite also consumes a certain amount of static RAM.
Unlike many libraries, Anjay Lite does not use dynamic memory allocation - no heap is required.
All larger buffers and data structures are allocated within the main context ``anj_t`` or within object instances.
If these objects are declared as static, they do not increase stack usage.

The table below shows approximate static RAM allocations for the core context and built-in objects
for a minimal configuration with security, caching and observations enabled.

+------------------------------------+-----------------+---------------------------------------------+
| Component                          | RAM usage [B]   | Flags affecting size                        |
+====================================+=================+=============================================+
| **anj_t core context**             | **7816**        | —                                           |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Incoming messages buffer       | 1200            | ANJ_IN_MSG_BUFFER_SIZE                      |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Outgoing messages buffer       | 1200            | ANJ_OUT_MSG_BUFFER_SIZE                     |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Outgoing payload buffer        | 1024            | ANJ_OUT_PAYLOAD_BUFFER_SIZE                 |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Caching payload buffer         | 1024            | ANJ_OUT_PAYLOAD_BUFFER_SIZE                 |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Observations array             | 10*120          | ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER         |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Attributes array               | 10*64           | ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER     |
+------------------------------------+-----------------+---------------------------------------------+
| **Security Object (/0) instance**  | **1040**        | —                                           |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Public key buffer              | 2*64            | ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Server public key buffer       | 2*64            | ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE      |
+------------------------------------+-----------------+---------------------------------------------+
| \-> Secret key buffer              | 2*64            | ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE             |
+------------------------------------+-----------------+---------------------------------------------+
| **Server Object (/1) instance**    | **152**         | —                                           |
+------------------------------------+-----------------+---------------------------------------------+
| **Device Object (/3) instance**    | **60**          | —                                           |
+------------------------------------+-----------------+---------------------------------------------+

To reduce memory footprint, you should primarily focus on adjusting:

- buffer sizes (``ANJ_IN_MSG_BUFFER_SIZE``, ``ANJ_OUT_MSG_BUFFER_SIZE``, etc.),
- maximum record counts (``ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER`` for observations number, etc.).

Summary
-------

Anjay Lite's configuration system is highly modular, allowing you to tailor the
client to very constrained environments or full-featured production use cases.
Start with a minimal setup, add features as required by your deployment, and
balance functionality against memory and binary size constraints.  
Careful selection of enabled modules ensures both efficiency and long-term
maintainability of your solution.
