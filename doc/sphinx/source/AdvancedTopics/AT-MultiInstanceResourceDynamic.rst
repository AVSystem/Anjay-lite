..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Multi-Instance Resource dynamic
===============================

Overview
--------

This guide explains how to implement a **dynamic multiple-instance resource** in Anjay Lite,
based on an extended version of the :doc:`Multiple-instance resource example <AT-MultiInstanceResource>`.
Unlike the static variant, this version supports creating and deleting resource instances at runtime,
and dynamically reports which instances currently exist. The set of active Resource Instance IDs (RIIDs)
can change during execution. This is useful for representing user-defined configurations, history logs,
or other lists that are not known at compile time.

.. note::

    Complete code of this example can be found in
    `examples/tutorial/AT-MultiInstanceResourceDynamic` subdirectory of main Anjay Lite
    project repository.

.. note::
    To generate code stubs for multiple-instance resources with dynamic instance support, use the ``anjay_codegen.py`` script with the ``-dri`` and ``-nri`` flags:

    .. code-block:: bash

        ./tools/anjay_codegen.py -i binary_app_data_container.xml -o binary_app_data_container.c -dri -nri <rid1> <max1> -nri <rid2> <max2> <...>
 
    For details, see the :ref:`Dynamic multiple resources instances generation<multi-resource-instances-dynamic-generator>` section.

Implement dynamic Resource
--------------------------

Implementation of a dynamic multiple-instance resource shares similarities with the static variant, with the following differences:

- Define `DATA_RES_MAX_INST_COUNT` to limit the number of allowed instances.

- Do not declare the `res_insts` array as `const`, as it is modified at runtime.

- Use `res_insts_cached` for transactional operations to temporarily store the RIIDs during changes.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResourceDynamic/src/binary_app_data_container.c
    :emphasize-lines: 4

    #define DATA_RES_MAX_INST_COUNT 3

    static anj_riid_t res_insts[DATA_RES_MAX_INST_COUNT];
    static anj_riid_t res_insts_cached[DATA_RES_MAX_INST_COUNT];

Handle Instance Creation and Deletion
-------------------------------------

**Instance Creation**

To allow the server to create and delete resource instances dynamically, you need to implement
`res_instance_create()` and `res_instance_delete()` handlers. These handlers will manage the `res_insts`
array, which keeps track of active Resource Instance IDs (RIIDs). 

During Create operation, new instance is added to the `res_insts` array, and the corresponding `bin_data_insts`
is initialized. To keep the order of instances, you must insert the new instance at the correct position based on its RIID.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResourceDynamic/src/binary_app_data_container.c

    static int res_inst_create(anj_t *anj,
                            const anj_dm_obj_t *obj,
                            anj_iid_t iid,
                            anj_rid_t rid,
                            anj_riid_t riid) {
        (void) anj;
        (void) obj;
        (void) iid;
        (void) rid;
        // there is space for new instance
        assert(res_insts[DATA_RES_MAX_INST_COUNT - 1] == ANJ_ID_INVALID);

        for (uint16_t i = 0; i < DATA_RES_MAX_INST_COUNT; i++) {
            if (res_insts[i] == ANJ_ID_INVALID || res_insts[i] >= riid) {
                for (uint16_t j = DATA_RES_MAX_INST_COUNT - 1; j > i; --j) {
                    res_insts[j] = res_insts[j - 1];
                    bin_data_insts[j] = bin_data_insts[j - 1];
                }
                res_insts[i] = riid;
                bin_data_insts[i].data_size = 0;
                break;
            }
        }
        return 0;
    }

**Instance Deletion**

The `res_instance_delete()` handler removes the specified RIID from the `res_insts` array by shifting subsequent entries left to fill the gap.
The last slot is then marked as empty by setting it to `ANJ_ID_INVALID`.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResourceDynamic/src/binary_app_data_container.c

    static int res_inst_delete(anj_t *anj,
                            const anj_dm_obj_t *obj,
                            anj_iid_t iid,
                            anj_rid_t rid,
                            anj_riid_t riid) {
        (void) anj;
        (void) obj;
        (void) iid;
        (void) rid;

        for (uint16_t i = 0; i < DATA_RES_MAX_INST_COUNT - 1; i++) {
            if (res_insts[i] == riid) {
                for (uint16_t j = i; j < DATA_RES_MAX_INST_COUNT - 1; j++) {
                    res_insts[j] = res_insts[j + 1];
                    bin_data_insts[j] = bin_data_insts[j + 1];
                }
                break;
            }
        }
        res_insts[DATA_RES_MAX_INST_COUNT - 1] = ANJ_ID_INVALID;
        return 0;
    }

.. note::

    `res_instance_create()` and `res_instance_delete()` can also be called during write operations.
    Write Update operation can create new instances. If Write Replace target a resource, then all
    existing instances are deleted, and new instances are created with the provided RIIDs.

.. note::

   Always ensure that the `res_insts` array is sorted in ascending order of RIIDs, and instances that are not present
   are marked as `ANJ_ID_INVALID`.

.. _reset-instance-context:

Reset instance context
----------------------

In order to support Write Replace operation that targets the object instance, you need to implement
`inst_reset()` handler. In this callback, you should clear all resource instances by setting each entry in the
`res_insts` array to `ANJ_ID_INVALID`.

.. warning::
    It is an :ref:`known issue <default-value-of-multi-instance-resource>` that
    multiple instance resources can't have a default value after a write-replace
    operation.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResourceDynamic/src/binary_app_data_container.c

    static int inst_reset(anj_t *anj, const anj_dm_obj_t *obj, anj_iid_t iid) {
        (void) anj;
        (void) obj;
        (void) iid;

        // Clear all resource instances
        for (uint8_t i = 0; i < DATA_RES_MAX_INST_COUNT; i++) {
            res_insts[i] = ANJ_ID_INVALID;
        }

        return 0;
    }

Register Object
---------------

Object definition is similar to the static version, but with additional handlers.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResourceDynamic/src/binary_app_data_container.c
    :emphasize-lines: 5-6

    static const anj_dm_handlers_t HANDLERS = {
        .res_read = res_read,
        .res_write = res_write,
        .inst_reset = inst_reset,
        .res_inst_create = res_inst_create,
        .res_inst_delete = res_inst_delete,
        .transaction_begin = transaction_begin,
        .transaction_end = transaction_end,
    };

    static const anj_dm_obj_t OBJ = {
        .oid = 19,
        .insts = &INST,
        .handlers = &HANDLERS,
        .max_inst_count = 1
    };

`init_binary_app_data_container` in this version calls `init_inst_ctx()` to initialize the resource instances with default values and
returns a pointer to the object definition.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-MultiInstanceResourceDynamic/src/binary_app_data_container.c
    :emphasize-lines: 2

    const anj_dm_obj_t *init_binary_app_data_container(void) {
        for (uint16_t i = 1; i < DATA_RES_MAX_INST_COUNT; i++) {
            bin_data_insts[i].data_size = 0;
            res_insts[i] = ANJ_ID_INVALID;
        }
        bin_data_insts[0].data[0] = (uint8_t) 'X';
        bin_data_insts[0].data_size = 1;
        res_insts[0] = 0;

        return &OBJ;
    }
