# Changelog

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
