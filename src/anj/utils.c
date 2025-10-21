/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#include <ctype.h>
#include <float.h>
#if defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH)
// Workaround for bug in gcc-arm-none-eabi
// https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1067692
#    include <sys/_stdint.h>
#endif // defined(__GNUC__) && defined(__arm__) && defined(__ARM_ARCH)
#include <inttypes.h> // IWYU pragma: keep
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <anj/defs.h>
#include <anj/log.h>
#include <anj/utils.h>

#include "utils.h"
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
#    include <assert.h>
#else // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
#    include <stdio.h>

#    define _SSCANF_FORMAT(Width, Type) "%" #    Width Type
#    define SSCANF_FORMAT(Width, Type) _SSCANF_FORMAT(Width, Type)

/** The values below do not include the terminating null character */
#    define ANJ_U64_STR_MAX_LEN_CONVERTED 20
ANJ_STATIC_ASSERT(ANJ_U64_STR_MAX_LEN == ANJ_U64_STR_MAX_LEN_CONVERTED,
                  ANJ_U64_STR_MAX_LEN_CONVERTED_MacroWrongValue);
#    define ANJ_I64_STR_MAX_LEN_CONVERTED 20
ANJ_STATIC_ASSERT(ANJ_I64_STR_MAX_LEN == ANJ_I64_STR_MAX_LEN_CONVERTED,
                  ANJ_I64_STR_MAX_LEN_CONVERTED_MacroWrongValue);
#    define ANJ_U32_STR_MAX_LEN_CONVERTED 10
ANJ_STATIC_ASSERT(ANJ_U32_STR_MAX_LEN == ANJ_U32_STR_MAX_LEN_CONVERTED,
                  ANJ_U32_STR_MAX_LEN_CONVERTED_MacroWrongValue);
#    define ANJ_DOUBLE_STR_MAX_LEN_CONVERTED 24
ANJ_STATIC_ASSERT(ANJ_DOUBLE_STR_MAX_LEN == ANJ_DOUBLE_STR_MAX_LEN_CONVERTED,
                  ANJ_DOUBLE_STR_MAX_LEN_CONVERTED_MacroWrongValue);
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS

#include "io/io.h"

#define INT64_MIN_STR "-9223372036854775808"

uint16_t _anj_determine_block_buffer_size(size_t buff_size) {
    if (buff_size < 16) {
        return 0;
    }
    size_t size = 16;
    while (size <= buff_size && size <= 1024) {
        size = (uint16_t) (size << 1);
    }
    return (uint16_t) (size >> 1);
}

bool anj_uri_path_increasing(const anj_uri_path_t *previous_path,
                             const anj_uri_path_t *current_path) {
    size_t path_to_check_len =
            ANJ_MIN(previous_path->uri_len, current_path->uri_len);

    for (size_t i = 0; i < path_to_check_len; i++) {
        if (current_path->ids[i] > previous_path->ids[i]) {
            return true;
        } else if (current_path->ids[i] < previous_path->ids[i]) {
            return false;
        }
    }
    return previous_path->uri_len < current_path->uri_len;
}

bool _anj_uri_path_to_security_or_oscore_obj(const anj_uri_path_t *path) {
    assert(path);
    if (anj_uri_path_has(path, ANJ_ID_OID)
            && (path->ids[ANJ_ID_OID] == ANJ_OBJ_ID_SECURITY
                || path->ids[ANJ_ID_OID] == ANJ_OBJ_ID_OSCORE)) {
        anj_log(utils, L_ERROR,
                "Access to Security and OSCORE Objects is prohibited");
        return true;
    }
    return false;
}

bool _anj_tokens_equal(const _anj_coap_token_t *left,
                       const _anj_coap_token_t *right) {
    return !(memcmp(left->bytes, right->bytes, left->size)
             || left->size != right->size);
}

int _anj_validate_obj_version(const char *version) {
    if (!version) {
        return 0;
    }
    // accepted format is X.Y where X and Y are digits
    if (!isdigit(version[0]) || version[1] != '.' || !isdigit(version[2])
            || version[3] != '\0') {
        return _ANJ_IO_ERR_INPUT_ARG;
    }
    return 0;
}

#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
static size_t uint64_to_string_value_internal(uint64_t value,
                                              char *out_buff,
                                              size_t *dot_position,
                                              bool ignore_zeros) {
    char buff[ANJ_U64_STR_MAX_LEN + 1] = { 0 };
    int idx = sizeof(buff) - 1;
    size_t msg_size = 0;
    do {
        char digit = (char) ('0' + (value % 10));
        value /= 10;
        if (ignore_zeros) {
            if (digit == '0') {
                continue;
            } else {
                ignore_zeros = false;
            }
        }
        buff[idx--] = digit;
        msg_size++;
        if (dot_position && *dot_position == msg_size) {
            buff[idx--] = '.';
            msg_size++;
        }
    } while (value > 0);
    assert(!dot_position || *dot_position < msg_size);

    memcpy(out_buff, &buff[idx + 1], msg_size);
    return msg_size;
}
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS

size_t anj_uint64_to_string_value(char *out_buff, uint64_t value) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    return uint64_to_string_value_internal(value, out_buff, NULL, false);
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    char buff[ANJ_U64_STR_MAX_LEN + 1];
    size_t ret = (size_t) snprintf(buff, sizeof(buff), "%" PRIu64, value);
    memcpy(out_buff, buff, ret);
    return ret;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

size_t anj_uint32_to_string_value(char *out_buff, uint32_t value) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    return uint64_to_string_value_internal((uint64_t) value, out_buff, NULL,
                                           false);
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    char buff[ANJ_U32_STR_MAX_LEN + 1];
    size_t ret = (size_t) snprintf(buff, sizeof(buff), "%" PRIu32, value);
    memcpy(out_buff, buff, ret);
    return ret;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

size_t anj_uint16_to_string_value(char *out_buff, uint16_t value) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    return uint64_to_string_value_internal((uint64_t) value, out_buff, NULL,
                                           false);
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    char buff[ANJ_U16_STR_MAX_LEN + 1];
    size_t ret = (size_t) snprintf(buff, sizeof(buff), "%" PRIu16, value);
    memcpy(out_buff, buff, ret);
    return ret;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

size_t anj_int64_to_string_value(char *out_buff, int64_t value) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    size_t msg_size = 0;

    /* Handle 0 and INT64_MIN cases */
    if (value == 0) {
        out_buff[0] = '0';
        return 1;
    } else if (value == INT64_MIN) {
        memcpy(out_buff, INT64_MIN_STR, sizeof(INT64_MIN_STR) - 1);
        return sizeof(INT64_MIN_STR) - 1;
    }

    if (value < 0) {
        out_buff[0] = '-';
        value = -value;
        msg_size++;
    }

    return msg_size
           + uint64_to_string_value_internal((uint64_t) value,
                                             &out_buff[msg_size], NULL, false);
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    char buff[ANJ_I64_STR_MAX_LEN + 1];
    size_t ret = (size_t) snprintf(buff, sizeof(buff), "%" PRId64, value);
    memcpy(out_buff, buff, ret);
    return ret;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

#define _MAX_FRACTION_SIZE_IN_EXPONENTIAL_NOTATION \
    (sizeof("2.2250738585072014") - 1 - 2)

size_t anj_double_to_string_value(char *out_buff, double value) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    size_t out_len = 0;
    char buff[ANJ_U64_STR_MAX_LEN + 1] = { 0 };
    size_t bytes_to_copy;

    if (isnan(value)) {
        memcpy(out_buff, "nan", 3);
        return 3;
    } else if (value == 0.0) {
        out_buff[0] = '0';
        return 1;
    }

    if (value < 0.0) {
        out_buff[out_len++] = '-';
        value = -value;
    }
    if (isinf(value)) {
        memcpy(&out_buff[out_len], "inf", 3);
        out_len += 3;
        return out_len;
    }

    // X.Y format
    if (value > 1.0 && value < (double) UINT64_MAX
            && (value - (double) (uint64_t) value > 0.0)) {
        size_t dot_position = 0;
        while (value - (double) (uint64_t) value > 0.0) {
            value = value * 10.0;
            dot_position++;
        }
        out_len += uint64_to_string_value_internal(
                (uint64_t) value, &out_buff[out_len], &dot_position, false);
    } else if (value >= 1.0 && value < (double) UINT64_MAX) { // X format
        out_len += uint64_to_string_value_internal(
                (uint64_t) value, &out_buff[out_len], NULL, false);
    } else if (value >= (double) UINT64_MAX) { // X.YeZ format
        size_t exponential_value = 0;
        double temp_value = value;
        while (temp_value >= 10.0) {
            temp_value = temp_value / 10.0;
            exponential_value++;
        }
        while (value > (double) UINT64_MAX) {
            value = value / 10.0;
        }
        bytes_to_copy = uint64_to_string_value_internal((uint64_t) value, buff,
                                                        NULL, true);
        out_buff[out_len++] = buff[0];
        bytes_to_copy--;
        if (bytes_to_copy) {
            out_buff[out_len++] = '.';
            bytes_to_copy = ANJ_MIN(bytes_to_copy,
                                    _MAX_FRACTION_SIZE_IN_EXPONENTIAL_NOTATION);
            memcpy(&out_buff[out_len], &buff[1], bytes_to_copy);
            out_len += bytes_to_copy;
        }
        out_buff[out_len++] = 'e';
        out_buff[out_len++] = '+';
        out_len += uint64_to_string_value_internal(
                (uint64_t) exponential_value, &out_buff[out_len], NULL, false);
    } else if (value < 1 && value > 1e-10) { // 0.X format
        size_t nil_counter = 0;
        while (value - (double) (uint64_t) value > 0.0) {
            value = value * 10.0;
            nil_counter++;
        }

        bytes_to_copy = uint64_to_string_value_internal((uint64_t) value, buff,
                                                        NULL, false);
        size_t nil_to_add = nil_counter - bytes_to_copy;
        memcpy(&out_buff[out_len], "0.", 2);
        out_len += 2;
        if (nil_to_add > 0) {
            memset(&out_buff[out_len], '0', nil_to_add);
            out_len += nil_to_add;
        }
        memcpy(&out_buff[out_len], buff, bytes_to_copy);
        out_len += bytes_to_copy;
    } else { // X.Ye-Z format
        size_t exponential_value = 0;
        double temp_value = value;
        while (temp_value < 1.0) {
            temp_value = temp_value * 10.0;
            exponential_value++;
        }
        while (value - (double) (uint64_t) value > 0.0) {
            value = value * 10.0;
        }
        bytes_to_copy = uint64_to_string_value_internal((uint64_t) value, buff,
                                                        NULL, true);
        out_buff[out_len++] = buff[0];
        bytes_to_copy--;
        if (bytes_to_copy) {
            out_buff[out_len++] = '.';
            bytes_to_copy = ANJ_MIN(bytes_to_copy,
                                    _MAX_FRACTION_SIZE_IN_EXPONENTIAL_NOTATION);
        }
        memcpy(&out_buff[out_len], &buff[1], bytes_to_copy);
        out_len += bytes_to_copy;
        out_buff[out_len++] = 'e';
        out_buff[out_len++] = '-';
        out_len += uint64_to_string_value_internal(
                (uint64_t) exponential_value, &out_buff[out_len], NULL, false);
    }

    return out_len;
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    size_t ret;
    char buff[ANJ_DOUBLE_STR_MAX_LEN + 1];
    double abs_value = fabs(value);
    if (abs_value > 1e-5 && abs_value < (double) UINT64_MAX) {
        if (abs_value > 0.1) {
            ret = (size_t) snprintf(buff, sizeof(buff), "%.5f", value);
        } else {
            ret = (size_t) snprintf(buff, sizeof(buff), "%.10f", value);
        }
    } else {
        ret = (size_t) snprintf(buff, sizeof(buff), "%e", value);
    }
    memcpy(out_buff, buff, ret);
    return ret;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

int anj_string_to_uint32_value(uint32_t *out_val,
                               const char *buff,
                               size_t buff_len) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    uint64_t value;
    if (anj_string_to_uint64_value(&value, buff, buff_len)) {
        return -1;
    }
    if (value > UINT32_MAX) {
        return -1;
    }
    *out_val = (uint32_t) value;
    return 0;
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    for (size_t i = 0; i < buff_len; i++) {
        if (!isdigit(buff[i])) {
            return -1;
        }
    }

    if (buff_len == 0 || buff_len > ANJ_U32_STR_MAX_LEN) {
        return -1; // too short or too long buffer
    }

    uint64_t value;
    char buff_copy[ANJ_U32_STR_MAX_LEN + 1] = { 0 };
    memcpy(buff_copy, buff, buff_len);

    if (sscanf(buff_copy, SSCANF_FORMAT(ANJ_U32_STR_MAX_LEN_CONVERTED, SCNu64),
               &value)
            != 1) {
        return -1;
    }

    if (value > UINT32_MAX) {
        return -1;
    }

    *out_val = (uint32_t) value;

    return 0;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

int anj_string_to_objlnk_value(anj_objlnk_value_t *out, const char *objlnk) {
    const char *colon;
    uint32_t oid, iid;
    if (!(colon = strchr(objlnk, ':'))
            || anj_string_to_uint32_value(&oid, objlnk,
                                          (size_t) (colon - objlnk))
            || oid > UINT16_MAX
            || anj_string_to_uint32_value(&iid, colon + 1, strlen(colon + 1))
            || iid > UINT16_MAX) {
        return -1;
    }
    out->oid = (anj_oid_t) oid;
    out->iid = (anj_iid_t) iid;
    return 0;
}

int anj_string_to_uint64_value(uint64_t *out_val,
                               const char *buff,
                               size_t buff_len) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    uint64_t value;
    size_t i = 0;

    if (buff_len == 0 || buff_len > ANJ_U64_STR_MAX_LEN) {
        return -1; // too short or too long buffer
    }

    for (value = 0; i < buff_len && (buff[i] - '0' >= 0 && buff[i] - '0' <= 9);
         i++) {
        uint64_t prev_value = value;
        value = 10 * value + ((uint64_t) (buff[i] - '0'));
        if (value < prev_value) {
            return -1; // overflow detected
        }
    }

    if (i < buff_len) {
        return -1; // incorrect buffer
    }

    *out_val = value;
    return 0;
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    for (size_t i = 0; i < buff_len; i++) {
        if (!isdigit(buff[i])) {
            return -1;
        }
    }

    if (buff_len == 0 || buff_len > ANJ_U64_STR_MAX_LEN) {
        return -1; // too short or too long buffer
    }

    uint64_t value;
    char buff_copy[ANJ_U64_STR_MAX_LEN + 1] = { 0 };
    memcpy(buff_copy, buff, buff_len);

    if (sscanf(buff_copy, SSCANF_FORMAT(ANJ_U64_STR_MAX_LEN_CONVERTED, SCNu64),
               &value)
            != 1) {
        return -1;
    }

    *out_val = value;

    return 0;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

int anj_string_to_int64_value(int64_t *out_val,
                              const char *buff,
                              size_t buff_len) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    uint64_t value;
    int64_t sign;
    size_t i = 0;

    if (buff_len == 0) {
        return -1; // too short buffer
    }

    sign = (buff[i] == '-') ? -1 : 1;

    if (buff[i] == '+' || buff[i] == '-') {
        i++;
    }

    if (anj_string_to_uint64_value(&value, &buff[i], buff_len - i)) {
        return -1;
    }

    if (value > INT64_MAX) {
        if (value - 1 == INT64_MAX && sign == -1) {
            *out_val = INT64_MIN;
            return 0;
        }
        return -1;
    }
    *out_val = sign * (int64_t) value;
    return 0;
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    for (size_t i = 0; i < buff_len; i++) {
        if (!(isdigit(buff[i])
              || (i == 0 && (buff[i] == '+' || buff[i] == '-')))) {
            return -1;
        }
    }

    if (buff_len == 0 || buff_len > ANJ_I64_STR_MAX_LEN) {
        return -1; // too short or too long buffer
    }

    int64_t value;
    char buff_copy[ANJ_I64_STR_MAX_LEN + 1] = { 0 };
    memcpy(buff_copy, buff, buff_len);

    if (sscanf(buff_copy, SSCANF_FORMAT(ANJ_I64_STR_MAX_LEN_CONVERTED, SCNd64),
               &value)
            != 1) {
        return -1;
    }

    *out_val = value;

    return 0;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

int anj_string_to_double_value(double *out_val,
                               const char *buff,
                               size_t buff_len) {
#ifdef ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    // handle the exponent notation
    for (size_t i = 0; i < buff_len; i++) {
        if (buff[i] == 'e' || buff[i] == 'E') {
            if (buff_len > ANJ_DOUBLE_STR_MAX_LEN) {
                return -1;
            }
            double mantissa;
            if (anj_string_to_double_value(&mantissa, buff, i)) {
                return -1;
            }
            int64_t exponent;
            if (anj_string_to_int64_value(&exponent, &buff[i + 1],
                                          buff_len - i - 1)) {
                return -1;
            }
            if (exponent > DBL_MAX_10_EXP || exponent < DBL_MIN_10_EXP) {
                return -1;
            }
            *out_val = mantissa * pow(10, (double) exponent);
            return 0;
        }
    }

    bool is_negative = false;
    if (!buff_len) {
        return -1;
    }
    if (buff[0] == '-') {
        is_negative = true;
        buff++;
        buff_len--;
    }
    if (!buff_len) {
        return -1;
    }

    bool is_fractional_part = false;
    for (size_t i = buff_len; i > 0; i--) {
        if (buff[i - 1] == '.') {
            is_fractional_part = true;
            break;
        }
    }

    uint8_t single_value;
    double fractional_value = 0;
    double fractional_divider = 1.0;
    double integer_value = 0;
    double multiplicator = 1.0;
    for (size_t i = buff_len; i > 0; i--) {
        if (buff[i - 1] == '.') {
            is_fractional_part = false;
            multiplicator = 1;
        } else {
            single_value = (uint8_t) buff[i - 1] - '0';
            if (single_value > 9) {
                return -1; // incorrect buffer
            }
            if (is_fractional_part) {
                fractional_value += single_value * multiplicator;
                fractional_divider *= 10;
            } else {
                integer_value += single_value * multiplicator;
            }
            multiplicator = multiplicator * 10;
        }
    }

    *out_val = integer_value;

    if (fractional_value > 0.0) {
        *out_val += fractional_value / fractional_divider;
    }
    if (is_negative) {
        *out_val = -(*out_val);
    }

    return 0;
#else  // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
    if (buff_len == 0 || buff_len > ANJ_DOUBLE_STR_MAX_LEN) {
        return -1; // too short or too long buffer
    }

    double value;
    char buff_copy[ANJ_DOUBLE_STR_MAX_LEN + 1] = { 0 };
    memcpy(buff_copy, buff, buff_len);

    if (sscanf(buff_copy, SSCANF_FORMAT(ANJ_DOUBLE_STR_MAX_LEN_CONVERTED, "lf"),
               &value)
            != 1) {
        return -1;
    }

    *out_val = value;

    return 0;
#endif // ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS
}

#ifdef ANJ_PLATFORM_BIG_ENDIAN
uint16_t _anj_convert_be16(uint16_t value) {
    return value;
}

uint32_t _anj_convert_be32(uint32_t value) {
    return value;
}

uint64_t _anj_convert_be64(uint64_t value) {
    return value;
}
#else // ANJ_PLATFORM_BIG_ENDIAN
uint16_t _anj_convert_be16(uint16_t value) {
    return (uint16_t) ((value >> 8) | (value << 8));
}

uint32_t _anj_convert_be32(uint32_t value) {
    return (uint32_t) ((value >> 24) | ((value & 0xFF0000) >> 8)
                       | ((value & 0xFF00) << 8) | (value << 24));
}

uint64_t _anj_convert_be64(uint64_t value) {
    return (uint64_t) ((value >> 56)
                       | ((value & UINT64_C(0xFF000000000000)) >> 40)
                       | ((value & UINT64_C(0xFF0000000000)) >> 24)
                       | ((value & UINT64_C(0xFF00000000)) >> 8)
                       | ((value & UINT64_C(0xFF000000)) << 8)
                       | ((value & UINT64_C(0xFF0000)) << 24)
                       | ((value & UINT64_C(0xFF00)) << 40) | (value << 56));
}

uint32_t _anj_htonf(float f) {
    ANJ_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t), float_sane);
    union {
        float f;
        uint32_t ieee;
    } conv;
    conv.f = f;
    return _anj_convert_be32(conv.ieee);
}

uint64_t _anj_htond(double d) {
    ANJ_STATIC_ASSERT(sizeof(double) == sizeof(uint64_t), float_sane);
    union {
        double d;
        uint64_t ieee;
    } conv;
    conv.d = d;
    return _anj_convert_be64(conv.ieee);
}

float _anj_ntohf(uint32_t v) {
    union {
        float f;
        uint32_t ieee;
    } conv;
    conv.ieee = _anj_convert_be32(v);
    return conv.f;
}

double _anj_ntohd(uint64_t v) {
    union {
        double d;
        uint64_t ieee;
    } conv;
    conv.ieee = _anj_convert_be64(v);
    return conv.d;
}

static bool is_double_within_int64_range(double value) {
    ANJ_STATIC_ASSERT(INT64_MIN != -INT64_MAX, standard_enforces_u2_for_intN_t);
    static const double DOUBLE_2_63 = (double) (((uint64_t) 1) << 63);
    // max == 2^63 - 1; min == -2^63
    return value >= -DOUBLE_2_63 && value < DOUBLE_2_63;
    // note that the largest value representable as IEEE 754 double that is
    // smaller than 2^63 is actually 2^63 - 1024
}

bool _anj_double_convertible_to_int64(double value) {
    return nearbyint(value) == value && is_double_within_int64_range(value);
}

static bool is_double_within_uint64_range(double value) {
    static const double DOUBLE_2_64 = 2.0 * (double) (((uint64_t) 1) << 63);
    // max == 2^64 - 1; min == 0
    return value >= 0 && value < DOUBLE_2_64;
    // note that the largest value representable as IEEE 754 double that is
    // smaller than 2^64 is actually 2^64 - 2048
}

bool _anj_double_convertible_to_uint64(double value) {
    return nearbyint(value) == value && is_double_within_uint64_range(value);
}

char *_anj_coap_code_format(char (*buff)[5], uint32_t code) {
    char *buf = buff[0];
    uint32_t major = code >> ANJ_COAP_CODE_CLASS_SHIFT;
    uint32_t minor = code & ANJ_COAP_CODE_DETAIL_MASK;
    anj_uint32_to_string_value(buf, major);
    buf[1] = '.';
    char *minor_target = buf + 2;
    minor_target[0] = '0';
    anj_uint32_to_string_value(minor > 9 ? minor_target : (minor_target + 1),
                               minor);
    return buf;
}

#endif // ANJ_PLATFORM_BIG_ENDIAN
