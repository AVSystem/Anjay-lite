..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
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
for end user applications. This allows you to view both the library's
diagnostic messages and your application logs in a consistent format, while
keeping runtime and code size overhead low.

Logger types
------------

Exactly one logger type must be enabled:

* **Built-in handler** (``ANJ_LOG_FULL``)

  The library provides its own implementation. Log lines are formatted into a
  buffer, prefixed with log level, module, file and line number, and then
  passed to an output function.

* **Minimal built-in handler** (``ANJ_LOG_MICRO``)

  A minimal built-in implementation that only prefixes log lines with log level
  and source file ID. This mode is intended for very constrained environments
  where code size is critical. It has a smaller footprint than the full logger,
  but also provides less context in log messages.

* **Alternate handler** (``ANJ_LOG_ALT_IMPL_HEADER``)

  A user-provided header file defines how ``anj_log()`` expands. This mode is
  meant for integrating Anjay Lite with platform loggers such as Zephyr's
  logging API or other RTOS-specific systems.
  It requires more user setup but offers maximum
  flexibility and tight integration with existing logging frameworks.

Output backends for the built-in handler
----------------------------------------

When using the built-in handler (``ANJ_LOG_FULL`` or ``ANJ_LOG_MICRO``),
you must select exactly one output type:

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

Micro-logs and offline decoding
-------------------------------

.. note::

    Code related to this tutorial can be found under ``examples/tutorial/AT-MicroLogs``
    in Anjay Lite source directory.

Enabling micro-logs
^^^^^^^^^^^^^^^^^^^

To use micro-logs, build Anjay Lite with the minimal built-in logger enabled, i.e.
define ``ANJ_LOG_MICRO`` and disable other logger types.

.. highlight:: cmake
.. snippet-source:: examples/tutorial/AT-MicroLogs/CMakeLists.txt

    set(ANJ_LOG_FULL OFF)
    set(ANJ_LOG_MICRO ON)

In this configuration, the in-memory representation of log messages is very compact and
uses a *source file ID* instead of a full file name. The file ID can later be resolved
offline by the conversion script.

Example log lines in micro-log format:

.. code-block:: text

    INFO <ANJ_uLOG>8;152</ANJ_uLOG> Anjay Lite initialized
    INFO <ANJ_uLOG>19;205</ANJ_uLOG> Device object installed
    INFO <ANJ_uLOG>27;676</ANJ_uLOG> Security object installed
    INFO <ANJ_uLOG>28;546</ANJ_uLOG> Server object installed
    INFO <ANJ_uLOG>13;79</ANJ_uLOG> Connected to eu.iot.avsystem.cloud:5683
    INFO <ANJ_uLOG>12;71</ANJ_uLOG> Registered successfully

File IDs
^^^^^^^^

When using micro-logs, each C source file that emits logs must define a unique
file ID. The ID must be defined before including `<anj/log.h>`:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MicroLogs/src/main.c

    #define ANJ_LOG_SOURCE_FILE_ID 201

    #include <anj/log.h>

File IDs:

* must be unique within the codebase,
* must stay stable over time for a given file (so that previously recorded logs
  can still be decoded),
* must not exceed ``65535``,
* must be defined as a numeric literal, not via another macro.

.. important::

    Anjay Lite source files already define their own file IDs. When integrating
    micro-logs into your application, make sure that your file IDs do not
    conflict with those used by Anjay Lite - choose IDs outside the range
    ``0-200`` reserved for the library.

Decoding micro-logs with the helper script
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Micro-logs are decoded outside the application using the ``micro_logs_decode.py``
script located in the ``tools/`` directory. The script:

* scans the source tree for ``ANJ_LOG_SOURCE_FILE_ID`` definitions,
* builds a mapping from file IDs to file paths,
* reads log lines from standard input (or from a file) and prints them in a
  human-readable form, with reconstructed module name, file and line number.

A typical usage when running the micro-logs AT example locally is:

.. code-block:: bash

    mkdir build
    cd build
    cmake ..
    make anjay_lite_at_micro_logs
    ./anjay_lite_at_micro_logs/anjay_lite_at_micro_logs endpoint_name 2>&1 \
       | ../tools/micro_logs_decode.py -r ../

In this example, the client binary is executed and its output is piped to the
decoding script. Logs are decoded in real-time.

Because the example outputs logs to ``stderr`` by default, ``2>&1`` is used
to redirect standard error to standard output.

The ``-r`` (or ``--root``) option specifies the project root that should be
scanned for source files. The script recursively walks this directory and its
subdirectories, looking for files that define ``ANJ_LOG_SOURCE_FILE_ID`` and
using those to resolve file IDs found in the logs.

The logs printed by the script look as follows:

.. code-block:: text

    INFO [src/anj/core/core.c:152]: Anjay Lite initialized
    INFO [src/anj/dm/dm_device_object.c:205]: Device object installed
    INFO [src/anj/dm/dm_security_object.c:676]: Security object installed
    INFO [src/anj/dm/dm_server_object.c:546]: Server object installed
    INFO [src/anj/core/srv_conn.c:79]: Connected to eu.iot.avsystem.cloud:5683
    INFO [src/anj/core/register.c:71]: Registered successfully

The logs can also be read from a file instead of standard input by providing the ``-i``
(or ``--input``) option followed by the file path.

Working with serial consoles
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When logs are coming from a serial console, it is often useful to:

* create a named pipe to capture UART output,
* open the serial console and write its output to the pipe,
* feed the result to the decoder script.

One possible setup using ``minicom`` and a named pipe:

.. code-block:: bash

   mkfifo uart_log
   minicom -D /dev/ttyACM0 -C uart_log

   # In another terminal:
   cat uart_log | ./tools/micro_logs_decode.py

In this example:

* ``minicom`` writes all UART output to the ``uart_log`` FIFO,
* ``micro_logs_decode.py`` decodes the remaining micro-log lines using the
  named pipe as input and the current working directory as the project root.
