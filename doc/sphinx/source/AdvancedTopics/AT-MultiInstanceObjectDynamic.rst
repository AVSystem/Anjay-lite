..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Multi-Instance Object dynamic
=============================

Overview
--------

This guide explains how to implement a **multi-instance Object** with a dynamic
set of Instances, as defined in
`OMA LwM2M Registry <https://www.openmobilealliance.org/specifications/registries/objects>`_
**with dynamic set of Instances**.

As an example, we are going to implement
`Temperature Object with ID 3303 in version 1.1 <https://raw.githubusercontent.com/OpenMobileAlliance/lwm2m-registry/prod/version_history/3303-1_1.xml>`_ by extending the code developed in `Multi-instance Object Implementation <../AT-MultiInstanceObject.html>`_.
This document highlights only the additional steps and modifications needed.

Implement the Object
--------------------

.. note::
   Code related to this tutorial can be found under `examples/tutorial/AT-MultiInstanceObjectDynamic`
   in the Anjay Lite source directory and is based on `examples/tutorial/AT-MultiInstanceObject`
   example.

.. note::
    To generate code stubs for multi-instance objects with dynamic instance management, use the ``anjay_codegen.py`` script with the ``-ni`` and ``-di`` flags:

    .. code-block:: bash

        ./tools/anjay_codegen.py -i temperature_obj.xml -o temperature_obj.c -ni 5 -di

    For details, see the :ref:`Dynamic multiple instance object generation<multi-dynamic-instance-generator>` section.

Although the set of Object Instances is dynamic, you must still define the
maximum number of instances using ``TEMP_OBJ_NUMBER_OF_INSTANCES``.

Object and Object Instances state
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Since the number of instances is now dynamic, to support transactional operations,
cache entire Object Instances instead of individual Resources.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObjectDynamic/src/temperature_obj.c
    :emphasize-lines: 12, 14

    typedef struct {
        anj_iid_t iid;
        double sensor_value;
        double min_sensor_value;
        double max_sensor_value;
        char application_type[TEMP_OBJ_APPL_TYPE_MAX_SIZE];
    } temp_obj_inst_t;

    typedef struct {
        anj_dm_obj_t obj;
        anj_dm_obj_inst_t insts[TEMP_OBJ_NUMBER_OF_INSTANCES];
        anj_dm_obj_inst_t insts_cached[TEMP_OBJ_NUMBER_OF_INSTANCES];
        temp_obj_inst_t temp_insts[TEMP_OBJ_NUMBER_OF_INSTANCES];
        temp_obj_inst_t temp_insts_cached[TEMP_OBJ_NUMBER_OF_INSTANCES];
    } temp_obj_ctx_t;


Create and Delete handlers
^^^^^^^^^^^^^^^^^^^^^^^^^^

To support server-side Instance management, implement the ``inst_create`` and
``inst_delete`` handlers.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObjectDynamic/src/temperature_obj.c
    :emphasize-lines: 2-3

    static const anj_dm_handlers_t TEMP_OBJ_HANDLERS = {
        .inst_create = inst_create,
        .inst_delete = inst_delete,
        .res_read = res_read,
        .res_write = res_write,
        .res_execute = res_execute,
        .transaction_begin = transaction_begin,
        .transaction_validate = transaction_validate,
        .transaction_end = transaction_end,
    };


Anjay Lite requires the instances to be sorted in ascending ``iid`` order in the
``anj_dm_obj_inst_t::insts``


.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObjectDynamic/src/temperature_obj.c

    // classic bubble sort for keeping the IID in the ascending order
    static void sort_instances(temp_obj_ctx_t *ctx) {
        for (uint16_t i = 0; i < TEMP_OBJ_NUMBER_OF_INSTANCES - 1; i++) {
            for (uint16_t j = i + 1; j < TEMP_OBJ_NUMBER_OF_INSTANCES; j++) {
                if (ctx->temp_insts[i].iid > ctx->temp_insts[j].iid) {
                    // swap temp_insts
                    temp_obj_inst_t tmp_temp = ctx->temp_insts[i];
                    ctx->temp_insts[i] = ctx->temp_insts[j];
                    ctx->temp_insts[j] = tmp_temp;

                    // swap insts
                    anj_dm_obj_inst_t tmp_inst = ctx->insts[i];
                    ctx->insts[i] = ctx->insts[j];
                    ctx->insts[j] = tmp_inst;
                }
            }
        }
    }

    static int inst_create(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
        (void) anj;
        assert(iid != ANJ_ID_INVALID);
        temp_obj_ctx_t *ctx = get_ctx();

        // find an unitialized instance and use it
        bool found = false;
        for (uint16_t idx = 0; idx < TEMP_OBJ_NUMBER_OF_INSTANCES; idx++) {
            if (ctx->temp_insts[idx].iid == ANJ_ID_INVALID) {
                ctx->temp_insts[idx].iid = iid;
                ctx->insts[idx].iid = iid;
                found = true;
                break;
            }
        }
        if (!found) {
            // no free instance found
            return -1;
        }
        sort_instances(ctx);
        return 0;
    }

    static int inst_delete(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
        (void) anj;
        temp_obj_ctx_t *ctx = get_ctx();
        for (uint16_t idx = 0; idx < TEMP_OBJ_NUMBER_OF_INSTANCES; idx++) {
            if (ctx->temp_insts[idx].iid == iid) {
                ctx->insts[idx].iid = ANJ_ID_INVALID;
                ctx->temp_insts[idx].iid = ANJ_ID_INVALID;
                sort_instances(ctx);
                return 0;
            }
        }
        return ANJ_DM_ERR_NOT_FOUND;
    }


If the **Create** operation does not specify an `iid`, Anjay Lite assigns one and
passes it to the `inst_create handler`.


Object definition and initialization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Begin by defining a static `temperature_obj` structure that holds the Object metadata:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObjectDynamic/src/temperature_obj.c

    static temp_obj_ctx_t temperature_obj = {
        .obj = {
            .oid = 3303,
            .version = "1.1",
            .handlers = &TEMP_OBJ_HANDLERS,
            .max_inst_count = TEMP_OBJ_NUMBER_OF_INSTANCES
        }
    };

Next, create an initialization function to populate the `insts` and `temp_insts` arrays:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObjectDynamic/src/temperature_obj.c

    void temperature_obj_init(void) {
        // initialize the object with 0 instances
        for (int i = 0; i < TEMP_OBJ_NUMBER_OF_INSTANCES; i++) {
            temperature_obj.insts[i].res_count = TEMPERATURE_RESOURCES_COUNT;
            temperature_obj.insts[i].resources = RES;
            temperature_obj.insts[i].iid = ANJ_ID_INVALID;
            temperature_obj.temp_insts[i].iid = ANJ_ID_INVALID;
        }

        temperature_obj.obj.insts = temperature_obj.insts;

        temp_obj_inst_t *inst;
        // initilize 1st instance
        inst = &temperature_obj.temp_insts[0];
        temperature_obj.insts[0].iid = 1;
        inst->iid = 1;
        snprintf(inst->application_type, sizeof(inst->application_type),
                "Sensor_1");
        inst->sensor_value = 10.0;
        inst->min_sensor_value = 10.0;
        inst->max_sensor_value = 10.0;

        // initialize 2nd instance
        inst = &temperature_obj.temp_insts[1];
        temperature_obj.insts[1].iid = 2;
        inst->iid = 2;
        snprintf(inst->application_type, sizeof(inst->application_type),
                "Sensor_2");
        inst->sensor_value = 20.0;
        inst->min_sensor_value = 20.0;
        inst->max_sensor_value = 20.0;
    }

Call the initialization function from `main()` before registering the Object with
the Anjay Lite Data Model:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObjectDynamic/src/main.c

    int main(int argc, char *argv[]) {
        // ...

        temperature_obj_init();
        if (anj_dm_add_obj(&anj, get_temperature_obj())) {
            log(L_ERROR, "install_temperature_object error");
            return -1;
        }

        // ...
    }


Support transactional Writes
----------------------------

To ensure consistent behavior during transactions, you must cache the complete
Object context — not just the writable Resources. This includes both the `insts`
and `temp_insts` arrays:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObjectDynamic/src/temperature_obj.c
    :emphasize-lines: 6-7, 28-30

    static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
        (void) anj;
        (void) obj;

        temp_obj_ctx_t *ctx = get_ctx();
        memcpy(ctx->insts_cached, ctx->insts, sizeof(ctx->insts));
        memcpy(ctx->temp_insts_cached, ctx->temp_insts, sizeof(ctx->temp_insts));
        return 0;
    }

    static int transaction_validate(anj_t *anj, const anj_dm_obj_t *obj) {
        (void) anj;
        (void) obj;
        // Perform validation of the object
        return 0;
    }

    static void transaction_end(anj_t *anj,
                                const anj_dm_obj_t *obj,
                                anj_dm_transaction_result_t result) {
        (void) anj;
        (void) obj;

        if (result == ANJ_DM_TRANSACTION_SUCCESS) {
            return;
        }
        // restore cached data
        temp_obj_ctx_t *ctx = get_ctx();
        memcpy(ctx->insts, ctx->insts_cached, sizeof(ctx->insts));
        memcpy(ctx->temp_insts, ctx->temp_insts_cached, sizeof(ctx->temp_insts));
    }
