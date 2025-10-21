/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <anj/defs.h>
#include <anj/dm/core.h>
#include <anj/dm/defs.h>
#include <anj/utils.h>

#include "temperature_obj.h"

#define TEMPERATURE_RESOURCES_COUNT 8
#define TEMP_OBJ_NUMBER_OF_INSTANCES 3

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

#define TEMP_OBJ_SENSOR_UNITS_VAL "C"
#define TEMP_OBJ_APPL_TYPE_MAX_SIZE 10

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

static inline temp_obj_ctx_t *get_ctx(void);

static temp_obj_inst_t *get_temp_inst(anj_iid_t iid) {
    temp_obj_ctx_t *ctx = get_ctx();
    for (uint16_t idx = 0; idx < TEMP_OBJ_NUMBER_OF_INSTANCES; idx++) {
        if (ctx->temp_insts[idx].iid == iid) {
            return &ctx->temp_insts[idx];
        }
    }

    return NULL;
}

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

void update_temperature_obj_value(void) {
    temp_obj_ctx_t *ctx = get_ctx();

    for (int i = 0; i < TEMP_OBJ_NUMBER_OF_INSTANCES; i++) {
        temp_obj_inst_t *inst = &ctx->temp_insts[i];
        if (inst->iid != ANJ_ID_INVALID) {
            inst->sensor_value =
                    next_temperature_with_limit(inst->sensor_value, 0.2);
            if (inst->sensor_value < inst->min_sensor_value) {
                inst->min_sensor_value = inst->sensor_value;
            }
            if (inst->sensor_value > inst->max_sensor_value) {
                inst->max_sensor_value = inst->sensor_value;
            }
        }
    }
}

static int res_read(anj_t *anj,
                    const anj_dm_obj_t *obj,
                    anj_iid_t iid,
                    anj_rid_t rid,
                    anj_riid_t riid,
                    anj_res_value_t *out_value) {
    (void) anj;
    (void) obj;
    (void) riid;

    temp_obj_inst_t *temp_inst = get_temp_inst(iid);
    assert(temp_inst);

    switch (rid) {
    case RID_SENSOR_VALUE:
        out_value->double_value = temp_inst->sensor_value;
        break;
    case RID_MIN_MEASURED_VALUE:
        out_value->double_value = temp_inst->min_sensor_value;
        break;
    case RID_MAX_MEASURED_VALUE:
        out_value->double_value = temp_inst->max_sensor_value;
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
        out_value->bytes_or_string.data = temp_inst->application_type;
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }
    return 0;
}

static int res_write(anj_t *anj,
                     const anj_dm_obj_t *obj,
                     anj_iid_t iid,
                     anj_rid_t rid,
                     anj_riid_t riid,
                     const anj_res_value_t *value) {
    (void) anj;
    (void) obj;
    (void) riid;

    temp_obj_inst_t *temp_inst = get_temp_inst(iid);
    assert(temp_inst);

    switch (rid) {
    case RID_APPLICATION_TYPE:
        return anj_dm_write_string_chunked(value, temp_inst->application_type,
                                           TEMP_OBJ_APPL_TYPE_MAX_SIZE, NULL);
        break;
    default:
        return ANJ_DM_ERR_NOT_FOUND;
    }
    return 0;
}

static int res_execute(anj_t *anj,
                       const anj_dm_obj_t *obj,
                       anj_iid_t iid,
                       anj_rid_t rid,
                       const char *execute_arg,
                       size_t execute_arg_len) {
    (void) anj;
    (void) obj;
    (void) execute_arg;
    (void) execute_arg_len;

    temp_obj_inst_t *temp_inst = get_temp_inst(iid);
    assert(temp_inst);

    switch (rid) {
    case RID_RESET_MIN_MAX_MEASURED_VALUES: {
        temp_inst->min_sensor_value = temp_inst->sensor_value;
        temp_inst->max_sensor_value = temp_inst->sensor_value;
        return 0;
    }
    default:
        break;
    }

    return ANJ_DM_ERR_NOT_FOUND;
}

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

static temp_obj_ctx_t temperature_obj = {
    .obj = {
        .oid = 3303,
        .version = "1.1",
        .handlers = &TEMP_OBJ_HANDLERS,
        .max_inst_count = TEMP_OBJ_NUMBER_OF_INSTANCES
    }
};

const anj_dm_obj_t *get_temperature_obj(void) {
    return &temperature_obj.obj;
}

static inline temp_obj_ctx_t *get_ctx(void) {
    return &temperature_obj;
}

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
