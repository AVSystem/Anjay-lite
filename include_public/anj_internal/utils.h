/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJ_INTERNAL_UTILS_H
#define ANJ_INTERNAL_UTILS_H

#ifndef ANJ_INTERNAL_INCLUDE_UTILS
#    error "Internal header must not be included directly"
#endif // ANJ_INTERNAL_INCLUDE_UTILS

#ifdef __cplusplus
extern "C" {
#endif

/**@{*/
/** @anj_internal_api_do_not_use */
#define _ANJ_VARARG_LENGTH_INTERNAL__(_5, _4, _3, _2, _1, N, ...) N

/** @anj_internal_api_do_not_use */
#define _ANJ_CONCAT_RAW_INTERNAL__(prefix, suffix) prefix##suffix

/** @anj_internal_api_do_not_use */
#define _ANJ_CONCAT_INTERNAL__(prefix, suffix) \
    _ANJ_CONCAT_RAW_INTERNAL__(prefix, suffix)

/** @anj_internal_api_do_not_use */
#define _ANJ_CONCAT_INTERNAL_1__(_1) _1

/** @anj_internal_api_do_not_use */
#define _ANJ_CONCAT_INTERNAL_2__(_1, _2) _ANJ_CONCAT_INTERNAL__(_1, _2)

/** @anj_internal_api_do_not_use */
#define _ANJ_CONCAT_INTERNAL_3__(_1, ...) \
    _ANJ_CONCAT_INTERNAL__(_1, _ANJ_CONCAT_INTERNAL_2__(__VA_ARGS__))

/** @anj_internal_api_do_not_use */
#define _ANJ_CONCAT_INTERNAL_4__(_1, ...) \
    _ANJ_CONCAT_INTERNAL__(_1, _ANJ_CONCAT_INTERNAL_3__(__VA_ARGS__))

/** @anj_internal_api_do_not_use */
#define ANJ_CONCAT_INTERNAL_5__(_1, ...) \
    _ANJ_CONCAT_INTERNAL__(_1, _ANJ_CONCAT_INTERNAL_4__(__VA_ARGS__))

/**
 * @anj_internal_api_do_not_use
 * Calculates the number of arguments to the macro. Works with up to 5
 * arguments.
 */
#define _ANJ_VARARG_LENGTH(...) \
    _ANJ_VARARG_LENGTH_INTERNAL__(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
/**@}*/

#define _ANJ_URI_PATH_INITIALIZER(Oid, Iid, Rid, Riid, Len) \
    {                                                       \
        .uri_len = Len,                                     \
        .ids = {                                            \
            [ANJ_ID_OID] = (Oid),                           \
            [ANJ_ID_IID] = (Iid),                           \
            [ANJ_ID_RID] = (Rid),                           \
            [ANJ_ID_RIID] = (Riid)                          \
        }                                                   \
    }

#define _ANJ_MAKE_URI_PATH(...) \
    ((anj_uri_path_t) _ANJ_URI_PATH_INITIALIZER(__VA_ARGS__))

#ifdef __cplusplus
}
#endif

#endif // ANJ_INTERNAL_UTILS_H
