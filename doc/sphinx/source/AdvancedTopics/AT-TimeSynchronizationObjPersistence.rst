..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Time synchronization persistence support
========================================

Overview
--------

This document complements :doc:`AT-TimeSynchronization` and explains how to
persist the state of the NTP Time Synchronization Object.

The NTP module is designed to integrate with the generic persistence mechanism
used in Anjay Lite. This allows the application to easily save the NTP object
state across device restarts.

.. note::
   Code related to this tutorial can be found under
   `examples/tutorial/AT-TimeSynchronizationPersistence` in the Anjay Lite source directory.

.. note::
    A detailed description of the persistence mechanism, including object storage
    and restoration patterns, is provided in the :doc:`AT-Persistence` tutorial.

Built-in object persistence
---------------------------

The Time Synchronization Object instance managed by the NTP module can be
saved to and restored from non-volatile storage using the helper functions:

* ``anj_ntp_obj_restore()`` - restores the NTP Object instance from
  previously stored data during startup,

* ``anj_ntp_obj_store()`` - serializes the current NTP Object instance, so it
  can be written to persistent storage.

To use persistence API set the CMake option ``ANJ_WITH_PERSISTENCE``.

Initialization with persistence restore
---------------------------------------

When the application starts, it attempts to restore the NTP Object state
from persistent storage using ``anj_ntp_obj_restore()``. If the restoration
succeeds, the NTP module continues using the previously saved configuration.
Please note that ``anj_ntp_obj_restore()`` is called after ``anj_ntp_init()``
and this is slightly different from the usual pattern described in the
:doc:`AT-Persistence` tutorial.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TimeSynchronizationPersistence/src/main.c

    if (anj_ntp_init(&anj, &ntp, &ntp_config)) {
        log(L_ERROR, "Failed to initialize NTP module");
        return -1;
    }

    FILE *file = fopen(PERSISTENCE_NTP_FILE, "r");
    if (file) {
        anj_persistence_context_t ctx =
                anj_persistence_restore_context_create(persistence_read, file);
        if (anj_ntp_obj_restore(&ntp, &ctx)) {
            fclose(file);
            remove(PERSISTENCE_NTP_FILE);
        } else {
            fclose(file);
        }
    }


Persisting state after object update
------------------------------------

When the LwM2M Server updates Time Synchronization Object resources (for
example, changes the NTP server address or synchronization period), the NTP
module reports this via the ANJ_NTP_STATUS_OBJECT_UPDATED status in the
event callback. The application can treat this status as a trigger to call
``anj_ntp_obj_store()`` and persist the new configuration together with
other client state.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-TimeSynchronizationPersistence/src/main.c

    case ANJ_NTP_STATUS_OBJECT_UPDATED: {
        FILE *file = fopen(PERSISTENCE_NTP_FILE, "w+");
        if (!file) {
            log(L_ERROR, "Could not open persistence file for writing");
            return;
        }
        anj_persistence_context_t ctx =
                anj_persistence_store_context_create(persistence_write, file);
        if (anj_ntp_obj_store(ntp, &ctx)) {
            log(L_ERROR, "Failed to persist NTP object state");
            fclose(file);
            remove(PERSISTENCE_NTP_FILE);
            return;
        }
        fclose(file);
        log(L_INFO, "NTP object state persisted successfully");
        break;
    }

Summary
-------

This tutorial explained how to use Anjay Lite's persistence mechanism to
store and restore the state of the NTP Time Synchronization Object. By
integrating persistence into the application, the NTP module can maintain
its configuration across device restarts, ensuring consistent time synchronization
behavior.
