/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Persistence API for storing and restoring Anjay Lite state.
 *
 * Provides a generic context/stream abstraction with read/write callbacks.
 * Used by higher-level modules to serialize internal state, security
 * information, or application data to a platform-specific medium.
 *
 * Supports storing/restoring primitive types, strings, fixed-size buffers,
 * and validating blobs with “magic” tags.
 *
 * @note The binary format is not portable across architectures or compiler
 *       ABIs. Persisted data is only guaranteed to be valid on the same
 *       platform/build.
 */

#ifndef ANJ_PERSISTENCE_H
#    define ANJ_PERSISTENCE_H

#    include <stdbool.h>
#    include <stdint.h>
#    include <string.h>

#    ifdef __cplusplus
extern "C" {
#    endif

#    ifdef ANJ_WITH_PERSISTENCE

/**
 * Maximum number of bytes accepted by @ref anj_persistence_magic() as the
 * “magic” tag used to identify a blob format/version.
 */
#        define ANJ_PERSISTENCE_MAGIC_MAX_SIZE 16

/**
 * Read callback type. Implementations must read exactly @p size bytes into @p
 * buf.
 *
 * @param      ctx  User context provided at context creation.
 * @param[out] buf  Destination buffer.
 * @param      size Number of bytes to read.
 *
 * @return 0 on success, negative value on error.
 */
typedef int anj_persistence_read_cb_t(void *ctx, void *buf, size_t size);

/**
 * Write callback type. Implementations must write exactly @p size bytes from @p
 * buf.
 *
 * @param ctx  User context provided at context creation.
 * @param buf  Source buffer.
 * @param size Number of bytes to write.
 *
 * @return 0 on success, negative value on error.
 */
typedef int anj_persistence_write_cb_t(void *ctx, const void *buf, size_t size);

/**
 * Persistence direction.
 */
typedef enum {
    /**
     * Store data to the underlying medium using the write callback.
     */
    ANJ_PERSISTENCE_STORE = 0,

    /**
     * Restore data from the underlying medium using the read callback.
     */
    ANJ_PERSISTENCE_RESTORE
} anj_persistence_direction_t;

/**
 * Persistence context.
 *
 * @anj_internal_fields_do_not_use
 */
typedef struct {
    anj_persistence_direction_t direction;
    anj_persistence_read_cb_t *read;
    anj_persistence_write_cb_t *write;
    void *ctx;
} anj_persistence_context_t;

/**
 * Creates a persistence context for storing data.
 *
 * @warning The persistence format is not guaranteed to be portable across
 *          architectures, compilers, or compiler versions. Moving persistence
 *          files between different systems may result in undefined behavior
 *          (e.g., due to endianness or ABI differences).
 *
 * @param write_cb Callback to use for writing data.
 * @param ctx      User context passed to the write callback.
 *
 * @return Persistence context.
 */
anj_persistence_context_t
anj_persistence_store_context_create(anj_persistence_write_cb_t *write_cb,
                                     void *ctx);

/**
 * Creates a persistence context for restoring data.
 *
 * @param read_cb Callback to use for reading data.
 * @param ctx     User context passed to the read callback.
 *
 * @return Persistence context.
 */
anj_persistence_context_t
anj_persistence_restore_context_create(anj_persistence_read_cb_t *read_cb,
                                       void *ctx);

/**
 * Returns the direction of the persistence context.
 */
static inline anj_persistence_direction_t
anj_persistence_direction(const anj_persistence_context_t *ctx) {
    return ctx->direction;
}

/**
 * Stores or restores a fixed-size byte buffer.
 *
 * - In STORE mode: writes exactly @p buffer_size bytes from @p inout_buffer.
 * - In RESTORE mode: reads exactly @p buffer_size bytes into @p inout_buffer.
 *
 * @param        ctx          Persistence context.
 * @param[inout] inout_buffer Buffer to write or fill.
 * @param        buffer_size  Number of bytes to transfer.
 *
 * @return 0 on success, negative value on error.
 */
int anj_persistence_bytes(const anj_persistence_context_t *ctx,
                          void *inout_buffer,
                          size_t buffer_size);

/**
 * Stores or restores a NUL-terminated string.
 *
 * - In STORE mode: writes the string pointed to by @p inout_str excluding
 *   the terminating NUL byte, and its length as a 64-bit unsigned integer.
 * - In RESTORE mode: reads a NUL-terminated string into @p inout_str, writing
 *   at most @p out_max_size bytes (including the terminator). The result is
 *   guaranteed to be NUL-terminated if the call succeeds.
 *
 * @param        ctx          Persistence context.
 * @param[inout] inout_str    String to write or destination buffer.
 * @param        out_max_size Size of @p inout_str (RESTORE). Ignored in
 *                              STORE.
 *
 * @return 0 on success, negative value on error.
 */
int anj_persistence_string(const anj_persistence_context_t *ctx,
                           char *inout_str,
                           size_t out_max_size);

/**
 * Writes or verifies a “magic” tag.
 *
 * - In STORE mode: writes @p magic of size @p magic_size.
 * - In RESTORE mode: reads @p magic_size bytes and verifies they are identical
 *   to @p magic.
 *
 * Typical use is to guard persisted blobs with a short prefix describing the
 * expected format/version.
 *
 * @param ctx        Persistence context.
 * @param magic      Pointer to expected/actual magic bytes.
 * @param magic_size Number of bytes (<= @ref ANJ_PERSISTENCE_MAGIC_MAX_SIZE).
 *
 * @return 0 on success, negative value on error or mismatch (RESTORE).
 */
int anj_persistence_magic(const anj_persistence_context_t *ctx,
                          const void *magic,
                          size_t magic_size);

/** Stores/restores a boolean value. */
static inline int anj_persistence_bool(const anj_persistence_context_t *ctx,
                                       bool *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

/** Stores/restores an unsigned 8-bit value. */
static inline int anj_persistence_u8(const anj_persistence_context_t *ctx,
                                     uint8_t *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

/** Stores/restores an unsigned 16-bit value. */
static inline int anj_persistence_u16(const anj_persistence_context_t *ctx,
                                      uint16_t *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

/** Stores/restores an unsigned 32-bit value. */
static inline int anj_persistence_u32(const anj_persistence_context_t *ctx,
                                      uint32_t *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

/** Stores/restores an unsigned 64-bit value. */
static inline int anj_persistence_u64(const anj_persistence_context_t *ctx,
                                      uint64_t *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

/** Stores/restores a signed 8-bit value. */
static inline int anj_persistence_i8(const anj_persistence_context_t *ctx,
                                     int8_t *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

/** Stores/restores a signed 16-bit value. */
static inline int anj_persistence_i16(const anj_persistence_context_t *ctx,
                                      int16_t *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

/** Stores/restores a signed 32-bit value. */
static inline int anj_persistence_i32(const anj_persistence_context_t *ctx,
                                      int32_t *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

/** Stores/restores a signed 64-bit value. */
static inline int anj_persistence_i64(const anj_persistence_context_t *ctx,
                                      int64_t *inout_value) {
    return anj_persistence_bytes(ctx, inout_value, sizeof(*inout_value));
}

#    endif // ANJ_WITH_PERSISTENCE

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_PERSISTENCE_H
