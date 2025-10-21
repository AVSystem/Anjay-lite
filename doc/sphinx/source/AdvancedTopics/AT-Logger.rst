..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Configuring and Using the Logger
================================

Overview
--------

Anjay Lite provides a lightweight logging system designed for embedded
environments. It is used internally by Anjay Lite itself and is also available
for end user applications. This allows you to wiev both the library's
diagnostic messages and your application logs in a consistent format, while
keeping runtime and code size overhead low.

Logger types
------------

Exactly one logger type must be enabled:

* **Built-in handler** (``ANJ_LOG_FULL``)

  The library provides its own implementation. Log lines are formatted into a
  buffer, prefixed with log level, module, file and line number, and then
  passed to an output function.

* **Alternate handler** (``ANJ_LOG_ALT_IMPL_HEADER``)

  A user-provided header file defines how ``anj_log()`` expands. This mode is
  meant for integrating Anjay Lite with platform loggers such as Zephyr's
  logging API or other RTOS-specific systems.
  It requires more user setup but offers maximum
  flexibility and tight integration with existing logging frameworks.

Output backends for the built-in handler
----------------------------------------

When using the built-in handler (``ANJ_LOG_FULL``), you must select exactly one
output type:

* **stderr output** (``ANJ_LOG_HANDLER_OUTPUT_STDERR``)

  The library provides a default ``anj_log_handler_output()`` implementation
  that writes the formatted log line to ``stderr``.

* **custom output** (``ANJ_LOG_HANDLER_OUTPUT_ALT``)

  The user must implement ``anj_log_handler_output(const char *output,
  size_t len)``. This function receives the fully formatted log line and is
  responsible for transmitting it to the desired backend. **This option is
  recommended for embedded systems**, as it allows routing logs to UART, RTT,
  or any other interface available on the platform.

Example: minimal UART output implementation for STM32 HAL:

.. code-block:: c

   #include <anj/compat/log_impl_decls.h>
   #include <stm32u3xx_nucleo.h>

   void anj_log_handler_output(const char *output, size_t len) {
       HAL_UART_Transmit(&hcom_uart[COM1], (const uint8_t *) output,
                         len, COM_POLL_TIMEOUT);
       static const char newline[] = "\r\n";
       HAL_UART_Transmit(&hcom_uart[COM1], (const uint8_t *) newline,
                         sizeof(newline) - 1, COM_POLL_TIMEOUT);
   }

Alternate handler
-----------------

If ``ANJ_LOG_ALT_IMPL_HEADER`` is defined, the built-in implementation is
disabled. The provided header file must define the macro
``ANJ_LOG_HANDLER_IMPL_MACRO(Module, LogLevel, ...)``. All ``anj_log()`` calls
expand through this macro.

This approach is useful when you want to integrate Anjay Lite logging with
existing platform loggers that are also macro-based, such as Zephyr ``LOG_*()``.
Although this requires additional setup, it allows complete alignment with the
target system's logging conventions, filtering, and runtime control.

Usage
-----

Emit a log statement using the ``anj_log`` macro:

.. code-block:: c

   anj_log(my_module, L_INFO, "Hello %s (%d)", "world", 42);

To reduce binary size, you can wrap constant parts of log strings with
``ANJ_LOG_DISPOSABLE()``. If ``ANJ_LOG_STRIP_CONSTANTS`` is enabled, these
constants are replaced with a single space during compilation:

.. code-block:: c

   anj_log(my_module, L_DEBUG, ANJ_LOG_DISPOSABLE("Result: ") "%d", result);

Modules
-------

Log statements in Anjay Lite are grouped by *modules*, e.g. ``bootstrap``,
``exchange``, ``observe``. When writing your own logs you can also choose a
module name. This mechanism serves two purposes:

* **Debugging convenience** - you can enable detailed logs only for specific
  modules during troubleshooting.
* **Footprint control** - increasing the log level for a single module does not
  increase binary size as much as enabling verbose logs globally.

Filtering log levels
--------------------

Log levels form a hierarchy:

* ``L_TRACE`` - enables all messages
* ``L_DEBUG`` - enables debug, info, warning, and error messages
* ``L_INFO`` - enables info, warning, and error messages
* ``L_WARNING`` - enables warnings and errors
* ``L_ERROR`` - enables only errors
* ``L_MUTED`` - disables logging completely

The default log level is controlled by ``ANJ_LOG_LEVEL_DEFAULT`` (defaults to
``L_INFO`` if not set). Logs below this level are removed at compile time.

For finer control, per-module overrides are possible via
``ANJ_LOG_FILTERING_CONFIG_HEADER``, which shall point to a user-provided header
file with per-module settings. For example:

.. code-block:: c

   // in my_log_filtering_config.h
   #define ANJ_LOG_LEVEL_FOR_MODULE_exchange L_TRACE
   #define ANJ_LOG_LEVEL_FOR_MODULE_observe L_MUTED

This configuration enables trace logs for the ``exchange`` module while completely
disabling logs in ``observe``.
Such selective configuration makes it possible to diagnose specific problems
while keeping the binary size low.
