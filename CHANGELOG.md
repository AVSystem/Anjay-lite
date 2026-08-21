# Changelog

## Anjay Lite 3.0.0 (August 21st, 2026)

### BREAKING CHANGES
- Mark `anj_crypto_storage_*` API as experimental.
- Flags `ANJ_FOTA_WITH_COAPS`, `ANJ_NET_WITH_DTLS`, `ANJ_WITH_SECURITY` and `ANJ_WITH_MBEDTLS` are
  now enabled by default to comply with Secure by Default policy. Flag `ANJ_FOTA_WITH_COAP` is now
  disabled by default.
- Removed `ANJ_MBEDTLS_TLS_VERSION`. `ANJ_MBEDTLS_ALLOWED_CIPHERSUITES` has been replaced with separate
  `ANJ_MBEDTLS_ALLOWED_CERT_CIPHERSUITES` and `ANJ_MBEDTLS_ALLOWED_PSK_CIPHERSUITES` options.
- Modified `anj_crypto_storage_*` API, all records are now added in a single call.
- Removed `anj_dm_security_obj_get_psk()` function.

### Features
- Added support for certificate-based security in Anjay Lite and the Mbed TLS integration layer.
- Added Trust Store support for certificate-based security.
- Added a retry mechanism in COAP Downloader configured by `anj_coap_downloader_configuration_t::retry_count`
  and `anj_coap_downloader_configuration_t::retry_delay`.
- Added `anj_coap_downloader_suspend()` and `anj_coap_downloader_resume()` API to CoAP Downloader.
- New `security.h` API allows you to retrieve security configuration from the Security Object.
- Add `anj_crypto_zeroize()` API for securely clearing sensitive data.

### Improvements
- Changed the Bootstrap transaction flow so that a single transaction now covers
  the entire Bootstrap process. This allows changes made during Bootstrap to be
  committed only after a successful Bootstrap Finish, instead of committing each
  LwM2M operation independently.
- Registration Retry attempts now log the total count of attempts.
- `tools/mbedtls_cache.py` has been introduced to easily manage different versions
   and configurations of MbedTLS builds.
- Removed usages of __attribute__((unused)) from the codebase.
- Refactored and streamlined the `anj_crypto_storage_*` usage flow.

### Bugfixes
- Fixed an issue in the Mbed TLS integration related to the close function.
When the Connection ID extension was not supported, no new handshake was
performed when reconnecting after calling the close function.

## Anjay Lite 2.1.2 (June 2nd, 2026)

### Improvements
- Updated documentation links in README.md.

## Anjay Lite 2.1.1 (May 29th, 2026)

### Bugfixes
- Added files necessary for installing pymbedtls (needed for integration tests)

## Anjay Lite 2.1.0 (May 27th, 2026)

### Features
- (commercial version only) Added support for OSCORE with Appendix B2 procedure.

### Improvements
- Internal headers now consistently prefix internal enum and macro symbols with `_ANJ_`
  to avoid public-looking symbol leakage.
- Improved and refactored logging to provide more detailed and informative messages.
- Replace all `usleep()` uses in examples with `nanosleep()`.
- Improved `anj_core_next_step_time()` so that it also reflects delays before scheduled
  Bootstrap and Registration retry attempts.

### Bugfixes
- Fixed a bug where the network context was not properly closed and cleaned up after a connection failure during Bootstrap.
- Fixed sending retransmissions after receiving ACK (separate response)
- Fixed a bug that caused an assertion failure when handling a request with an empty token.
- Added Message ID validation for received messages.
- Fixed incorrect handling of a Non-confirmable Execute operation.
- Fixed missing empty ACK for confirmable separate error responses (CoAP 4.xx/5.xx) in the exchange layer.
- Fix a bug that made PSK identity length validation based on `MBEDTLS_PSK_MAX_LEN` instead of `ANJ_MBEDTLS_PSK_IDENTITY_MAX_LEN`

## Anjay Lite 2.0.0 (January 29th, 2026)

### BREAKING CHANGES
- Removed support for TCP and TLS bindings in NET layer - 
  `ANJ_NET_WITH_TCP`, `ANJ_COAP_WITH_UDP`, `ANJ_COAP_WITH_TCP` options were removed.
- Removed `anj_net_shutdown` from NET layer.
- Monotonic time is now the primary clock used for scheduling operations,
  replacing real-time clock usage across the codebase.

### Features
- NTP module for time synchronization.
- (commercial version only) Added default OSCORE Object implementation.
- Added support for canceling observations with CoAP RST message.
- Re-introduced `anj_core_ongoing_operation` API.
- Added `ANJ_OBSERVE_OBSERVATION_CANCEL_ON_TIMEOUT` option to control behavior
  in case of notification timeouts.
- Introduced `ANJ_LOG_MICRO` option for optimizing footprint of 
  logging with `tools/micro_logs_decode.py` tool to decode log messages.

### Improvements
- Switch RNG implementation from getrandom() to getentropy() for improved portability.
- Tests structure refactored in order to increase coverage.
- Shorter Send operation payloads due to the use of a common path when encoding messages.
- MbedTLS integration layer sets the correct MTU value.
- MbedTLS is now fetched and compiled only once when calling `make all` via root `CMakeLists.txt`.

### Bugfixes
- Fixed a bug in TLV decoder that incorrectly rejected request with a resource
  instance specified in the uri-path.
- Fixed handling of Discover requests targeting the root path.
- Fixed incorrect descriptions of return values in callbacks from FOTA.
- Writes to Object Instances now ignore unknown optional Resources, per LwM2M specification.

## Anjay Lite 1.0.0 (October 20th, 2025)

### BREAKING CHANGES
- Implemented new Time API with distinct clock types:
  `anj_time_real_t`, `anj_time_monotonic_t`, `anj_time_duration_t`.
- Renamed `anj_dm_res_t::operation` field to `anj_dm_res_t::kind`.
- Renamed `anj_dm_res_operation_t` type to `anj_dm_res_kind_t`.
- Moved `anj/log/log.h` header to `anj/log.h`.
- Removed `anj_core_ongoing_operation` API.
- Removed `anj_dm_bootstrap_cleanup` API.

### Bugfixes
- Fixed broken README.md link in integrations documentation

## Anjay Lite 1.0.0-beta.2 (August 27th, 2025)

### BREAKING CHANGES

- Changed error codes defined in the `anj_net_api.h` to positive values. Returning any positive values from the
  user Network API implementation, other than `ANJ_NET_E*`, is prohibited.
- New mandatory `anj_net_queue_mode_rx_off_t` API with example per-binding 
  implementations, including `anj_udp_queue_mode_rx_off()` and 
  `anj_dtls_queue_mode_rx_off()` were added.
- Removed `anj_net_reuse_last_port_t` with example per-binding implementations.
- Changed `anj_dm_transaction_end_t` last argument.

### Features

- CoAP Downloader module for large file transfers from CoAP servers, supporting FOTA Pull scenarios.
- Added support for Write-Composite operation.
- Persistence module and mechanism for Security and Server objects store/restore.

### Improvements

- Added `anj/init.h` header to make config includes and option dependency checks
  more consistent.
- Removed unnecessary usages of `ANJ_CONTAINER_OF()` in object implementation
  examples.
- Introduce Python tools for downloading object XMLs from OMA registry - `tools/lwm2m_object_registry.py` and generating
  object stubs - `tools/anjay_codegen.py`
- Implemented responses caching.
- If no confirmable notification is sent for 24 hours, the next notification sent will be confirmable.
- The library now follows the include pattern recommendations from Include What You Use (IWYU) version 0.24 
  compatible with clang 20.

### Bugfixes

- Fixed improper handling of LwM2M Server responses in Separate Response mode.
- Updated default value in description of `ANJ_COAP_MAX_OPTIONS_NUMBER` option.
- Fixed an issue with calling the observation module API during message exchange handling.
- Corrected the timing for setting the last sent notification timestamp.
- Fixed an issue that allowed server both bootstrap and management to freely read 
  the Security and OSCORE objects.

## Anjay Lite 1.0.0-beta.1 (June 9th, 2025)

Initial release.
