..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Send method
===========

The **Send** operation allows a LwM2M Client to transmit data to the LwM2M Server  
without receiving an explicit request from the Server.

To create a **Send** message, the client populates the ``anj_send_request_t`` structure  
with the desired resource records. These represent the data to be sent. Once the  
structure is prepared, the client invokes the ``anj_send_new_request()`` function  
to register the message for transmission.

The payload can contain values from multiple resources, even if they belong to  
different objects, or represent time-series data for a given resource.

.. note::
    Calling ``anj_send_new_request()`` does *not* send the message  
    immediately. Anjay Lite queues the request and sends it only when:

    - A registration session with the LwM2M Server is active.  
    - There is no other ongoing CoAP exchange in progress (e.g., a Server request,
      Update message, or Notification)

.. note::
   The **Send** operation requires either `SenML CBOR` or `LwM2M CBOR` support to be
   enabled in Anjay Lite. Choose the format based on your application's requirements.

   - `SenML CBOR` supports timestamps, which is useful for time-series data.
   - `LwM2M CBOR` cannot encode identical resource paths, but it typically produces
     more compact messages and is recommended when minimizing payload size is important.

Example
-------

.. note::
   Code related to this tutorial can be found under `examples/tutorial/BC-Send`
   in the Anjay Lite source directory and is based on `examples/tutorial/BC-BasicObjectImplementation`
   example.

First, set the queue size and enable **Send** operation (``ANJ_WITH_LWM2M_SEND``)
in `CMakeLists.txt`:


.. highlight:: cmake
.. snippet-source:: examples/tutorial/BC-Send/CMakeLists.txt
   :emphasize-lines: 8-9

    cmake_minimum_required(VERSION 3.16.0)

    project(anjay_lite_bc_send C)

    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_EXTENSIONS OFF)

    set(ANJ_WITH_LWM2M_SEND ON)
    set(ANJ_LWM2M_SEND_QUEUE_SIZE 1)

This setting determines how many Send messages Anjay Lite can queue at the  
same time.

Next, we make a few modifications to the loop in which we call  
``anj_core_step()``:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Send/src/main.c

    anj_res_value_t value;
    anj_time_monotonic_t next_read_time =
            anj_time_monotonic_add(anj_time_monotonic_now(),
                                   anj_time_duration_new(1, ANJ_TIME_UNIT_S));
    uint16_t send_id = 0;
    anj_io_out_entry_t records[MAX_RECORDS];
    fin_handler_data_t data = { 0 };

    while (true) {
        anj_core_step(&anj);
        update_temperature_obj_value();
        usleep(50 * 1000);
        if (anj_time_monotonic_lt(next_read_time, anj_time_monotonic_now())) {
            next_read_time = anj_time_monotonic_add(
                    anj_time_monotonic_now(),
                    anj_time_duration_new(1, ANJ_TIME_UNIT_S));
            if (data.record_idx < MAX_RECORDS) {
                if (anj_dm_res_read(&anj,
                                    &ANJ_MAKE_RESOURCE_PATH(3303, 0, 5700),
                                    &value)) {
                    log(L_ERROR, "Failed to read resource");
                } else {
                    records[data.record_idx].path =
                            ANJ_MAKE_RESOURCE_PATH(3303, 0, 5700);
                    records[data.record_idx].type = ANJ_DATA_TYPE_DOUBLE;
                    records[data.record_idx].value = value;
                    records[data.record_idx].timestamp =
                            anj_time_real_to_fscalar(anj_time_real_now(),
                                                     ANJ_TIME_UNIT_S);
                    data.record_idx++;
                }
            } else {
                log(L_WARNING,
                    "Records array full, abort send operation ID: "
                    "%u",
                    send_id);
                if (anj_send_abort(&anj, send_id)) {
                    log(L_ERROR, "Failed to abort send operation");
                } else {
                    data.record_idx = 0;
                    data.send_in_progress = false;
                }
            }
        }

        if (data.record_idx >= RECORDS_CNT_SEND_TRIGGER
                && !data.send_in_progress) {
            data.records_cnt = data.record_idx;
            data.records = records;
            data.send_in_progress = true;

            /* Record list full, request send */
            anj_send_request_t send_req = {
                .finished_handler = send_finished_handler,
                .data = (void *) &data,
                .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
                .records_cnt = data.records_cnt,
                .records = records
            };

            if (anj_send_new_request(&anj, &send_req, &send_id)) {
                log(L_ERROR, "Failed to request new send");
                data.send_in_progress = false;
            }
        }
    }

**How it works**

Here's what each key variable does:

    - ``anj_res_value_t value``: holds the latest value read from the resource.
    - ``uint64_t next_read_time``: defines when the next resource read should happen.
      It's updated every time we try to read the resource.
    - ``uint16_t send_id``: stores the current **Send** operation's ID. You will need
      this value only if you want to abort the operation by calling ``anj_send_abort``.
    - ``anj_io_out_entry_t records[MAX_RECORDS]``: stores the list of values to be sent.
    - ``fin_handler_data_t data`` tracks metadata that you want to process after the
      **Send** operation completes. The data structure looks like this:
      
    .. highlight:: c
    .. snippet-source:: examples/tutorial/BC-Send/src/main.c

        typedef struct fin_handler_data {
            size_t records_cnt;
            size_t record_idx;
            anj_io_out_entry_t *records;
            bool send_in_progress;
        } fin_handler_data_t;


Gather the data for the Send message
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Once per second, we attempt to call ``anj_dm_res_read`` to read the ``/3303/0/5700``
resource. If the read is successful, we create a new entry in the records array with:

    - the resource path
    - the data type
    - the current value
    - a timestamp

We use ``data.record_idx`` to track the next free slot in the array and increase
it after each successful read.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Send/src/main.c

    records[data.record_idx].path =
        ANJ_MAKE_RESOURCE_PATH(3303, 0, 5700);
    records[data.record_idx].type = ANJ_DATA_TYPE_DOUBLE;
    records[data.record_idx].value = value;
    records[data.record_idx].timestamp =
        anj_time_real_to_fscalar(anj_time_real_now(),
                                 ANJ_TIME_UNIT_S);
    data.record_idx++;

.. note::
   If a timestamp is not required, you may omit setting this field in the record.

.. note::
   The values we store in the ``records`` array may be gathered directly from
   the sensor object omiting the ``anj_dm_res_read`` call.

Prepare the Send message
^^^^^^^^^^^^^^^^^^^^^^^^

When ``data.record_idx`` reaches or exceeds ``RECORDS_CNT_SEND_TRIGGER``, it
means we've gathered enough data to send.

Start by updating the ``data`` structure:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Send/src/main.c

    data.records_cnt = data.record_idx;
    data.records = records;
    data.send_in_progress = true;

The ``send_in_progress`` flag indicates that a **Send** message is currently in progress.

.. note::
    If ``ANJ_LWM2M_SEND_QUEUE_SIZE`` is set to ``1``, only one **Send** request
    can be active at a time. To support more simultaneous operations, increase
    this setting.

The ``record_idx`` and ``records`` values are stored in the ``data`` structure
so they can later be cleaned up once the send completes. The ``data`` structure
is passed to the callback function that is invoked after the **Send** message
has been processed.

Now we create a Send request:  

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Send/src/main.c

    /* Record list full, request send */
    anj_send_request_t send_req = {
        .finished_handler = send_finished_handler,
        .data = (void *) &data,
        .content_format = ANJ_SEND_CONTENT_FORMAT_SENML_CBOR,
        .records_cnt = data.records_cnt,
        .records = records
    };

We configure the following fields in the request structure:

    - ``finished_handler``: a callback function that will be called after the **Send** operation completes.
    - ``data``: a pointer to the user-defined structure passed to the callback.
    - ``content_format``: specifies the encoding formatv.
    - ``records_cnt`` and ``records``: define the number of records and a pointer to the array containing them.

.. note::
   The ``records`` array passed in ``anj_send_request_t`` is not copied  
   internally. Its contents must remain unchanged and valid until the **Send** operation
   completes.

Schedule the send
^^^^^^^^^^^^^^^^^

Once the request is ready, pass it to ``anj_send_new_request()``. If the function
succeeds:

    - A new **Send** message is queued,
    - ``send_id`` stores its ID, which you can use later to cancel the **Send** operation if needed,
    - Anjay Lite will process the request during the subsequent ``anj_core_step()`` calls.

Send mesage completion
^^^^^^^^^^^^^^^^^^^^^^

Once Anjay Lite finishes processing the Send request, it calls the handler function
provided in the request to notify that the operation has completed:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Send/src/main.c

    static void
    send_finished_handler(anj_t *anjay, uint16_t send_id, int result, void *data_) {
        (void) anjay;
        (void) send_id;
        (void) result;

        assert(data_);
        fin_handler_data_t *data = (fin_handler_data_t *) data_;

        /* move the records not yet processed to the begining of the array */
        memmove(data->records,
                data->records + data->records_cnt,
                (MAX_RECORDS - data->records_cnt) * sizeof(anj_io_out_entry_t));

        data->record_idx = data->record_idx - data->records_cnt;
        data->send_in_progress = false;
    }

The logic inside this function can be adjusted to suit your application needs.

What this handler does:

    - Clear the ``send_in_progress`` flag to indicate readiness for the next Send operation.
    - Shifts any remaining unsent records to the front of the ``records`` array
      using ``memmove()`` to free up space for new data.

That's it! Your client is now ready to send data using the LwM2M **Send** method in Anjay Lite.
