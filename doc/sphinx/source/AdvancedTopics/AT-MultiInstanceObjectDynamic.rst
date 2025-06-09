..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Multi-Instance Object Dynamic
=============================

This section describes implementation of a **multi-instance Object** defined in
`OMA LwM2M Registry <https://www.openmobilealliance.org/specifications/registries/objects>`_
**with dynamic set of Instances**.

As an example, we are going to implement
`Temperature Object with ID 3303 in version 1.1 <https://raw.githubusercontent.com/OpenMobileAlliance/lwm2m-registry/prod/version_history/3303-1_1.xml>`_ by expanding the code developed in `Multi-instance Object Implementation <../AT-MultiInstanceObject.html>`_.
Only the differences that need to be applied are described in this article.

Implementing the Object
-----------------------

.. note::
   Code related to this tutorial can be found under `examples/tutorial/AT-MultiInstanceObjectDynamic`
   in the Anjay Lite source directory and is based on `examples/tutorial/AT-MultiInstanceObject`
   example.

Besides the fact that the set of Object Instances is dynamic, the maximum number
`TEMP_OBJ_NUMBER_OF_INSTANCES` must still be set.

Object and Object Instances State
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Since the number of instances is now dynamic, and we want to handle transactional
operations properly, let's change the caching method from Resource level to the 
whole Instance level.

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


Create and Delete Handlers
^^^^^^^^^^^^^^^^^^^^^^^^^^

To support creating and deleting Object Instances by the Server, we need to
implement appropriate handlers: ``inst_create`` and ``inst_delete``.

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
        temp_obj_ctx_t *ctx = ANJ_CONTAINER_OF(obj, temp_obj_ctx_t, obj);

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
        temp_obj_ctx_t *ctx = ANJ_CONTAINER_OF(obj, temp_obj_ctx_t, obj);
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


For **Create** operation that does not indicate the new **iid**, Anjay Lite
assigns a new ``iid`` and passes it down to the ``inst_create`` handler from
Object definition.


Object definition and Initialization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First, let's create a static structure of `temperature_obj` with the basic information:

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

And then we can add an initialization function that fills the `insts` and `temp_insts`
arrays contents:

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

This initialization must be then called in `main` function before installing
the Object in Anjay Lite Data Model.

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


Supporting transactional Operations
-----------------------------------

In this case, to achieve proper transactions support, we need to cache not only
the writeable resources, but rather the whole Object context. This can be done
by creating copies of both `insts` and `temp_insts` arrays:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceObjectDynamic/src/temperature_obj.c
    :emphasize-lines: 5-6, 23-25

    static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
        (void) anj;

        temp_obj_ctx_t *ctx = ANJ_CONTAINER_OF(obj, temp_obj_ctx_t, obj);
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

    static void transaction_end(anj_t *anj, const anj_dm_obj_t *obj, int result) {
        (void) anj;

        if (result) {
            // restore cached data
            temp_obj_ctx_t *ctx = ANJ_CONTAINER_OF(obj, temp_obj_ctx_t, obj);
            memcpy(ctx->insts, ctx->insts_cached, sizeof(ctx->insts));
            memcpy(ctx->temp_insts, ctx->temp_insts_cached,
                sizeof(ctx->temp_insts));
        }
    }

