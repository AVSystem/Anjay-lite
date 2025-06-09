..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Introduction
============

**Anjay Lite** is a library implementing the *OMA Lightweight Machine to
Machine* (LwM2M) protocol, including essential CoAP functionality.

This project is developed and maintained by
`AVSystem <https://www.avsystem.com>`_.

This is a beta release. It offers a limited subset of features compared to
`Anjay <https://github.com/AVSystem/Anjay>`_, and its functionality
is still under active development. As such, users should be aware that the API
and behavior may change in future versions, and breaking changes are to be
expected.

Supported Protocol Version
--------------------------

This implementation is based on the following specification documents:

- *Lightweight Machine to Machine Technical Specification: Core*  
   Document number: `OMA-TS-LightweightM2M_Core-V1_2_2-20240613-A <https://www.openmobilealliance.org/release/LightweightM2M/V1_2_2-20240613-A/OMA-TS-LightweightM2M_Core-V1_2_2-20240613-A.pdf>`_

- *Lightweight Machine to Machine Technical Specification: Transport Bindings*  
   Document number: `OMA-TS-LightweightM2M_Transport-V1_2_2-20240613-A <https://www.openmobilealliance.org/release/lightweightm2m/V1_2_2-20240613-A/OMA-TS-LightweightM2M_Transport-V1_2_2-20240613-A.pdf>`_

Supported Features
------------------

Features currently supported
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Support for both LwM2M 1.1 and 1.2**
- **Interfaces**
   - *Bootstrap support*
   - *Client Registration support*
   - *Device Management and Service Enablement support*
   - *Information Reporting support*
- **Transport**
   - *Support for UDP Binding*
- **Content Formats**
   - **Input**: TLV, PlainText, Opaque, CBOR, SenML CBOR, LwM2M CBOR
   - **Output**: PlainText, Opaque, CBOR, SenML CBOR, LwM2M CBOR
- **Preimplemented LwM2M Objects**
   - *Security* (``/0``)
   - *Server* (``/1``)
   - *Device* (``/3``)
   - *Firmware Update* (``/5``)

Future work
^^^^^^^^^^^

- **Security**
   - *PSK*
   - *Certificates*
   - *OSCORE*
   - *EST*
- **HSM support**
- **Composite Write operation**
- **Bootstrap-Pack-Request operation**
- **Notification Storing When Offline**
- **CoAP Downloader**
- **HTTP Downloader**
- **Other transport bindings**

Technical Information
---------------------

Anjay Lite is implemented in standards-compliant C99 and depends only on widely
available functions from the standard C library, making it suitable for a wide
range of platforms, including bare-metal systems without an operating system.
While its core does not rely on OS-specific features, it can be compiled out of
the box with support for POSIX networking and time interfaces.
