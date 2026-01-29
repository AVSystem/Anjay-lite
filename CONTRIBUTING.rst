..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Contributing to Anjay Lite
==========================

Thank you for considering contributing to Anjay Lite!

Please take a moment to review this document in order to make the contribution process easy and effective for everyone involved.


Following these guidelines shows that you value the time and effort of the AVSystem team maintaining and developing this project, which is released under a source-available dual license. While Anjay Lite is an AVSystem product, we welcome community contributions, suggestions, and improvements—and we aim to provide constructive feedback and support throughout the contribution process.

Reporting issues
----------------

**If you find a security vulnerability, do NOT open an issue!** Please email `opensource@avsystem.com <mailto:opensource@avsystem.com>`_ instead. We will address the issue as soon as possible and will give you an estimate for when we have a fix and release available for an eventual public disclosure.

Use GitHub issue tracker for reporting other bugs. A great bug report should include:

- Affected library version.
- Platform information:

  - processor architecture,
  - operating system,
  - compiler version.
- Minimal code example or a list of steps required to reproduce the bug.
- In case of run-time errors, logs from ``strace``, ``valgrind`` and PCAP network traffic dumps are greatly appreciated.

Feature requests
----------------

If you think some feature is missing, or that something can be improved, do not hesitate to suggest how we can make it better! When doing that, use GitHub issue tracker to post a description of the feature and some explanation why you need it.

Code contributions
------------------

If you never submitted a pull request before, take a look at `this fantastic tutorial <https://egghead.io/courses/how-to-contribute-to-an-open-source-project-on-github>`_.

#. Fork the project.
#. Work on your contribution - follow guidelines described below.
#. Push changes to your fork.
#. Make sure all ``make check`` tests still pass.
#. `Create a pull request <https://help.github.com/articles/creating-a-pull-request-from-a-fork/>`_.
#. Wait for someone from Anjay Lite team to review your contribution and give feedback.

Code style
^^^^^^^^^^

All code should be fully C99-compliant and compile without warnings under ``gcc`` and ``clang``. Be sure you have enabled extra warnings by using ``cmake -DANJ_WITH_EXTRA_WARNINGS=ON``. Compiling and testing the code on multiple architectures (e.g. 32/64-bit x86, ARM, MIPS) is not required, but welcome.

General guidelines
^^^^^^^^^^^^^^^^^^
- Do not use GNU extensions.
- Use ``UPPER_CASE`` for constants and ``snake_case`` in all other cases.
- Prefer ``static`` functions. Use ``_anj_`` prefix for private functions shared between translation units and ``anj_`` prefix for types and public functions.
- Do not use global variables. Only constants are allowed in global scope.
- When using bitfields, make sure they are not saved to persistent storage nor sent over the network - their memory layout is implementation-defined, making them non-portable.
- Avoid recursion - when writing code for an embedded platform, it is important to determine a hard limit on the stack space used by a program.
- Include license information at the top of each file you add.

