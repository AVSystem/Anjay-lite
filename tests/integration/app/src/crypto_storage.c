/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/compat/crypto/storage.h>
#include <anj/crypto.h>
#include <anj/log.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Test-only crypto storage implementation used by the integration test app.
 *
 * The production/default crypto storage writes external credentials to files.
 * For integration tests this is inconvenient, because it leaves
 * crypto_record_*.dat files in the working directory. This implementation keeps
 * credentials provisioned at runtime, e.g. through Bootstrap Write, in memory.
 *
 * There are two supported kinds of external identities:
 * - identities created by this storage, such as "integration_crypto_record_0";
 *   these refer to in-memory records,
 * - identities passed directly from the test configuration; these are treated
 *   as file paths and are read on demand.
 */

#if defined(ANJ_WITH_EXTERNAL_CRYPTO_STORAGE)

#    define log(...) anj_log(test_crypto_storage, __VA_ARGS__)

#    define MAX_RECORDS 12
#    define RECORD_ID_SIZE 32
#    define RECORD_ID_FORMAT "integration_crypto_record_%d"

typedef struct {
    char identity[RECORD_ID_SIZE];
    void *data;
    size_t data_size;
} test_crypto_record_t;

typedef struct {
    test_crypto_record_t records[MAX_RECORDS];
} test_crypto_storage_t;

static test_crypto_record_t *find_record(test_crypto_storage_t *ctx,
                                         const char *identity) {
    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        if (ctx->records[i].identity[0] != '\0'
                && strcmp(ctx->records[i].identity, identity) == 0) {
            return &ctx->records[i];
        }
    }
    return NULL;
}

static test_crypto_record_t *find_free_record(test_crypto_storage_t *ctx) {
    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        if (ctx->records[i].identity[0] == '\0') {
            return &ctx->records[i];
        }
    }
    return NULL;
}

static int read_file_into_buffer(const char *path,
                                 char *out_buffer,
                                 size_t out_buffer_size,
                                 size_t *out_record_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        log(L_ERROR, "Failed to open '%s': %s", path, strerror(errno));
        return -1;
    }

    if (fseek(fp, 0, SEEK_END)) {
        log(L_ERROR, "Failed to seek '%s': %s", path, strerror(errno));
        fclose(fp);
        return -1;
    }

    long file_size = ftell(fp);
    if (file_size < 0) {
        log(L_ERROR, "Failed to get size of '%s': %s", path, strerror(errno));
        fclose(fp);
        return -1;
    }

    if ((size_t) file_size > out_buffer_size) {
        log(L_ERROR, "File '%s' too large: %ld > %lu", path, file_size,
            (unsigned long) out_buffer_size);
        fclose(fp);
        return -1;
    }

    rewind(fp);

    size_t read_bytes = fread(out_buffer, 1, (size_t) file_size, fp);
    if (read_bytes != (size_t) file_size) {
        log(L_ERROR, "Failed to read '%s'", path);
        fclose(fp);
        return -1;
    }

    if (fclose(fp)) {
        log(L_ERROR, "Failed to close '%s': %s", path, strerror(errno));
        return -1;
    }

    *out_record_size = read_bytes;
    return 0;
}

int anj_crypto_storage_init(void **out_crypto_ctx) {
    assert(out_crypto_ctx);

    test_crypto_storage_t *ctx =
            (test_crypto_storage_t *) calloc(1, sizeof(test_crypto_storage_t));
    if (!ctx) {
        log(L_ERROR, "Allocation failed");
        return -1;
    }

    *out_crypto_ctx = ctx;
    return 0;
}

void anj_crypto_storage_deinit(void *crypto_ctx) {
    test_crypto_storage_t *ctx = (test_crypto_storage_t *) crypto_ctx;
    if (!ctx) {
        return;
    }

    for (size_t i = 0; i < MAX_RECORDS; ++i) {
        free(ctx->records[i].data);
    }

    free(ctx);
}

int anj_crypto_storage_create_record(void *crypto_ctx,
                                     anj_crypto_security_info_t *out_info,
                                     const void *data,
                                     size_t data_size) {
    assert(crypto_ctx);
    assert(out_info);

    if (!data || !data_size) {
        log(L_ERROR, "No data provided");
        return -1;
    }

    test_crypto_storage_t *ctx = (test_crypto_storage_t *) crypto_ctx;
    test_crypto_record_t *record = find_free_record(ctx);
    if (!record) {
        log(L_ERROR, "No free crypto storage record");
        return -1;
    }

    void *copy = malloc(data_size);
    if (!copy) {
        log(L_ERROR, "Allocation failed");
        return -1;
    }

    memcpy(copy, data, data_size);

    size_t index = (size_t) (record - ctx->records);
    int written = snprintf(record->identity,
                           sizeof(record->identity),
                           RECORD_ID_FORMAT,
                           (int) index);
    if (written < 0 || (size_t) written >= sizeof(record->identity)) {
        free(copy);
        record->identity[0] = '\0';
        log(L_ERROR, "Generated identity too long");
        return -1;
    }

    record->data = copy;
    record->data_size = data_size;

    out_info->source = ANJ_CRYPTO_DATA_SOURCE_EXTERNAL;
    out_info->info.external.identity = record->identity;

    log(L_INFO, "Created crypto storage record '%s'", record->identity);
    return 0;
}

int anj_crypto_storage_delete_record(void *crypto_ctx,
                                     const anj_crypto_security_info_t *info) {
    assert(crypto_ctx);
    assert(info);
    assert(info->source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);

    const char *identity = info->info.external.identity;
    if (!identity || identity[0] == '\0') {
        log(L_ERROR, "Invalid crypto storage identity");
        return -1;
    }

    test_crypto_storage_t *ctx = (test_crypto_storage_t *) crypto_ctx;
    test_crypto_record_t *record = find_record(ctx, identity);
    if (!record) {
        // Paths passed from test config are not owned by this storage.
        log(L_INFO, "Crypto storage record '%s' not owned by test storage",
            identity);
        return 0;
    }

    free(record->data);
    memset(record, 0, sizeof(*record));

    log(L_INFO, "Deleted crypto storage record '%s'", identity);
    return 0;
}

int anj_crypto_storage_resolve_security_info(
        void *crypto_ctx,
        const anj_crypto_security_info_external_t *info,
        char *out_buffer,
        size_t out_buffer_size,
        size_t *out_record_size) {
    assert(crypto_ctx);
    assert(info);
    assert(out_buffer);
    assert(out_record_size);

    const char *identity = info->identity;
    if (!identity || identity[0] == '\0') {
        log(L_ERROR, "Invalid crypto storage identity");
        return -1;
    }

    test_crypto_storage_t *ctx = (test_crypto_storage_t *) crypto_ctx;
    test_crypto_record_t *record = find_record(ctx, identity);
    if (record) {
        if (record->data_size > out_buffer_size) {
            log(L_ERROR, "Crypto storage record '%s' too large: %lu > %lu",
                identity, (unsigned long) record->data_size,
                (unsigned long) out_buffer_size);
            return -1;
        }
        log(L_INFO, "Resolved crypto storage record '%s' from internal storage",
            identity);
        memcpy(out_buffer, record->data, record->data_size);
        *out_record_size = record->data_size;
        return 0;
    }

    // Identities passed directly from integration test configuration are file
    // paths. They are not owned by the storage; we only read them on demand.
    log(L_INFO, "Resolved crypto storage record '%s' from file", identity);
    return read_file_into_buffer(identity, out_buffer, out_buffer_size,
                                 out_record_size);
}

#    ifdef ANJ_WITH_PERSISTENCE

int anj_crypto_storage_get_persistence_info(
        void *crypto_ctx,
        const anj_crypto_security_info_external_t *info,
        void *out_data,
        size_t *out_data_size) {
    // TODO: Fill this in when adding a test that combines PSK/certs,
    // persistence, and crypto storage.
    return 0;
}

int anj_crypto_storage_resolve_persistence_info(
        void *crypto_ctx,
        const void *data,
        size_t data_size,
        anj_crypto_security_info_external_t *out_info) {
    // TODO:
    return 0;
}

#    endif // ANJ_WITH_PERSISTENCE

#endif // ANJ_WITH_EXTERNAL_CRYPTO_STORAGE
