..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Notifications support
=====================

Overview
^^^^^^^^

Some resources, like sensor readings, change over time. To track these changes, the LwM2M Server can use the Observe operation. This allows it to receive notifications whenever a value changes or meets certain conditions.

.. seealso::
    For more details about the Observe operation and observation attributes,
    refer to the `LwM2M TS: Core <https://www.openmobilealliance.org/release/LightweightM2M/V1_2-20201110-A/HTML-Version/OMA-TS-LightweightM2M_Core-V1_2-20201110-A.html#5-1-2-0-512-Attributes-Classification>`_
    specification.

Notify the library about changes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The LwM2M Server can observe changes in resources using the Observe operation. When resource values change independently of LwM2M operations (such as Write or Create), you must inform the Anjay Lite library manually by calling the ``anj_core_data_model_changed()`` function:

.. highlight:: c
.. snippet-source:: include_public/anj/core.h

    void anj_core_data_model_changed(anj_t *anj,
                                    const anj_uri_path_t *path,
                                    anj_core_change_type_t change_type);


Call this function with the correct ``change_type`` based on what changed:

- Use ``ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED`` when a resource or resource instance value has been updated.

- Use ``ANJ_CORE_CHANGE_TYPE_ADDED`` or ``ANJ_CORE_CHANGE_TYPE_DELETED`` when an object instance or resource instance has been added or removed. 

  .. note:: 

      If the removed entity was being observed, its observation will be
      automatically removed.


Once ``anj_core_data_model_changed()`` is called, the library will respond to
the change during subsequent calls to ``anj_core_step()``. If the affected
resource is under observation and the configured conditions are met, a Notify
message will be prepared and eventually sent to the LwM2M Server.

.. important::
    ``anj_core_data_model_changed()`` is not only used to support notifications,
    but is also essential for other mechanisms, including:

    - Triggering an Update message that includes a list of all objects and their
      instances when the set of registered object instances changes.
    - Reading certain resources that manage registration state, such as the
      *Lifetime*, *Mute Send*, or *Notification Storing* resources of the Server
      object.

.. note::
    The number of concurrent observations and the number of entities with
    observation attributes are limited by the configuration options:
    ``ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER`` and
    ``ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER``.

Example: Add notifications to a Temperature object
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This example shows how to add notification support to a Temperature object. You'll
use the ``anj_core_data_model_changed()`` function to inform the library when resource
values change.

Update resource values
----------------------

The ``update_temperature_obj_value()`` function updates three resources
periodically: *Sensor Value*, *Min Measured Value*, and *Max Measured Value*.

Call ``anj_core_data_model_changed()`` for each resource whose value changes:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Notifications/src/temperature_obj.c
    :emphasize-lines: 1, 4-5, 7-12, 15-19, 23-27

    void update_temperature_obj_value(anj_t *anj) {
        temp_obj_ctx_t *ctx = get_ctx();

        double prev_temp_value = ctx->sensor_value;
        ctx->sensor_value = next_temperature_with_limit(ctx->sensor_value, 0.2);

        if (prev_temp_value != ctx->sensor_value) {
            anj_core_data_model_changed(anj,
                                        &ANJ_MAKE_RESOURCE_PATH(TEMPERATURE_OID, 0,
                                                                RID_SENSOR_VALUE),
                                        ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
        }
        if (ctx->sensor_value < ctx->min_sensor_value) {
            ctx->min_sensor_value = ctx->sensor_value;
            anj_core_data_model_changed(
                    anj,
                    &ANJ_MAKE_RESOURCE_PATH(TEMPERATURE_OID, 0,
                                            RID_MIN_MEASURED_VALUE),
                    ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
        }
        if (ctx->sensor_value > ctx->max_sensor_value) {
            ctx->max_sensor_value = ctx->sensor_value;
            anj_core_data_model_changed(
                    anj,
                    &ANJ_MAKE_RESOURCE_PATH(TEMPERATURE_OID, 0,
                                            RID_MAX_MEASURED_VALUE),
                    ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
        }
    }

If any of these resources are observed by the LwM2M Server, the library will send a Notify message when the value changes.

.. note::
    Do not call ``anj_core_data_model_changed()`` when the change is directly
    triggered by an LwM2M operation (e.g., Write or Create). In such cases, all
    required actions are handled internally by the library.

.. warning::
    It's crucial to call ``anj_core_data_model_changed()`` **only after**
    ensuring that the subsequent ``res_read()`` call will return the updated
    resource value. In this simple example, since ``res_read()`` just returns a
    value from memory, the function can be called right after assigning new
    values to fields in ``temp_obj_ctx_t``. Anjay Lite may perform a read
    immediately during a call to ``anj_core_data_model_changed()``, and may
    continue reading during future ``anj_core_step()`` calls.

    This behavior differs from Anjay 3, where ``anjay_notify_changed()`` and
    ``anjay_notify_instances_changed()`` can be invoked at any time, as long as
    the new resource values are available before the next call to
    ``anjay_event_loop_run()``.

Handle execute operations
-------------------------
The *Reset Min* and *Max Measured Values* resource is implemented using ``res_execute()``. Although it is triggered by an LwM2M command, it changes other resource values, so you must still notify the library:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Notifications/src/temperature_obj.c
    :emphasize-lines: 20-29

    static int res_execute(anj_t *anj,
                           const anj_dm_obj_t *obj,
                           anj_iid_t iid,
                           anj_rid_t rid,
                           const char *execute_arg,
                           size_t execute_arg_len) {
        (void) anj;
        (void) obj;
        (void) iid;
        (void) execute_arg;
        (void) execute_arg_len;

        temp_obj_ctx_t *temp_obj_ctx = get_ctx();

        switch (rid) {
        case RID_RESET_MIN_MAX_MEASURED_VALUES: {
            temp_obj_ctx->min_sensor_value = temp_obj_ctx->sensor_value;
            temp_obj_ctx->max_sensor_value = temp_obj_ctx->sensor_value;

            anj_core_data_model_changed(
                    anj,
                    &ANJ_MAKE_RESOURCE_PATH(TEMPERATURE_OID, 0,
                                            RID_MIN_MEASURED_VALUE),
                    ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
            anj_core_data_model_changed(
                    anj,
                    &ANJ_MAKE_RESOURCE_PATH(TEMPERATURE_OID, 0,
                                            RID_MAX_MEASURED_VALUE),
                    ANJ_CORE_CHANGE_TYPE_VALUE_CHANGED);
            return 0;
        }
        default:
            break;
        }

        return ANJ_DM_ERR_NOT_FOUND;
    }

.. note::

    It is not required to check whether the value actually changed before calling ``anj_core_data_model_changed()``. For simplicity, this check is skipped in the ``res_execute()`` callback.
    However, doing so helps avoid unnecessary Notify messages—especially for resources that are observed without any conditions (like thresholds). In these cases, the library does not remember the last value it sent, so it may send unnecessary notifications unless your application filters them out.
 
Update function declaration
---------------------------
Update the ``update_temperature_obj_value()`` function to include the ``anj_t *anj`` parameter, since it is required by ``anj_core_data_model_changed()``:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Notifications/src/temperature_obj.h
    :emphasize-lines: 8, 10

    /**
     * @brief Updates the sensor value and adjusts min/max tracked values.
     *
     * Simulates a new temperature reading for the given object by applying a small
     * random fluctuation to the current value. Also updates the minimum and maximum
     * recorded values based on the new reading.
     *
     * @param anj Pointer to the Anjay Lite instance.
     */
    void update_temperature_obj_value(anj_t *anj);

Call the function in the main loop
----------------------------------
.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Notifications/src/main.c
    :emphasize-lines: 7-9

    int main(int argc, char *argv[]) {
        // ...

        while (true) {
            anj_core_step(&anj);

            if (!anj_core_ongoing_operation(&anj)) {
                update_temperature_obj_value(&anj);
            }
            usleep(50 * 1000);
        }
        return 0;
    }

See :ref:`Concurrent data model modifications<concurrent_dm_modifications>` for more.

That's it. Your application now supports Observe/Notify for dynamic resources like temperature readings.
