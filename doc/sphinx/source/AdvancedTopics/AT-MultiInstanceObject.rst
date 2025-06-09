..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Multi-Instance Object
=====================

This section describes implementation of a **multi-instance Object** defined in
`OMA LwM2M Registry <https://www.openmobilealliance.org/specifications/registries/objects>`_.

LwM2M Objects may have multiple Instances; sometimes the number of Instances may
be variable and the Instances themselves may be creatable via LwM2M. Examples of
such Objects include the Object that manages connections to LwM2M Servers or
Object that represents optional software packages installed on the device.

As an example, we are going to implement
`Temperature Object with ID 3303 in version 1.1 <https://raw.githubusercontent.com/OpenMobileAlliance/lwm2m-registry/prod/version_history/3303-1_1.xml>`_ by expanding the code developed in `Basic Object Implementation <../BasicClient/BC-BasicObjectImplementation.html>`_.
Only the differences that need to be applied are described in this article.

Implementing the Object
-----------------------

.. note::
   Code related to this tutorial can be found under `examples/tutorial/AT-MultiInstanceObject`
   in the Anjay Lite source directory and is based on `examples/tutorial/BC-BasicObjectImplementation`
   example.

Object and Object Instances State
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To allow keeping state of two independent Object Instances, first we need to
introduce separate structures for an Object Instance (keeping Instance ID and
Resources values) and for the Object itself, keeping object definition and its instances.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObject/src/temperature_obj.c

    typedef struct {
        anj_iid_t iid;
        double sensor_value;
        double min_sensor_value;
        double max_sensor_value;
        char application_type[TEMP_OBJ_APPL_TYPE_MAX_SIZE];
        char application_type_cached[TEMP_OBJ_APPL_TYPE_MAX_SIZE];
    } temp_obj_inst_t;

    typedef struct {
        anj_dm_obj_t obj;
        anj_dm_obj_inst_t insts[TEMP_OBJ_NUMBER_OF_INSTANCES];
        temp_obj_inst_t temp_insts[TEMP_OBJ_NUMBER_OF_INSTANCES];
    } temp_obj_ctx_t;


Now, let's add a helper function to get a pointer to an instance with given `iid` (**Instance ID**).

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObject/src/temperature_obj.c

    static temp_obj_inst_t *get_temp_inst(const anj_dm_obj_t *obj, anj_iid_t iid) {
        temp_obj_ctx_t *ctx = ANJ_CONTAINER_OF(obj, temp_obj_ctx_t, obj);
        for (uint16_t idx = 0; idx < TEMP_OBJ_NUMBER_OF_INSTANCES; idx++) {
            if (ctx->temp_insts[idx].iid == iid) {
                return &ctx->temp_insts[idx];
            }
        }

        return NULL;
    }


Read, write and execute Handlers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

All object handlers that are called from Anjay Lite DM (Data Model) Module have
a `anj_iid_t iid` parameter, that was previously ignored. Now we will make use of it.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObject/src/temperature_obj.c
    :emphasize-lines: 3, 10-11, 15, 22, 29-30, 35, 43, 51-52, 56-57

    static int res_read(anj_t *anj,
                        const anj_dm_obj_t *obj,
                        anj_iid_t iid,
                        anj_rid_t rid,
                        anj_riid_t riid,
                        anj_res_value_t *out_value) {
        (void) anj;
        (void) riid;

        temp_obj_inst_t *temp_inst = get_temp_inst(obj, iid);
        assert(temp_inst);

        switch (rid) {
        case RID_SENSOR_VALUE:
            out_value->double_value = temp_inst->sensor_value;
            break;
        //...
    }

    static int res_write(anj_t *anj,
                        const anj_dm_obj_t *obj,
                        anj_iid_t iid,
                        anj_rid_t rid,
                        anj_riid_t riid,
                        const anj_res_value_t *value) {
        (void) anj;
        (void) riid;

        temp_obj_inst_t *temp_inst = get_temp_inst(obj, iid);
        assert(temp_inst);

        switch (rid) {
        case RID_APPLICATION_TYPE:
            return anj_dm_write_string_chunked(value,
                                            temp_inst->application_type,
                                            TEMP_OBJ_APPL_TYPE_MAX_SIZE, NULL);
            break;
        //...
    }

    static int res_execute(anj_t *anj,
                        const anj_dm_obj_t *obj,
                        anj_iid_t iid,
                        anj_rid_t rid,
                        const char *execute_arg,
                        size_t execute_arg_len) {
        (void) anj;
        (void) execute_arg;
        (void) execute_arg_len;

        temp_obj_inst_t *temp_inst = get_temp_inst(obj, iid);
        assert(temp_inst);

        switch (rid) {
        case RID_RESET_MIN_MAX_MEASURED_VALUES: {
            temp_inst->min_sensor_value = temp_inst->sensor_value;
            temp_inst->max_sensor_value = temp_inst->sensor_value;
            return 0;
        }

        //...
    }


Object definition and Initialization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First, we need to change the `anj_dm_obj_inst_t` structure to an array.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObject/src/temperature_obj.c

    static anj_dm_obj_inst_t INSTS[TEMP_OBJ_NUMBER_OF_INSTANCES] = {
        {
            .iid = 1,
            .res_count = TEMPERATURE_RESOURCES_COUNT,
            .resources = RES
        },
        {
            .iid = 2,
            .res_count = TEMPERATURE_RESOURCES_COUNT,
            .resources = RES
        }
    };

And then we can add a static initialization of the Object, including its
Instances.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObject/src/temperature_obj.c

    static temp_obj_ctx_t temperature_obj = {
        .obj = {
            .oid = 3303,
            .version = "1.1",
            .insts = INSTS,
            .handlers = &TEMP_OBJ_HANDLERS,
            .max_inst_count = TEMP_OBJ_NUMBER_OF_INSTANCES
        },
        .temp_insts[0].iid = 1,
        .temp_insts[0].application_type = "Sensor_1",
        .temp_insts[0].sensor_value = 10.0,
        .temp_insts[0].min_sensor_value = 10.0,
        .temp_insts[0].max_sensor_value = 10.0,

        .temp_insts[1].iid = 2,
        .temp_insts[1].application_type = "Sensor_2",
        .temp_insts[1].sensor_value = 20.0,
        .temp_insts[1].min_sensor_value = 20.0,
        .temp_insts[1].max_sensor_value = 20.0
    };


Supporting transactional Writes
-------------------------------

Transactions are performed on an Object level, not an Object Instance Level.
This means we need to back up the state of all writable resources **in all Object Instances**.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObject/src/temperature_obj.c
    :emphasize-lines: 6-8, 26-30

    static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
        (void) anj;

        temp_obj_ctx_t *ctx = ANJ_CONTAINER_OF(obj, temp_obj_ctx_t, obj);
        for (int i = 0; i < TEMP_OBJ_NUMBER_OF_INSTANCES; i++) {
            temp_obj_inst_t *temp_inst = &ctx->temp_insts[i];
            memcpy(temp_inst->application_type_cached, temp_inst->application_type,
                TEMP_OBJ_APPL_TYPE_MAX_SIZE);
        }
        return 0;
    }

    static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
        (void) anj;
        (void) obj;
        // Perform validation of the object
        return 0;
    }

    static void transaction_end(anj_t *anj, const anj_dm_obj_t *obj, int result) {
        (void) anj;

        if (result) {
            // restore cached data
            temp_obj_ctx_t *ctx = ANJ_CONTAINER_OF(obj, temp_obj_ctx_t, obj);
            for (int i = 0; i < TEMP_OBJ_NUMBER_OF_INSTANCES; i++) {
                temp_obj_inst_t *temp_inst = &ctx->temp_insts[i];
                memcpy(temp_inst->application_type,
                    temp_inst->application_type_cached,
                    TEMP_OBJ_APPL_TYPE_MAX_SIZE);
            }
        }
    }
