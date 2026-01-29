/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 3

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "common.h"

int _anj_bytes_append(_anj_bytes_appender_t *appender,
                      const void *data,
                      size_t size_bytes) {
    if (!appender->bytes_left || appender->bytes_left < size_bytes) {
        return -1;
    }

    if (size_bytes > 0) {
        assert(data);
        memcpy(appender->write_ptr, data, size_bytes);
    }

    appender->write_ptr += size_bytes;
    appender->bytes_left -= size_bytes;
    return 0;
}

int _anj_bytes_extract(anj_bytes_dispenser_t *dispenser,
                       void *out,
                       size_t size_bytes) {
    if (!dispenser->bytes_left || dispenser->bytes_left < size_bytes) {
        return -1;
    }

    if (size_bytes) {
        assert(out);
        memcpy(out, dispenser->read_ptr, size_bytes);
    }

    dispenser->read_ptr += size_bytes;
    dispenser->bytes_left -= size_bytes;
    return 0;
}
