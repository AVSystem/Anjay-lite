/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 53

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <anj/log.h>
#include <anj/persistence.h>
#include <anj/utils.h>

#ifdef ANJ_WITH_PERSISTENCE

#    define persistence_log(...) anj_log(persistence, __VA_ARGS__)

anj_persistence_context_t
anj_persistence_store_context_create(anj_persistence_write_cb_t *write_cb,
                                     void *ctx) {
    assert(write_cb);
    anj_persistence_context_t persistence_ctx = {
        .direction = ANJ_PERSISTENCE_STORE,
        .write = write_cb,
        .ctx = ctx
    };
    return persistence_ctx;
}

anj_persistence_context_t
anj_persistence_restore_context_create(anj_persistence_read_cb_t *read_cb,
                                       void *ctx) {
    assert(read_cb);
    anj_persistence_context_t persistence_ctx = {
        .direction = ANJ_PERSISTENCE_RESTORE,
        .read = read_cb,
        .ctx = ctx
    };
    return persistence_ctx;
}

int anj_persistence_bytes(const anj_persistence_context_t *ctx,
                          void *inout_buffer,
                          size_t buffer_size) {
    assert(ctx && inout_buffer);
    if (ctx->direction == ANJ_PERSISTENCE_STORE) {
        assert(ctx->write);
        return ctx->write(ctx->ctx, inout_buffer, buffer_size);
    } else {
        assert(ctx->read);
        return ctx->read(ctx->ctx, inout_buffer, buffer_size);
    }
}

int anj_persistence_string(const anj_persistence_context_t *ctx,
                           char *inout_str,
                           size_t out_max_size) {
    assert(ctx && inout_str);
    if (ctx->direction == ANJ_PERSISTENCE_STORE) {
        size_t str_len = strlen(inout_str);
        uint64_t size64 = (uint64_t) str_len;
        int result = anj_persistence_u64(ctx, &size64);
        if (result) {
            return result;
        }
        return anj_persistence_bytes(ctx, inout_str, str_len);
    } else {
        uint64_t size64;
        int result = anj_persistence_u64(ctx, &size64);
        if (result) {
            return result;
        }
        if (size64 >= out_max_size) {
            persistence_log(L_ERROR, "String too long");
            return -1;
        }
        result = anj_persistence_bytes(ctx, inout_str, (size_t) size64);
        if (result) {
            return result;
        }
        inout_str[size64] = '\0';
        return 0;
    }
}

int anj_persistence_magic(const anj_persistence_context_t *ctx,
                          const void *magic,
                          size_t magic_size) {
    assert(ctx && magic && magic_size > 0);
    if (magic_size > ANJ_PERSISTENCE_MAGIC_MAX_SIZE) {
        return -1;
    }
    if (ctx->direction == ANJ_PERSISTENCE_STORE) {
        uint8_t buffer[ANJ_PERSISTENCE_MAGIC_MAX_SIZE];
        memcpy(buffer, magic, magic_size);
        return anj_persistence_bytes(ctx, buffer, magic_size);
    } else {
        uint8_t buffer[ANJ_PERSISTENCE_MAGIC_MAX_SIZE];
        int result = anj_persistence_bytes(ctx, buffer, magic_size);
        if (result) {
            return result;
        }
        if (memcmp(buffer, magic, magic_size) != 0) {
            persistence_log(L_ERROR, "Persistence magic mismatch");
            return -1;
        }
        return 0;
    }
}

#endif // ANJ_WITH_PERSISTENCE
