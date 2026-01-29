..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Initialize Anjay Lite client
============================

Project structure
^^^^^^^^^^^^^^^^^

Create the following directory layout for your client project:

.. code-block:: none

    example/
      ├── CMakeLists.txt
      └── src/
          └── main.c

.. note::

    All code found in this tutorial is available under
    ``examples/tutorial/BC*`` in Anjay Lite source directory. Each tutorial project
    follows the same project directory layout.

Configure the build system
^^^^^^^^^^^^^^^^^^^^^^^^^^

We are going to use CMake as a build system. Create a ``CMakeLists.txt`` file in
the root of your project directory with the following content:

.. highlight:: cmake
.. snippet-source:: examples/tutorial/BC-Initialization/CMakeLists.txt

    cmake_minimum_required(VERSION 3.16.0)

    project(anjay_lite_bc_initialization C)

    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_EXTENSIONS OFF)

    if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
        set(anjay_lite_DIR "../../../cmake")
        find_package(anjay_lite REQUIRED)
    endif()

    add_executable(anjay_lite_bc_initialization src/main.c)
    target_include_directories(anjay_lite_bc_initialization PUBLIC
        "${CMAKE_SOURCE_DIR}"
        )

    target_link_libraries(anjay_lite_bc_initialization PRIVATE
                          anj
                          anj_extra_warning_flags)

This file configures the CMake build system to compile your client and link it
with the Anjay Lite library.

.. _anjay-lite-hello-world:

Implement the Hello World client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Now, we can begin the actual client implementation. Create a ``main.c`` file in the
``src/`` directory with the following content:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Initialization/src/main.c

    #include <stdbool.h>
    #include <stdio.h>
    #include <unistd.h>

    #include <anj/core.h>
    #include <anj/log.h>

    #define log(...) anj_log(example_log, __VA_ARGS__)

    int main(int argc, char *argv[]) {
        if (argc != 2) {
            log(L_ERROR, "No endpoint name given");
            return -1;
        }

        anj_t anj;
        anj_configuration_t config = {
            .endpoint_name = argv[1]
        };

        if (anj_core_init(&anj, &config)) {
            log(L_ERROR, "Failed to initialize Anjay Lite");
            return -1;
        }

        while (true) {
            anj_core_step(&anj);
            usleep(50 * 1000);
        }
        return 0;
    }

.. note::

    Complete code of this example can be found in
    `examples/tutorial/BC-Initialization` subdirectory of main Anjay Lite
    project repository.

**Code explanation:**

- The ``anj_core_init()`` function is used to initialize the Anjay Lite instance.
  You must pass a ``anj_configuration_t`` structure with the basic runtime
  configuration.
- The example program configures only one basic value: `endpoint_name`. This
  endpoint name is used to uniquely identify the client to a LwM2M server.
- After initializing the library, a loop calls ``anj_core_step()`` continuously.
  This function handles all LwM2M client tasks. It is non-blocking unless a
  blocking network API is used.

.. note::
    Anjay Lite does not allocate resources that require cleanup. You do not need
    to call a shutdown or cleanup function.

Build and run the client
^^^^^^^^^^^^^^^^^^^^^^^^

In your project directory, build the client:

.. code-block:: sh

    $ cmake . && make

If the build succeeds, you can run the client. Provide an endpoint name as an
argument. When the communication with the server takes place later in the project
this will be a name that the client uses to identify itself to the server. At
this point, you can use the local hostname to simulate an endpoint name:

.. code-block:: sh

    $ ./anjay_lite_bc_initialization urn:dev:os:$(hostname)

The program will log errors about the missing Server instance and may appear idle.
This is expected at this stage, as no server is configured yet.

The connection to the LwM2M Server will be described in the next steps.
