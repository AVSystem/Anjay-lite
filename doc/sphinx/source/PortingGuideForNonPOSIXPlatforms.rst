..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Porting guide for non-POSIX platforms
=====================================

By default, Anjay Lite makes use of POSIX-specific interfaces for retrieving time
and handling network traffic. If no such interfaces are provided by the
toolchain, the user needs to provide custom implementations.

The documents below provide additional information about the specific functions
that need to be implemented.

.. toctree::
   :titlesonly:

   PortingGuideForNonPOSIXPlatforms/TimeAPI
   PortingGuideForNonPOSIXPlatforms/NetworkingAPI
