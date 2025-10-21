..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Porting Guide for non-POSIX Platforms
=====================================

By default, Anjay Lite makes use of POSIX-specific interfaces for retrieving
time and handling network traffic.
If your toolchain doesn't provide these interfaces, you need to implement custom replacements.

The documents below provide additional information about the specific functions
that need to be implemented.

.. toctree::
   :titlesonly:

   PortingGuideForNonPOSIXPlatforms/TimeAPI
   PortingGuideForNonPOSIXPlatforms/NetworkingAPI
