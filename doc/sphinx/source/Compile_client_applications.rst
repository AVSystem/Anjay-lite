..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Compile Client Applications
===========================

This guide explains how to set up your environment, integrate Anjay Lite into
your application, and build and run tests on Linux systems.

.. note::

   This guide applies to Linux systems only and has been verified on Ubuntu
   22.04 and Ubuntu 24.04.

Prepare the environment
-----------------------

To avoid dependency issues, install all required packages with:

.. code-block:: bash

   apt-get update && apt-get install -yq git build-essential cmake

The following tools are not required for building Anjay Lite project itself, but
they may be needed for auxiliary tasks such as testing, documentation generation
or memory analysis.

.. code-block:: bash

   apt-get install -yq python3 python3-pip clang-tools valgrind curl doxygen
   pip3 install -U -r requirements.txt

.. _integrating-anjay-lite:

Integrate Anjay Lite into a custom application
----------------------------------------------

Before building and integrating Anjay Lite, review and adjust its compile-time
configuration to match the target platform and required features.
Anjay Lite is modular and flexible — many features can be enabled or disabled
as needed.

Anjay Lite uses a configuration header to control which features and components
are included during compilation. A default template is provided as
:file:`anj_config.h.in`. You can configure it in two ways:

- **Automatic (CMake-based):**

  - If you integrate Anjay Lite using :code:`find_package`, CMake generates the
    configuration file automatically.
    Customize it by setting relevant ``ANJ_*`` variables in your
    :file:`CMakeLists.txt` before calling :code:`find_package`. This
    approach requires no manual editing of the ``.in`` file.

- **Manual configuration:**

  When not using :code:`find_package` or when building as part of a larger
  cross-platform project: you can configure Anjay Lite manually:

  - Copy the :file:`include_public/anj/anj_config.h.in` file to the
    ``config/`` directory, and rename it to :file:`config/anj/anj_config.h`.

  - Open the file and:

    - Replace ``#cmakedefine *_SOME_FEATURE`` with ``#define *_SOME_FEATURE``
      to enable a feature.
    - Remove or comment the line to disable a feature.
    - Replace any ``@PLACEHOLDER@`` values.

  - Add the ``config/`` directory to your include path.

Each configuration option is documented inline, explaining its purpose and
possible values. Adjusting these options lets you tailor the library for
different targets—from full-featured POSIX systems to minimal bare-metal setups.

Build on Linux
^^^^^^^^^^^^^^

To integrate Anjay Lite into your own project on Linux using CMake, you don't
need to install the library system-wide. Instead, Anjay Lite is set up in a way
that lets CMake use it directly from its source directory. You can include it in
your project by using the :code:`find_package` command. Just make sure to tell
CMake where to find the Anjay Lite sources — for example, by setting the
:code:`anjay_lite_DIR` variable to point to the cmake subdirectory of the Anjay
Lite source tree, as shown below.

When using :code:`find_package`, define the required configuration variables
before calling the command. If no variables are set, CMake uses the default
configuration from the :file:`cmake/anjay-lite-config.cmake` file. CMake
automatically generates the :file:`anj_config.h` file based on the selected
configuration. The file is placed within the include path, so you do
not need to manage it manually. Configuration options are listed and documented
in the :file:`anj_config.h.in` file.

The following is an example :file:`CMakeLists.txt` file that demonstrates how to
build an application with Anjay Lite using a custom configuration:

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.16.0)
   project(MyAnjayLiteApp C)

   # Set custom configuration of Anjay Lite, if required
   set(ANJ_WITH_EXTRA_WARNINGS OFF)

   # Set the path to Anjay Lite CMake config directory
   set(anjay_lite_DIR "<anjay_lite_root>/cmake")

   # Find Anjay Lite package
   find_package(anjay_lite REQUIRED)

   # Define the executable target and its source file(s)
   add_executable(my_application main.c)

   # Link Anjay Lite to the executable
   target_link_libraries(my_application PRIVATE
                         anj
                         anj_extra_warning_flags)

.. note::
   ``anj_extra_warning_flags`` is a CMake **INTERFACE** target that injects
   extended warning flags when using GCC or Clang.
   It doesn't produce binaries and is linked automatically when the
   ``ANJ_WITH_EXTRA_WARNINGS`` option (ON by default) is enabled. You
   can simply add ``anj_extra_warning_flags`` to ``target_link_libraries`` if
   you want compile your sources with the same flags, but it is not required.

Now you can simply build your application:

.. code-block:: bash

   mkdir build
   cd build
   cmake ..
   make -j

Build without including Anjay Lite as a package
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you are building Anjay Lite as part of a larger project or targeting a
non-host platform, you may integrate its sources directly using a custom build
system setup. This approach is suitable for cross-compilation environments,
where using :code:`find_package` is not practical or when CMake is not the
target build system.

.. tab-set::

   .. tab-item:: Flat build without build system

      Before running this example, it is required to have a directory structure
      similar to following:

      .. code-block:: none

         project/
         ├── main.c
         └── anjay_lite/

      The following example compiles a sample application with the Anjay Lite
      library in a Unix-like environment:

      .. code-block:: bash

         # configuration
         mkdir -p config/anj
         cp anjay_lite/include_public/anj/anj_config.h.in config/anj/anj_config.h.in

         # renaming anj_config.h.in to the anj_config.h
         # edit this file before, as described earlier
         mv config/anj/anj_config.h.in config/anj/anj_config.h

         # building
         cc \
            -Iconfig \
            -Ianjay_lite/include_public \
            main.c \
            $(find anjay_lite/src -name '*.c') \
            -lm

      See :ref:`integrating-anjay-lite` for details regarding config files.

      Final directory structure:

      .. code-block:: none

         project/
         ├── main.c
         ├── config/
         │   ├── anj/
         │   │   └── anj_config.h
         ├── anjay_lite/
         └── <build_artifacts>

   .. tab-item:: CMake without find_package

      Before running this example, it is required to have a directory structure
      similar to following:

      .. code-block:: none

         project/
         ├── main.c
         ├── CMakeLists.txt
         ├── config/
         │   ├── anj/
         │   │   └── anj_config.h
         └── anjay_lite/

      .. note::

         The configuration files have to be provided manually. See
         :ref:`integrating-anjay-lite` for details.

      The following example :file:`CMakeLists.txt` can be used to build an
      application with Anjay Lite using custom configuration:

      .. code-block:: cmake

         cmake_minimum_required(VERSION 3.16.0)
         project(MyAnjayLiteApp C)

         # Set the path to the Anjay Lite source directory
         set(ANJAY_LITE_PATH "anjay_lite")

         # Recursively collect all .c source files from Anjay Lite
         file(GLOB_RECURSE ANJAY_LITE_SOURCES
            ${ANJAY_LITE_PATH}/src/*.c
         )

         # Create the library
         add_library(anjay_lite STATIC
            ${ANJAY_LITE_SOURCES}
         )

         # Add include directories
         target_include_directories(anjay_lite
            PUBLIC
               # Anjay Lite public API headers
               ${ANJAY_LITE_PATH}/include_public
               # App-specific configuration headers for Anjay Lite
               ${CMAKE_CURRENT_SOURCE_DIR}/config
         )

         # Add your own application source files
         set(APP_SOURCES
            main.c
         )

         # Create the executable
         add_executable(my_application
            ${APP_SOURCES}
         )

         # Link libraries with application
         target_link_libraries(my_application PRIVATE
            # Anjay Lite library
            anjay_lite
            # Math library
            m
         )

      Now you can simply build your application:

      .. code-block:: bash

         mkdir build
         cd build
         cmake ..
         make -j

Build and run tests
-------------------

Anjay Lite provides a collection of example applications that serve as practical
starting points for developing your own solutions. It also includes a
comprehensive test suite to support integration and debugging efforts.

Build tests
^^^^^^^^^^^

To build all tests, run the following commands from the project root directory:

.. code-block:: bash

   mkdir build
   cd build/
   cmake ..
   make -j

The top-level :file:`CMakeLists.txt` file acts as a wrapper that organizes all
example and test projects. You can also build individual test suites directly
from their respective directories. For example, to build tests for the
``anj/core`` module:

.. code-block:: bash

   cd tests/anj/core
   mkdir build
   cd build/
   cmake ..
   make -j

All compiled tests binaries are placed in the ``build/`` directory, each within
its corresponding subdirectory.

Run tests
^^^^^^^^^

After building, run tests from the ``build/`` directory by executing
the compiled binaries. For example:

.. code-block:: bash

   core_tests/core_tests

To run the same tests with Valgrind, use the corresponding ``make`` target with
the ``_with_valgrind`` suffix:

.. code-block:: bash

   make core_tests_with_valgrind

Next steps
----------

Your development environment is now set up, and all example applications and
tests have been successfully built and executed. Continue by exploring
specific features, object implementations, or integration workflows described
in the subsequent sections of this documentation.
