..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Random Number Generation API
============================

Overview
--------

Anjay Lite requires a platform hook that returns random bytes. The RNG is used
by various subsystems, including security (e.g., DTLS) and LwM2M features
(e.g., token generation for CoAP messages).

Build-time configuration
------------------------

If you want to use the POSIX RNG compatibility layer, enable
``ANJ_WITH_RNG_POSIX_COMPAT`` in your CMake configuration.

If your platform is non-POSIX (bare metal, RTOS without POSIX), or you want to
provide a custom RNG implementation, you need to disable the POSIX RNG layer
and provide your own implementation of the API described below.

Function to implement
---------------------

API for random number generation consists of a single function:

+-------------------------------+-----------------------------------------------+
| Function                      | Purpose                                       |
+===============================+===============================================+
| ``anj_rng_generate()``        | Fills the provided buffer with random bytes.  |
+-------------------------------+-----------------------------------------------+

.. note::
   For function signature and detailed description, see
   ``include_public/anj/compat/rng.h``.

Reference implementation
------------------------

The default implementation in ``src/anj/compat/posix/anj_rng.c`` uses the POSIX
``getentropy()`` API and can be used as a reference for your integration.
Due to ``getentropy()`` limitations, the implementation breaks the request
into chunks of maximum 256 bytes.

.. highlight:: c
.. snippet-source:: src/anj/compat/posix/anj_rng.c

    int anj_rng_generate(uint8_t *buffer, size_t size) {
        const size_t MAX_CHUNK = 256; // getentropy() limit per call

        while (size > 0) {
            size_t chunk = size > MAX_CHUNK ? MAX_CHUNK : size;
            if (getentropy(buffer, chunk) != 0) {
                anj_log(rng, L_ERROR, "getentropy failed: %s", strerror(errno));
                return -1;
            }
            buffer += chunk;
            size -= chunk;
        }
        return 0;
    }

Another reference implementation using STM32 HAL library is available in
compat layer of Anjay Lite Bare Metal Client. In this example, the RNG
peripheral returns 32-bit random numbers, which are copied into the output buffer.

.. code-block:: c

    int anj_rng_generate(uint8_t *buffer, size_t size) {
        uint32_t random_number;
        for (size_t i = 0; i < size / sizeof(random_number); i++) {
            if (HAL_RNG_GenerateRandomNumber(&hrng, &random_number) != HAL_OK) {
                return -1;
            }
            memcpy(buffer, &random_number, sizeof(random_number));
            buffer += sizeof(random_number);
        }

        size_t last_chunk_size = size % sizeof(random_number);
        if (last_chunk_size) {
            if (HAL_RNG_GenerateRandomNumber(&hrng, &random_number) != HAL_OK) {
                return -1;
            }
            memcpy(buffer, &random_number, last_chunk_size);
        }

        return 0;
    }

Considerations for RNG quality
------------------------------

The application must provide a random byte generator used by the library
wherever randomness is required. When default security integrations are
enabled (e.g., ``ANJ_WITH_MBEDTLS``), this function also serves as an
entropy source for the cryptographic backend.

When secure connections are used, a cryptographically secure random number
generator (CSRNG) is mandatory. Recommended sources include:

- The operating system CSRNG (e.g., Linux ``getentropy()``).
- On embedded/bare-metal targets:

  - A hardware TRNG, if it provides sufficient throughput, or

  - A DRBG compliant with NIST SP 800-90A (CTR_DRBG, HMAC_DRBG/MAC_DRBG,
    or HASH_DRBG) seeded by a TRNG.

.. warning::
   The library assumes this function returns cryptographically secure bytes
   whenever security features are enabled. If a weaker generator is supplied,
   overall security is undefined.

For non-secure (no transport security) deployments, a general-purpose PRNG
is acceptable.
