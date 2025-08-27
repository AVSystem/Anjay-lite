..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Multi-Instance Resource
=======================

Overview
---------

This guide explains how to implement a **multiple-instance resource** in Anjay Lite,
based on a *Binary App Data Container* (Object ID: 19)
defined in the `LwM2M Registry <https://raw.githubusercontent.com/OpenMobileAlliance/lwm2m-registry/prod/version_history/19-1_0.xml>`_.

In this tutorial, we implement only the mandatory Resource `/0`, which can hold
multiple instances of binary data, each identified by a Resource Instance ID (RIID).
This allows for efficient storage and retrieval of multiple values under a single Resource ID (RID).

.. note::

    Complete code of this example can be found in
    `examples/tutorial/AT-MultiInstanceResource` subdirectory of main Anjay Lite
    project repository.

.. note::
    To generate code stubs for multiple-instance resources, use the ``anjay_codegen.py`` script with the ``-nri`` option:

    .. code-block:: bash

        ./tools/anjay_codegen.py -i binary_app_data_container.xml -o binary_app_data_container.c -nri <rid1> <n1> -nri <rid2> <n2> <...>

 
    For details, see the :ref:`Multiple resource instances generation<multi-resource-instances-generator>` section. 

Implement Static Resource
-------------------------

**Define the resource structure**

Define `binary_app_data_container_inst_t` to hold the binary data and its size.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResource/src/binary_app_data_container.c

    #define DATA_RES_INST_MAX_SIZE 64

    typedef struct {
        uint8_t data[DATA_RES_INST_MAX_SIZE];
        size_t data_size;
    } binary_app_data_container_inst_t;

**Create Instance and ID Arrays**

Create an array of `anj_riid_t` to represent the Resource Instance IDs, and an array of `binary_app_data_container_inst_t` to hold the actual data.
Each index in `anj_riid_t` corresponds to a `binary_app_data_container_inst_t` instance.
`bin_data_insts_cached` is used to handle transactional operations, allowing for temporary storage of data during write operations.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResource/src/binary_app_data_container.c

    static const anj_riid_t res_insts[DATA_RES_INST_COUNT] = { 0, 1, 2 };
    static binary_app_data_container_inst_t bin_data_insts[DATA_RES_INST_COUNT];
    static binary_app_data_container_inst_t
            bin_data_insts_cached[DATA_RES_INST_COUNT];

**Define the resource**

Define the resource in `anj_dm_res_t`, specifying the Resource ID, type, operation mode, and instance definitions.
At the end, define a single object instance that includes the resource.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResource/src/binary_app_data_container.c

    static const anj_dm_res_t RES_DATA = {
        .rid = RID_DATA,
        .type = ANJ_DATA_TYPE_BYTES,
        .operation = ANJ_DM_RES_RWM,
        .insts = res_insts,
        .max_inst_count = DATA_RES_INST_COUNT,
    };

    static const anj_dm_obj_inst_t INST = {
        .iid = 0,
        .res_count = 1,
        .resources = &RES_DATA
    };


.. note::

    This tutorial uses a static resource definition. All instance definitions are declared as `const`.
    Only the data content (`bin_data_insts`) is mutable at runtime.

Read and Write Resource Instances
---------------------------------

**Select an Instance**

Each RIID identifies one of the binary data buffers. At runtime, the corresponding instance is selected by the
`get_inst_ctx()` function.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResource/src/binary_app_data_container.c

    static binary_app_data_container_inst_t *get_inst_ctx(anj_riid_t riid) {
        for (uint16_t i = 0; i < DATA_RES_INST_COUNT; i++) {
            if (res_insts[i] == riid) {
                return &bin_data_insts[i];
            }
        }
        return NULL;
    }

**Read Data**

The `res_read` function handles read operations. It retrieves a pointer to the appropriate instance using the `get_inst_ctx` function.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResource/src/binary_app_data_container.c
    :emphasize-lines: 11,16-17

    static int res_read(anj_t *anj,
                        const anj_dm_obj_t *obj,
                        anj_iid_t iid,
                        anj_rid_t rid,
                        anj_riid_t riid,
                        anj_res_value_t *out_value) {
        (void) anj;
        (void) obj;
        (void) iid;

        binary_app_data_container_inst_t *inst_ctx = get_inst_ctx(riid);
        assert(inst_ctx);

        switch (rid) {
        case RID_DATA:
            out_value->bytes_or_string.data = inst_ctx->data;
            out_value->bytes_or_string.chunk_length = inst_ctx->data_size;
            break;
        default:
            return ANJ_DM_ERR_NOT_FOUND;
        }
        return 0;
    }

**Write Data**

The `res_write` function handles write operations. It uses `anj_dm_write_bytes_chunked` to write binary data in chunks.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResource/src/binary_app_data_container.c
    :emphasize-lines: 11,16-18

    static int res_write(anj_t *anj,
                        const anj_dm_obj_t *obj,
                        anj_iid_t iid,
                        anj_rid_t rid,
                        anj_riid_t riid,
                        const anj_res_value_t *value) {
        (void) anj;
        (void) obj;
        (void) iid;

        binary_app_data_container_inst_t *inst_ctx = get_inst_ctx(riid);
        assert(inst_ctx);

        switch (rid) {
        case RID_DATA:
            return anj_dm_write_bytes_chunked(value, inst_ctx->data,
                                            DATA_RES_INST_MAX_SIZE,
                                            &inst_ctx->data_size, NULL);
            break;
        default:
            return ANJ_DM_ERR_NOT_FOUND;
        }
        return 0;
    }

**Handle Transactions**

To ensure atomic write operations, implement transaction handling using the `transaction_begin` and `transaction_end` functions.
These functions are called at the beginning and end of a write operation, respectively.
The `transaction_begin` function saves the current state of `bin_data_insts` to `bin_data_insts_cached`,
enabling rollback if a write fails. If the write operation is unsuccessful, `transaction_end` restores the cached state.


.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResource/src/binary_app_data_container.c
    :emphasize-lines: 5,15

    static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
        (void) anj;
        (void) obj;

        memcpy(bin_data_insts_cached, bin_data_insts, sizeof(bin_data_insts));
        return 0;
    }

    static void transaction_end(anj_t *anj, const anj_dm_obj_t *obj, int result) {
        (void) anj;
        (void) obj;
        if (!result) {
            return;
        }
        memcpy(bin_data_insts, bin_data_insts_cached, sizeof(bin_data_insts));
    }

Register the Object
-------------------

Finally, define the object and its handlers. The `init_binary_app_data_container` function initializes
the resource instances with default values and returns a pointer to the object definition.
Use this pointer when calling the `anj_dm_add_obj` function to register the object.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResource/src/binary_app_data_container.c
    :emphasize-lines: 15-21

    static const anj_dm_handlers_t HANDLERS = {
        .res_read = res_read,
        .res_write = res_write,
        .transaction_begin = transaction_begin,
        .transaction_end = transaction_end,
    };

    static const anj_dm_obj_t OBJ = {
        .oid = 19,
        .insts = &INST,
        .handlers = &HANDLERS,
        .max_inst_count = 1
    };

    const anj_dm_obj_t *init_binary_app_data_container(void) {
        for (uint16_t i = 0; i < DATA_RES_INST_COUNT; i++) {
            bin_data_insts[i].data[0] = (uint8_t) i;
            bin_data_insts[i].data_size = 1;
        }
        return &OBJ;
    }

Conclusion
----------

This example demonstrates how to implement a static multiple-instance resource with minimal runtime overhead.
All structural elements are defined as `const`, and instance selection is handled using the `riid` value.

For a dynamic approach, where the number of resource instances can vary at runtime,
see :doc:`AT-MultiInstanceResourceDynamic`.
