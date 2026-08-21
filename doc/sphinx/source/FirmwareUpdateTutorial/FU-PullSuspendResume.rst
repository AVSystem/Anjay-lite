..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Suspending and resuming FOTA Pull downloads
===========================================

Overview
^^^^^^^^

A firmware download may need to be paused even though the transfer itself has
not failed. This may happen when the application reaches a communication
budget, leaves an allowed transmission window, or gets an indication from the
modem or network stack that connectivity is temporarily unavailable.

The CoAP Downloader provides ``anj_coap_downloader_suspend()`` and
``anj_coap_downloader_resume()`` for such cases. Suspending a download stops
network activity while preserving its progress. Once the application decides
that the transfer may continue, it can resume the same download instead of
starting it from the beginning.

This tutorial builds on :doc:`FU-PullBasicImplementation` and focuses only on
adding application-controlled suspension and resumption to an existing CoAP
Pull firmware download.

Project structure
^^^^^^^^^^^^^^^^^

The source files used in this tutorial are located in the
``examples/tutorial/firmware-update-downloader-suspend-resume`` directory:

.. code::

    examples/tutorial/firmware-update-downloader-suspend-resume/
    ├── CMakeLists.txt
    └── src
        ├── firmware_update.c (changed)
        ├── firmware_update.h (changed)
        └── main.c (changed)

Example scenario
^^^^^^^^^^^^^^^^

The example demonstrates a simple application-defined communication budget.
The firmware download is suspended after every 100 received data chunks. The
application waits for 30 seconds, resumes the transfer and moves the next limit
by another 100 chunks.

.. note::

    The number of chunks is used only to keep the example deterministic and
    easy to run. A chunk delivered by the CoAP Downloader is not necessarily
    equivalent to a physical, IP, UDP or cellular-network packet. A production
    application that enforces a real traffic or message budget should normally
    obtain that information from the modem or networking layer.

Expose download progress to the application
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The transfer-window policy is implemented in ``main.c``, while downloader
events are handled in ``firmware_update.c``. The Firmware Update module
therefore exposes only the information needed by the policy: whether a download
is active and how many data chunks have been received.

The chunk counter is advanced whenever the downloader delivers firmware data:

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-downloader-suspend-resume/src/firmware_update.c

    case ANJ_COAP_DOWNLOADER_STATUS_DOWNLOADING: {
        assert(fu->firmware_file != NULL);

        ++fu->received_chunks;
        log(L_INFO,
            "Writing %zu bytes at offset %zu (chunk %zu)",
            data_len,
            fu->offset,
            fu->received_chunks);
        fu->offset += data_len;

``main.c`` accesses the state through small helper functions and does not need
access to the CoAP Downloader instance itself:

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-downloader-suspend-resume/src/firmware_update.c

    bool fw_update_is_download_active(void) {
        return firmware_update.download_active;
    }

    size_t fw_update_get_received_chunk_count(void) {
        return firmware_update.received_chunks;
    }

    int fw_update_suspend_download(void) {
        return anj_coap_downloader_suspend(&coap_downloader);
    }

    int fw_update_resume_download(void) {
        return anj_coap_downloader_resume(&coap_downloader);
    }

Implement transfer windows
^^^^^^^^^^^^^^^^^^^^^^^^^^

The application keeps the next chunk limit and the time at which a suspended
download should be resumed. When the current limit is reached, it requests
suspension, schedules resumption 30 seconds later and advances the next limit by
100 chunks.

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-downloader-suspend-resume/src/main.c
    :emphasize-lines: 12,27

    static void handle_download_window(download_control_t *control) {
        if (!fw_update_is_download_active()) {
            reset_download_control(control);
            return;
        }

        const size_t received_chunks = fw_update_get_received_chunk_count();

        if (!control->is_suspended
                && received_chunks >= control->next_suspend_at_chunk) {
            if (!fw_update_suspend_download()) {
                control->is_suspended = true;
                control->resume_at = anj_time_monotonic_add(
                        anj_time_monotonic_now(),
                        anj_time_duration_new(DOWNLOAD_SUSPEND_SECONDS,
                                            ANJ_TIME_UNIT_S));
                control->next_suspend_at_chunk += DOWNLOAD_CHUNK_LIMIT_STEP;
                log(L_INFO,
                    "Downloaded %zu chunks; suspending firmware download for "
                    "%d "
                    "seconds",
                    received_chunks, DOWNLOAD_SUSPEND_SECONDS);
            } else {
                log(L_ERROR, "Failed to suspend firmware download - invalid state");
            }
            return;
        }

        if (control->is_suspended
                && anj_time_monotonic_geq(anj_time_monotonic_now(),
                                        control->resume_at)) {
            if (!fw_update_resume_download()) {
                control->is_suspended = false;
                log(L_INFO, "Resuming firmware download");
            } else {
                log(L_ERROR, "Failed to resume firmware download - invalid state");
            }
        }
    }


The policy is evaluated from the main loop together with the normal Anjay Lite
and Firmware Update processing:

.. highlight:: c
.. snippet-source:: examples/tutorial/firmware-update-downloader-suspend-resume/src/main.c
    :emphasize-lines: 4

    while (true) {
        anj_core_step(&anj);
        fw_update_process();
        handle_download_window(&download_control);

        struct timespec ts = { 0, 50 * 1000 * 1000 }; // 50 ms
        nanosleep(&ts, NULL);
    }

.. note::

    Suspension is asynchronous. Calling ``anj_coap_downloader_suspend()``
    starts the suspension process, but the downloader may need several
    calls to ``anj_coap_downloader_step()`` before it can be resumed.

Connectivity loss
^^^^^^^^^^^^^^^^^

A communication budget is only one reason to suspend a download. On an embedded
device, the modem or network stack may provide a direct indication that
connectivity has been lost. If the application knows that communication is not
currently possible, it can suspend an active firmware download instead of
letting it generate unnecessary traffic attempts.

When connectivity becomes available again, the same application-level event can
be used to resume the suspended download. The source of such connectivity
information is platform-specific; the suspend/resume API does not require a
particular modem, operating system or network-stack integration.

Using suspend/resume together with retry
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Suspend/resume and automatic retry address different conditions and can be used
together. The retry policy configured for the CoAP Downloader handles transient
communication failures automatically, as described in
:doc:`FU-PullBasicImplementation`. Suspension is an explicit application
decision to stop download activity for a longer or externally defined period.

Suspending a download also stops any pending retry handling. The downloader
remains suspended until the application explicitly resumes it. Calling
``anj_coap_downloader_resume()`` starts a new download attempt immediately,
without waiting for ``anj_coap_downloader_configuration_t.retry_delay``.
Afterwards, the configured retry policy continues to apply to subsequent
communication failures.
