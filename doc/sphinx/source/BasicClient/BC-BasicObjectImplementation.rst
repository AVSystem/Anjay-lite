..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Basic Object Implementation
===========================

Overview
^^^^^^^^
This guide explains how to implement a standard Object as defined in the
`OMA LwM2M Registry <https://www.openmobilealliance.org/specifications/registries/objects>`_.
As an example, we will implement the
`Temperature Object with ID 3303 in version 1.1 <https://raw.githubusercontent.com/OpenMobileAlliance/lwm2m-registry/prod/version_history/3303-1_1.xml>`_.

.. note::
    For a complete example of a Temperature Object implementation integrated with a real platform,
    see the integration examples provided in the :ref:`integrations` directory of the Anjay Lite repository.
    These demonstrate how to connect the Temperature Object to actual hardware or platform-specific APIs.

.. note::
    To quickly generate object stubs and boilerplate code, you can use the
    :ref:`anjay_codegen.py <anjay-object-stub-generator>` script.
    This tool automates the creation of resource definitions, handler functions, and the basic object structure,
    so you can focus on implementing your object's specific logic.

    For example, to generate a simple single-instance Temperature Object, use the following command:

    .. code-block:: bash

        ./tools/anjay_codegen.py -i temperature_obj.xml -o temperature_obj.c -ni 1
 
    For details, see :ref:`Single Instance Object<single-instance-generator>`.

Supported Resources
^^^^^^^^^^^^^^^^^^^

The following resources from the Temperature Object are implemented:

+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+
| ID   | Name                       | Operations | Mandatory | Type   | Description                                                                           |
+======+============================+============+===========+========+=======================================================================================+
| 5700 | Sensor Value               | R          | Mandatory | Float  | Last or Current Measured Value from the Sensor.                                       |
+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+
| 5701 | Sensor Units               | R          | Optional  | String | Measurement Units Definition.                                                         |
+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+
| 5601 | Min Measured Value         | R          | Optional  | Float  | The minimum value measured by the sensor since power ON or reset.                     |
+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+
| 5602 | Max Measured Value         | R          | Optional  | Float  | The maximum value measured by the sensor since power ON or reset.                     |
+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+
| 5603 | Min Range Value            | R          | Optional  | Float  | The minimum value that can be measured by the sensor.                                 |
+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+
| 5604 | Max Range Value            | R          | Optional  | Float  | The maximum value that can be measured by the sensor.                                 |
+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+
| 5605 | Reset Min and Max          | E          | Optional  | Exec   | Reset the Min and Max Measured Values to Current Value.                               |
|      | Measured Values            |            |           |        |                                                                                       |
+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+
| 5750 | Application Type           | RW         | Optional  | String | The application type of the sensor or actuator as a string depending on the use case. |
+------+----------------------------+------------+-----------+--------+---------------------------------------------------------------------------------------+

Implement the Object
^^^^^^^^^^^^^^^^^^^^

.. note::
   Code related to this tutorial can be found under `examples/tutorial/BC-BasicObjectImplementation`
   in the Anjay Lite source directory and is based on `examples/tutorial/BC-MandatoryObjects`
   example.

Step 1: Define Resource Metadata
--------------------------------

The first step in implementing the object is to define its resources.

We start by declaring the LwM2M resource IDs as constants and creating matching array
indices for easy access within the resource definition array:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    #define TEMPERATURE_OID 3303
    #define TEMPERATURE_RESOURCES_COUNT 8

    enum {
        RID_MIN_MEASURED_VALUE = 5601,
        RID_MAX_MEASURED_VALUE = 5602,
        RID_MIN_RANGE_VALUE = 5603,
        RID_MAX_RANGE_VALUE = 5604,
        RID_RESET_MIN_MAX_MEASURED_VALUES = 5605,
        RID_SENSOR_VALUE = 5700,
        RID_SENSOR_UNIT = 5701,
        RID_APPLICATION_TYPE = 5750,
    };

    enum {
        RID_MIN_MEASURED_VALUE_IDX = 0,
        RID_MAX_MEASURED_VALUE_IDX,
        RID_MIN_RANGE_VALUE_IDX,
        RID_MAX_RANGE_VALUE_IDX,
        RID_RESET_MIN_MAX_MEASURED_VALUES_IDX,
        RID_SENSOR_VALUE_IDX,
        RID_SENSOR_UNIT_IDX,
        RID_APPLICATION_TYPE_IDX,
        _RID_LAST
    };

    ANJ_STATIC_ASSERT(_RID_LAST == TEMPERATURE_RESOURCES_COUNT,
                      temperature_resource_count_mismatch);

    static const anj_dm_res_t RES[TEMPERATURE_RESOURCES_COUNT] = {
        [RID_MIN_MEASURED_VALUE_IDX] = {
            .rid = RID_MIN_MEASURED_VALUE,
            .type = ANJ_DATA_TYPE_DOUBLE,
            .kind = ANJ_DM_RES_R
        },
        [RID_MAX_MEASURED_VALUE_IDX] = {
            .rid = RID_MAX_MEASURED_VALUE,
            .type = ANJ_DATA_TYPE_DOUBLE,
            .kind = ANJ_DM_RES_R
        },
        [RID_MIN_RANGE_VALUE_IDX] = {
            .rid = RID_MIN_RANGE_VALUE,
            .type = ANJ_DATA_TYPE_DOUBLE,
            .kind = ANJ_DM_RES_R
        },
        [RID_MAX_RANGE_VALUE_IDX] = {
            .rid = RID_MAX_RANGE_VALUE,
            .type = ANJ_DATA_TYPE_DOUBLE,
            .kind = ANJ_DM_RES_R
        },
        [RID_RESET_MIN_MAX_MEASURED_VALUES_IDX] = {
            .rid = RID_RESET_MIN_MAX_MEASURED_VALUES,
            .kind = ANJ_DM_RES_E
        },
        [RID_SENSOR_VALUE_IDX] = {
            .rid = RID_SENSOR_VALUE,
            .type = ANJ_DATA_TYPE_DOUBLE,
            .kind = ANJ_DM_RES_R
        },
        [RID_SENSOR_UNIT_IDX] = {
            .rid = RID_SENSOR_UNIT,
            .type = ANJ_DATA_TYPE_STRING,
            .kind = ANJ_DM_RES_R
        },
        [RID_APPLICATION_TYPE_IDX] = {
            .rid = RID_APPLICATION_TYPE,
            .type = ANJ_DATA_TYPE_STRING,
            .kind = ANJ_DM_RES_RW
        }
    };

The ``RES`` array contains the definitions of the resources implemented in this object.
Each entry defines one resource and includes the following fields:

+---------------+-------------------------------------------------------------------------------------------------------------+
| Field         | Description                                                                                                 |
+===============+=============================================================================================================+
| ``rid``       | Numerical ID of the resource, as per LwM2M object definition (e.g., 5700)                                   |
+---------------+-------------------------------------------------------------------------------------------------------------+
| ``type``      | Data format of the resource. Not set for executable resources.                                              |
+---------------+-------------------------------------------------------------------------------------------------------------+
| ``operation`` | Permitted LwM2M operations for the resource.                                                                |
+---------------+-------------------------------------------------------------------------------------------------------------+

.. important::
    In Anjay Lite, the ``rid`` values in the resource array must appear in **strictly increasing order**.
    Failure to comply will result in initialization errors.

Step 2: Define Object State
---------------------------

The state of our Temperature Object is encapsulated in the ``temp_obj_ctx_t`` structure.
This structure holds the current sensor measurement as well as the minimum and maximum
values observed during runtime. It represents the internal state of the temperature
sensor on the device.

Additionally, the ``application_type`` field contains a user-configurable string
that describes the intended use case of the sensor. A cached version of this string
is stored in ``application_type_cached`` as a backup value for transaction operations.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    typedef struct {
        double sensor_value;
        double min_sensor_value;
        double max_sensor_value;
        char application_type[TEMP_OBJ_APPL_TYPE_MAX_SIZE];
        char application_type_cached[TEMP_OBJ_APPL_TYPE_MAX_SIZE];
    } temp_obj_ctx_t;

We will also add a function declaration that returns a pointer to a statically
allocated structure representing the sensor state that we will need later.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    static inline temp_obj_ctx_t *get_ctx(void);

.. note::
    The object state does not need to be stored in a statically allocated
    structure. For advanced scenarios - such as running multiple LwM2M clients
    within a single application, or reusing the same object logic across
    multiple objects - you may move the statically defined ``anj_dm_obj_t`` into
    a dynamically allocated context structure. In callbacks, Anjay Lite will
    provide a pointer to the object, allowing you to retrieve your context using
    the ``ANJ_CONTAINER_OF()`` macro. For reference, see usages in `src/anj/dm`
    directory.

Step 3: Simulate Sensor Values
------------------------------

Since this is just an example and we are not using a physical temperature sensor,
we simulate sensor readings by generating pseudo-random values. To achieve this,
we define helper functions that return new temperature values based on the previous
reading:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    #define MIN_TEMP_VALUE -10
    #define MAX_TEMP_VALUE 40

    // Simulates a temperature sensor readout based on the previous value
    static double next_temperature(double current_temp, double volatility) {
        double random_change =
                ((double) rand() / RAND_MAX) * 2.0 - 1.0; // Random value in [-1, 1]
        return current_temp + volatility * random_change;
    }

    static double next_temperature_with_limit(double current_temp,
                                              double volatility) {
        double new_temp = next_temperature(current_temp, volatility);
        if (new_temp < MIN_TEMP_VALUE) {
            return MIN_TEMP_VALUE;
        } else if (new_temp > MAX_TEMP_VALUE) {
            return MAX_TEMP_VALUE;
        }
        return new_temp;
    }

The ``next_temperature_with_limit()`` function generates a realistic temperature
value that varies slightly from the previous one while staying within the defined
range of `[-10, 40]` degrees. The ``volatility`` parameter controls the magnitude
of the random variation.

We then define a function that updates the internal state of the Temperature
Object instance.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    void update_temperature_obj_value(void) {
        temp_obj_ctx_t *ctx = get_ctx();

        ctx->sensor_value = next_temperature_with_limit(ctx->sensor_value, 0.2);
        if (ctx->sensor_value < ctx->min_sensor_value) {
            ctx->min_sensor_value = ctx->sensor_value;
        }
        if (ctx->sensor_value > ctx->max_sensor_value) {
            ctx->max_sensor_value = ctx->sensor_value;
        }
    }

This function simulates a new sensor measurement and updates the current,
minimum, and maximum observed values accordingly. We will periodically call
``update_temperature_obj_value()`` to simulate ongoing temperature updates in
our LwM2M object.

.. _concurrent_dm_modifications:

**Concurrent data model modifications**

In Anjay Lite, operations that modify the data model (for example LwM2M
Write or Write-Attributes requests) may span multiple calls to
``anj_core_step()``. This is a consequence of block-wise transfers and other
protocol mechanisms that require the library to keep intermediate state
between subsequent processing steps.

To preserve a consistent state of objects implemented by the user, the
application must avoid modifying the data model while such an operation is
in progress. Failing to do so may lead to subtle race conditions, for
example when both the LwM2M Server and the application attempt to update
the same variable representing a resource at the same time.

To address this, Anjay Lite provides the function
``anj_core_ongoing_operation()``. Before performing any application-driven
modification of objects or resources, the application should check this
function and only proceed when it indicates that no LwM2M operation on the
data model is currently in progress. This ensures that server-triggered and
application-triggered updates do not interfere with each other and that the
data model remains consistent.

There is no need to call this function in user-implemented Resource Handlers,
as the access there is driven by Anjay Lite itself with internal
synchronization.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/main.c

        if (!anj_core_ongoing_operation(&anj)) {
            update_temperature_obj_value();
        }

Step 4: Implement Resource Handlers
-----------------------------------

**Read handler**

The following function handles the LwM2M Read operation for the Temperature Object.
It reads the value of a specific resource identified by its Resource ID (``rid``) and
writes the result to the ``out_value`` output structure:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    static int res_read(anj_t *anj,
                        const anj_dm_obj_t *obj,
                        anj_iid_t iid,
                        anj_rid_t rid,
                        anj_riid_t riid,
                        anj_res_value_t *out_value) {
        (void) anj;
        (void) obj;
        (void) iid;
        (void) riid;

        temp_obj_ctx_t *temp_obj_ctx = get_ctx();

        switch (rid) {
        case RID_SENSOR_VALUE:
            out_value->double_value = temp_obj_ctx->sensor_value;
            break;
        case RID_MIN_MEASURED_VALUE:
            out_value->double_value = temp_obj_ctx->min_sensor_value;
            break;
        case RID_MAX_MEASURED_VALUE:
            out_value->double_value = temp_obj_ctx->max_sensor_value;
            break;
        case RID_MIN_RANGE_VALUE:
            out_value->double_value = MIN_TEMP_VALUE;
            break;
        case RID_MAX_RANGE_VALUE:
            out_value->double_value = MAX_TEMP_VALUE;
            break;
        case RID_SENSOR_UNIT:
            out_value->bytes_or_string.data = TEMP_OBJ_SENSOR_UNITS_VAL;
            break;
        case RID_APPLICATION_TYPE:
            out_value->bytes_or_string.data = temp_obj_ctx->application_type;
            break;
        default:
            return ANJ_DM_ERR_NOT_FOUND;
        }
        return 0;
    }

What this handler does:

    - Checks the ``rid`` to determine which resource is being accessed.
    - Sets ``out_value`` based on the current value from the context.
    - Returns ``ANJ_DM_ERR_NOT_FOUND`` if the resource is not supported or not readable.

.. note::
    This implementation assumes a single-instance object and does not distinguish between
    multiple Object Instances (``iid``) or Resource Instances (``riid``). These values
    are ignored for simplicity.

.. note::
   In the case of ``ANJ_DATA_TYPE_STRING`` or ``ANJ_DATA_TYPE_BYTES``, the read function
   operates on pointers, and the value is not copied. For ``ANJ_DATA_TYPE_BYTES``,
   the function also needs to set the length of the data being returned.
   The memory pointed to **must remain unchanged** for the duration of the data model operation.

**Write handler**

This function implements the LwM2M Write operation handler for the Temperature Object.
It allows modifying the values of writable resources — specifically, the
**Application Type** (Resource ID: 5750).

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    static int res_write(anj_t *anj,
                         const anj_dm_obj_t *obj,
                         anj_iid_t iid,
                         anj_rid_t rid,
                         anj_riid_t riid,
                         const anj_res_value_t *value) {
        (void) anj;
        (void) obj;
        (void) iid;
        (void) riid;

        temp_obj_ctx_t *temp_obj_ctx = get_ctx();

        switch (rid) {
        case RID_APPLICATION_TYPE:
            return anj_dm_write_string_chunked(value,
                                               temp_obj_ctx->application_type,
                                               TEMP_OBJ_APPL_TYPE_MAX_SIZE, NULL);
            break;
        default:
            return ANJ_DM_ERR_NOT_FOUND;
        }
        return 0;
    }

What this handler does:

    - Checks the resource ID (``rid``); only ``RID_APPLICATION_TYPE`` is handled.
    - Uses ``anj_dm_write_string_chunked()`` to write the received string value to ``application_type``.
    - Returns ``ANJ_DM_ERR_NOT_FOUND`` if the resource is not supported or not writable.

.. note::
    Anjay Lite uses chunked writing to support CoAP block-wise transfers.
    The helper function ``anj_dm_write_string_chunked()`` safely assembles the received chunks
    into a single buffer with size checks. For data type other then ``ANJ_DATA_TYPE_BYTES`` and
    ``ANJ_DATA_TYPE_STRING`` the payload always comes as a single write.

.. note::
   For data type ``ANJ_DATA_TYPE_BYTES`` there is an alternative function called
   ``anj_dm_write_bytes_chunked()`` to handle the block-wise transfers.

**Execute handler**

This function implements the LwM2M Execute operation handler for the Temperature Object.
It allows triggering actions on specific executable resources — in this case,
resetting recorded minimum and maximum measured values.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

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
            return 0;
        }
        default:
            break;
        }

        return ANJ_DM_ERR_NOT_FOUND;
    }

What this handler does:

    - If called on resource ``5605`` (Reset Min and Max Measured Values),
      it updates both ``min_sensor_value`` and ``max_sensor_value`` to match the current ``sensor_value``.
    - If called on any other resource, it returns ``ANJ_DM_ERR_NOT_FOUND`` to indicate the operation is not supported.

.. note::
    Although ``execute_arg`` and ``execute_arg_len`` are available for passing arguments to executable resources,
    this implementation does not make use of them, and they are explicitly ignored.

Step 5: Define and Initialize Object
------------------------------------

Now that all handlers and data structures are defined, we can finish an implementation of
the Temperature Object. We start by defining a constant ``anj_dm_handlers_t`` structure that references
our previously implemented handlers:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c
   :emphasize-lines: 2-4

    static const anj_dm_handlers_t TEMP_OBJ_HANDLERS = {
        .res_read = res_read,
        .res_write = res_write,
        .res_execute = res_execute,
        .transaction_begin = transaction_begin,
        .transaction_validate = transaction_validate,
        .transaction_end = transaction_end,
    };

.. note::
    Only handlers for operation we want to support in a given object need to
    be defined in ``anj_dm_handlers_t`` structure.

Next, we define the object instance using ``anj_dm_obj_inst_t``. This structure
includes the instance ID (``iid``), the number of resources, and a pointer to the
resource array we defined earlier:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    static const anj_dm_obj_inst_t INST = {
        .iid = 0,
        .res_count = TEMPERATURE_RESOURCES_COUNT,
        .resources = RES
    };

Now, define the Object itself with the ``anj_dm_obj_t`` structure and a function
that returns a pointer to it:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    static const anj_dm_obj_t OBJ = {
        .oid = TEMPERATURE_OID,
        .version = "1.1",
        .insts = &INST,
        .handlers = &TEMP_OBJ_HANDLERS,
        .max_inst_count = 1
    };

    const anj_dm_obj_t *get_temperature_obj(void) {
        return &OBJ;
    }

The fields in this structure contains metadata describing the object, such as its Object ID, version,
associated instances, and handlers.

Here's a quick summary of the fields:

    - ``oid`` - Object ID.
    - ``version`` - Object version.
    - ``insts`` - Pointer to the object instance definition.
    - ``handlers`` - Reference to the function table defined above.
    - ``max_inst_count`` - Maximum number of object instances.

Finally, we set the initial state of the sensor within the ``temp_obj_ctx_t`` context structure
and define the ``get_ctx()`` function:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    static temp_obj_ctx_t temperature_ctx = {
        .application_type = "Sensor_1",
        .sensor_value = 10.0,
        .min_sensor_value = 10.0,
        .max_sensor_value = 10.0
    };

    static inline temp_obj_ctx_t *get_ctx(void) {
        return &temperature_ctx;
    }

This defines the initial sensor reading and sets the minimum and maximum to the same value.

.. note::
    Because this object is statically allocated, the initial values can be defined
    directly in the initializer. If the object were dynamically created
    its initial state would need to be set manually during the creation process.

.. _registering-objects:

Step 6: Add the Object to Anjay Lite
------------------------------------

The final step is to register the Temperature Object with Anjay Lite and simulate
periodic sensor readings. To do this, we call ``update_temperature_obj_value()``
inside the main loop of the application.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/main.c
   :emphasize-lines: 7-9, 30-33, 38-40

    int main(int argc, char *argv[]) {
        if (argc != 2) {
            log(L_ERROR, "No endpoint name given");
            return -1;
        }

        // Use current time as seed for random generator used by
        // update_temperature_obj_value()
        srand((unsigned int) time(NULL));

        anj_t anj;
        anj_dm_device_obj_t device_obj;
        anj_dm_server_obj_t server_obj;
        anj_dm_security_obj_t security_obj;

        anj_configuration_t config = {
            .endpoint_name = argv[1]
        };
        if (anj_core_init(&anj, &config)) {
            log(L_ERROR, "Failed to initialize Anjay Lite");
            return -1;
        }

        if (install_device_obj(&anj, &device_obj)
                || install_security_obj(&anj, &security_obj)
                || install_server_obj(&anj, &server_obj)) {
            return -1;
        }

        if (anj_dm_add_obj(&anj, get_temperature_obj())) {
            log(L_ERROR, "install_temperature_object error");
            return -1;
        }

        while (true) {
            anj_core_step(&anj);

            if (!anj_core_ongoing_operation(&anj)) {
                update_temperature_obj_value();
            }
            usleep(50 * 1000);
        }
        return 0;
    }

.. _transactional_writes:

Support transactional Writes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Transactional writes protect object integrity when multiple writable resources are modified.
Without transaction handling, a partial update could leave the object in an inconsistent
state if an error occurs during the operation.

To avoid this, Anjay Lite supports transaction mechanisms that:

- Save the current state before applying changes.
- Validate the updated state.
- Revert changes if validation fails or a Composite Write operation fails on any resource.

To support transactional operations, we must implement three handlers:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c

    static int transaction_begin(anj_t *anj, const anj_dm_obj_t *obj) {
        (void) anj;
        (void) obj;
        temp_obj_ctx_t *temp_obj_ctx = get_ctx();
        memcpy(temp_obj_ctx->application_type_cached,
               temp_obj_ctx->application_type, TEMP_OBJ_APPL_TYPE_MAX_SIZE);
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
        temp_obj_ctx_t *temp_obj_ctx = get_ctx();
        // Restore cached data
        memcpy(temp_obj_ctx->application_type,
                temp_obj_ctx->application_type_cached,
                TEMP_OBJ_APPL_TYPE_MAX_SIZE);
    }

These handlers are then registered in the object's handler structure:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-BasicObjectImplementation/src/temperature_obj.c
   :emphasize-lines: 5-7

    static const anj_dm_handlers_t TEMP_OBJ_HANDLERS = {
        .res_read = res_read,
        .res_write = res_write,
        .res_execute = res_execute,
        .transaction_begin = transaction_begin,
        .transaction_validate = transaction_validate,
        .transaction_end = transaction_end,
    };

What these handlers do:

    - ``transaction_begin``: Called before any operation that may modify the object.
      In this case, it makes a backup of the ``application_type`` string.
    - ``transaction_validate``: Called after all write operations have been performed.
      It allows checking whether the new state is valid before committing the transaction.
    - ``transaction_end``: Called at the end of the transaction.
      If the ``result`` indicates failure, the object state is restored using the previously cached data.

.. note::
    Implementing the ``transaction_validate`` handler is optional. Anjay Lite
    will still call ``transaction_end`` even if ``transaction_validate`` is not implemented,
    allowing the user to restore the object state in case of an error.

.. note::
    For more information on transactional writes, see
    :doc:`../AdvancedTopics/AT-Transactions`.

That's it! Your client is now ready to use the new LwM2M Object. Other objects
can be implemented in a similar way.
