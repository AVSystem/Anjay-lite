/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 56

// IWYU pragma: begin_keep
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// IWYU pragma: end_keep

#if defined(ANJ_WITH_CRYPTO_STORAGE_DEFAULT)

#    include <anj/compat/crypto/storage.h>
#    include <anj/crypto.h>
#    include <anj/log.h>
#    include <anj/utils.h>

#    define data_loader_log(...) anj_log(crypto_data_loader, __VA_ARGS__)

#    define FILE_NAME "crypto_record_%d.dat"
#    define MAX_FILE_NAME_SIZE 25
// 3 resources per instance, 2 instances in Security Object
#    define MAX_RECORDS 6

typedef struct {
    char file_name[MAX_RECORDS][MAX_FILE_NAME_SIZE];
    FILE *fp;
    int occupied_slots;
} anj_default_crypto_storage_t;

int anj_crypto_storage_init(void **out_crypto_ctx) {
    assert(out_crypto_ctx);
    anj_default_crypto_storage_t *ctx = (anj_default_crypto_storage_t *) malloc(
            1 * sizeof(anj_default_crypto_storage_t));
    if (!ctx) {
        data_loader_log(L_ERROR, "Allocation failed");
        return -1;
    }
    memset(ctx, 0, sizeof(anj_default_crypto_storage_t));
    *out_crypto_ctx = ctx;
    return 0;
}

void anj_crypto_storage_deinit(void *out_crypto_ctx) {
    assert(out_crypto_ctx);
    anj_default_crypto_storage_t *ctx =
            (anj_default_crypto_storage_t *) out_crypto_ctx;
    if (ctx->fp) {
        fclose(ctx->fp);
    }
    free(ctx);
    ctx = NULL;
}

int anj_crypto_storage_create_new_record(void *crypto_ctx,
                                         anj_crypto_security_info_t *out_info) {
    assert(crypto_ctx && out_info);
    assert(out_info->source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    anj_default_crypto_storage_t *ctx =
            (anj_default_crypto_storage_t *) crypto_ctx;
    if (ctx->occupied_slots >= MAX_RECORDS) {
        data_loader_log(L_ERROR, "No more available records");
        return -1;
    }
    // find space for new record
    anj_crypto_security_info_external_t *external_info =
            &out_info->info.external;
    for (int i = 0; i < MAX_RECORDS; ++i) {
        if (ctx->file_name[i][0] == '\0') { // empty slot
            snprintf(ctx->file_name[i], MAX_FILE_NAME_SIZE, FILE_NAME, i);
            external_info->identity = ctx->file_name[i];
            ctx->occupied_slots++;
            // file exist but we don't track it - so we need to delete it
            if (access(external_info->identity, F_OK) == 0) {
                data_loader_log(L_WARNING,
                                "File '%s' already exists, deleting it",
                                external_info->identity);
                if (remove(external_info->identity) == 0) {
                    data_loader_log(L_INFO, "file deleted successfully");
                }
            }
            return 0;
        }
    }
    data_loader_log(L_ERROR, "Internal error: no empty slots found");
    return -1;
}

int anj_crypto_storage_store_data(void *crypto_ctx,
                                  const anj_crypto_security_info_t *info,
                                  const void *data,
                                  size_t data_size,
                                  bool last_chunk) {
    assert(crypto_ctx && info);
    assert(info->source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);

    anj_default_crypto_storage_t *ctx =
            (anj_default_crypto_storage_t *) crypto_ctx;
    const anj_crypto_security_info_external_t *external_info =
            &info->info.external;

    // If file is not open yet, open it for appending in binary mode
    if (!ctx->fp) {
        ctx->fp = fopen(external_info->identity, "ab");
        if (!ctx->fp) {
            data_loader_log(L_ERROR, "Failed to open file '%s': %s",
                            external_info->identity, strerror(errno));
            return -1;
        }
        data_loader_log(L_INFO, "Opened file '%s' for writing data",
                        external_info->identity);
    }
    // Write data
    if (data_size > 0) {
        size_t written = fwrite(data, 1, data_size, ctx->fp);
        if (written != data_size) {
            fclose(ctx->fp);
            ctx->fp = NULL;
            data_loader_log(L_ERROR, "Failed to write data to file '%s': %s",
                            external_info->identity, strerror(errno));
            return -1;
        }
    }
    // If this is the last chunk, close file permanently
    if (last_chunk) {
        if (fclose(ctx->fp) != 0) {
            data_loader_log(L_ERROR, "Failed to close file '%s': %s",
                            external_info->identity, strerror(errno));
            ctx->fp = NULL;
            return -1;
        }
        ctx->fp = NULL;
        data_loader_log(L_INFO, "Finished writing to file '%s'",
                        external_info->identity);
    }
    return 0;
}

int anj_crypto_storage_delete_record(void *crypto_ctx,
                                     const anj_crypto_security_info_t *info) {
    assert(crypto_ctx && info);
    assert(info->source == ANJ_CRYPTO_DATA_SOURCE_EXTERNAL);
    anj_default_crypto_storage_t *ctx =
            (anj_default_crypto_storage_t *) crypto_ctx;
    const anj_crypto_security_info_external_t *external_info =
            &info->info.external;

    if (!external_info->identity || strlen(external_info->identity) == 0) {
        data_loader_log(L_ERROR, "Invalid record identity");
        return -1;
    }

    // Find the record in the storage table
    int idx = -1;
    for (int i = 0; i < MAX_RECORDS; ++i) {
        if (ctx->file_name[i][0] != '\0'
                && strcmp(ctx->file_name[i], external_info->identity) == 0) {
            idx = i;
            break;
        }
    }
    // If file is open — close it
    if (ctx->fp) {
        fclose(ctx->fp);
        ctx->fp = NULL;
    }
    // Remove file from disk
    if (remove(external_info->identity) != 0) {
        data_loader_log(L_ERROR, "Failed to delete file '%s': %s",
                        external_info->identity, strerror(errno));
        // If file deletion failed, we still want to remove it from the table
    }
    // Remove entry from local structure (if found)
    if (idx >= 0) {
        assert(ctx->occupied_slots > 0);
        ctx->occupied_slots--;
        data_loader_log(L_INFO, "Deleted security record '%s'",
                        external_info->identity);
        ctx->file_name[idx][0] = '\0'; // mark as empty
        return 0;
    }
    // Not found in table — maybe it was already deleted?
    data_loader_log(L_WARNING,
                    "Record '%s' not tracked in storage table",
                    external_info->identity);
    return 0;
}

#    ifdef ANJ_WITH_PERSISTENCE

int anj_crypto_storage_get_persistence_info(
        void *crypto_ctx,
        const anj_crypto_security_info_external_t *info,
        void *out_data,
        size_t *out_data_size) {
    (void) crypto_ctx; // Unused here
    assert(info && out_data);
    if (!info->identity || info->identity[0] == '\0') {
        return -1; // Invalid identity
    }
    memcpy(out_data, info->identity, strlen(info->identity));
    *out_data_size = strlen(info->identity);
    return 0;
}

int anj_crypto_storage_resolve_persistence_info(
        void *crypto_ctx,
        const void *data,
        size_t data_size,
        anj_crypto_security_info_external_t *out_info) {
    assert(crypto_ctx && data && out_info);
    if (data_size == 0) {
        data_loader_log(L_ERROR, "Empty data buffer");
        return -1;
    }

    anj_default_crypto_storage_t *ctx =
            (anj_default_crypto_storage_t *) crypto_ctx;

    // The incoming data buffer should be a string representing the identity.
    // Ensure it is null-terminated for safety by copying to a temporary buffer.
    char identity[MAX_FILE_NAME_SIZE];
    if (data_size >= sizeof(identity)) {
        data_loader_log(L_ERROR, "Identity too long");
        return -1;
    }
    memcpy(identity, data, data_size);
    identity[data_size] = '\0'; // Null-terminate

    // If the identity is already tracked, just return it
    for (int i = 0; i < MAX_RECORDS; ++i) {
        if (ctx->file_name[i][0] != '\0'
                && strcmp(ctx->file_name[i], identity) == 0) {
            out_info->identity = ctx->file_name[i];
            data_loader_log(L_INFO,
                            "Identity '%s' already exists, returning it",
                            out_info->identity);
            return 0;
        }
    }

    // Verify that the file actually exists on disk
    FILE *test = fopen(identity, "rb");
    if (!test) {
        data_loader_log(L_ERROR, "Identity file '%s' does not exist: %s",
                        identity, strerror(errno));
        return -1;
    }
    fclose(test);

    // Find an empty slot to store this identity
    int slot = -1;
    for (int i = 0; i < MAX_RECORDS; ++i) {
        if (ctx->file_name[i][0] == '\0') {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        data_loader_log(L_ERROR, "No free slot available for identity '%s'",
                        identity);
        return -1;
    }
    // Store the identity string in the table
    memcpy(ctx->file_name[slot], identity, strlen(identity));
    ctx->occupied_slots++;
    data_loader_log(L_INFO, "Resolved identity '%s' to slot %d", identity,
                    slot);
    out_info->identity = ctx->file_name[slot];
    return 0;
}
#    endif // ANJ_WITH_PERSISTENCE

int anj_crypto_storage_resolve_security_info(
        void *crypto_ctx,
        anj_crypto_security_info_external_t *info,
        char *out_buffer,
        size_t out_buffer_size,
        size_t *out_record_size) {
    assert(crypto_ctx && info && out_record_size && out_buffer);

    data_loader_log(L_INFO, "Resolving security info %s", info->identity);
    // read from file and store it in out_buffer
    if (!info->identity || info->identity[0] == '\0') {
        data_loader_log(L_ERROR, "External identity is empty");
        return -1;
    }

    // Open the file for reading in binary mode
    FILE *fp = fopen(info->identity, "rb");
    if (!fp) {
        data_loader_log(L_ERROR, "Failed to open file '%s': %s", info->identity,
                        strerror(errno));
        return -1;
    }

    // Determine file size
    if (fseek(fp, 0, SEEK_END) != 0) {
        data_loader_log(L_ERROR, "Failed to seek to end of '%s': %s",
                        info->identity, strerror(errno));
        fclose(fp);
        return -1;
    }
    long fsize = ftell(fp);
    if (fsize < 0) {
        data_loader_log(L_ERROR, "Failed to tell size of '%s': %s",
                        info->identity, strerror(errno));
        fclose(fp);
        return -1;
    }
    if (fsize == 0) {
        data_loader_log(L_INFO, "File '%s' is empty", info->identity);
        fclose(fp);
        *out_record_size = 0;
        return 0;
    }
    if ((size_t) fsize > out_buffer_size) {
        data_loader_log(L_ERROR,
                        "File '%s' too large (%ld bytes) for buffer cap %u",
                        info->identity, fsize, (unsigned) out_buffer_size);
        fclose(fp);
        return -1;
    }
    // Rewind and read the whole file
    rewind(fp);
    size_t to_read = (size_t) fsize;
    size_t nread = fread((void *) out_buffer, 1, to_read, fp);
    if (nread != to_read) {
        data_loader_log(L_ERROR, "Failed to read file '%s': %s", info->identity,
                        ferror(fp) ? strerror(errno) : "unexpected EOF");
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        data_loader_log(L_ERROR, "Failed to close file '%s': %s",
                        info->identity, strerror(errno));
        return -1;
    }

    *out_record_size = nread;
    return 0;
}

#endif // ANJ_WITH_CRYPTO_STORAGE_DEFAULT
