..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

.. _integrations:

Integrations
============

This section provides example integrations of Anjay Lite with different platforms.

These examples are intended as reference implementations to help you get started with integrating Anjay Lite into your own projects.
While not exhaustive or production-ready, they demonstrate key concepts and typical integration steps.

Bare Metal Client integration
-----------------------------

This example showcases the integration of the Anjay Lite LwM2M Client SDK on bare metal platforms, such as ARM Cortex-M microcontrollers.
It provides a minimal implementation that operates without the need for an operating system or real-time operating system (RTOS) features.
As a demonstration, it includes a simple Temperature Object that measures the internal core temperature.

The example targets the STM32 Nucleo-U385RG-Q board and utilizes a BG96 cellular module for network connectivity.
A custom asynchronous driver has been implemented to support the BG96 module, enabling reliable communication in a bare metal environment.

Project setup
^^^^^^^^^^^^^
The project is available on our `GitHub <https://www.github.com/AVSystem/Anjay-lite-bare-metal-client>`_. Detailed setup instructions
can be found in the project's `README <https://www.github.com/AVSystem/Anjay-lite-bare-metal-client/blob/master/README.md>`_.


.. important::
   Note that you need to clone the project with submodules, as it uses Anjay Lite as a submodule.
   You can do this with the following command:

   .. code-block:: bash

      git clone https://www.github.com/AVSystem/Anjay-lite-bare-metal-client --recurse-submodules
