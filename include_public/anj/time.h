/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

/**
 * @file
 * @brief Strongly-typed time utilities used across Anjay Lite.
 *
 * The module provides:
 * - a @ref anj_time_unit_t enumeration of time units,
 * - opaque structs for relative durations and absolute times
 *   (@ref anj_time_duration_t, @ref anj_time_monotonic_t, @ref
 * anj_time_real_t),
 * - arithmetic, comparison and conversion helpers,
 * - convenience stringification helpers intended for logging and diagnostics.
 *
 * @note All durations are represented internally as signed 64-bit microseconds.
 *       Arithmetic operations do not check for overflow. Use with values that
 *       fit into the 64-bit range.
 *
 * @see anj_time_duration_new
 * @see anj_time_real_new
 * @see anj_time_monotonic_new
 * @see anj_time_scalar_from_duration
 * @see ANJ_TIME_DURATION_AS_STRING
 */

#ifndef ANJ_TIME_H
#    define ANJ_TIME_H

#    include <inttypes.h>
#    include <math.h>
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    ifdef __cplusplus
extern "C" {
#    endif

/**
 * @brief Time unit of a scalar value.
 *
 * Used by conversion helpers to interpret or present time quantities.
 */
typedef enum {
    /**
     * @brief Days (24 hours).
     */
    ANJ_TIME_UNIT_DAY,

    /**
     * @brief Hours (60 minutes).
     */
    ANJ_TIME_UNIT_HOUR,

    /**
     * @brief Minutes (60 seconds).
     */
    ANJ_TIME_UNIT_MIN,

    /**
     * @brief Seconds (1000 milliseconds).
     */
    ANJ_TIME_UNIT_S,

    /**
     * @brief Milliseconds (1000 microseconds).
     */
    ANJ_TIME_UNIT_MS,

    /**
     * @brief Microseconds (1/1000 milliseconds).
     */
    ANJ_TIME_UNIT_US,
} anj_time_unit_t;

/**
 * @brief Maximum value for a duration in microseconds.
 *
 * Used as a sentinel to represent an invalid duration.
 */
#    define ANJ_TIME_US_MAX INT64_MAX

/**
 * @brief Maximum length for buffers created by
 * @ref ANJ_TIME_DURATION_AS_STRING and related macros.
 *
 * Includes one extra byte for a null terminator.
 */
#    define ANJ_TIME_DURATION_AS_STRING_MAX_LEN (sizeof("-9223372036854775808"))

/**
 * @brief Relative duration of time.
 *
 * Durations are stored as a signed 64-bit number of microseconds. Negative
 * values are allowed.
 */
typedef struct {
    /**
     * @brief Number of microseconds.
     */
    int64_t us;
} anj_time_duration_t;

/**
 * @brief Absolute monotonic time (time since boot).
 *
 * Represents time measured on a monotonic clock—i.e., one that is not subject
 * to system time adjustments. Useful for measuring intervals.
 */
typedef struct {
    /**
     * @brief Duration since an unspecified monotonic epoch (e.g., boot).
     */
    anj_time_duration_t since_monotonic_epoch;
} anj_time_monotonic_t;

/**
 * @brief Absolute real (calendar) time.
 *
 * Represents wall-clock time as a duration since the Unix epoch.
 */
typedef struct {
    /**
     * @brief Duration since January 1, 1970, 00:00:00 UTC.
     */
    anj_time_duration_t since_real_epoch;
} anj_time_real_t;

/** @brief Zero duration (0 microseconds). */
extern const anj_time_duration_t ANJ_TIME_DURATION_ZERO;
/** @brief Real time set to 0 (Unix epoch). */
extern const anj_time_real_t ANJ_TIME_REAL_ZERO;
/** @brief Monotonic time set to 0 (monotonic epoch). */
extern const anj_time_monotonic_t ANJ_TIME_MONOTONIC_ZERO;
/** @brief Sentinel duration representing “infinity”. */
extern const anj_time_duration_t ANJ_TIME_DURATION_INVALID;
/** @brief Sentinel real time representing “infinity”. */
extern const anj_time_real_t ANJ_TIME_REAL_INVALID;
/** @brief Sentinel monotonic time representing “infinity”. */
extern const anj_time_monotonic_t ANJ_TIME_MONOTONIC_INVALID;

/**
 * @brief Creates a duration from an integer scalar in the given @p unit.
 *
 * @param scalar Integer value to convert.
 * @param unit   Unit of @p scalar.
 * @return Duration expressed in microseconds.
 *
 * @warning No overflow checking is performed.
 */
anj_time_duration_t anj_time_duration_new(int64_t scalar, anj_time_unit_t unit);

/**
 * @brief Creates a duration from a floating-point scalar in the given @p unit.
 *
 * The value is converted to microseconds and truncated toward zero.
 *
 * @param scalar Floating-point value to convert.
 * @param unit   Unit of @p scalar.
 * @return Duration expressed in microseconds.
 *
 * @warning No overflow checking is performed. Precision may be lost due to
 *          conversion to integer microseconds.
 */
anj_time_duration_t anj_time_duration_fnew(double scalar, anj_time_unit_t unit);

/**
 * @brief Converts a duration to an integer scalar in the given @p unit.
 *
 * The value is truncated toward zero.
 *
 * @param duration Duration to convert.
 * @param unit     Target unit.
 * @return Integer scalar value in @p unit.
 */
int64_t anj_time_duration_to_scalar(anj_time_duration_t duration,
                                    anj_time_unit_t unit);

/**
 * @brief Converts a duration to a floating-point scalar in the given @p unit.
 *
 * @param duration Duration to convert.
 * @param unit     Target unit.
 * @return Floating-point scalar value in @p unit.
 */
double anj_time_duration_to_fscalar(anj_time_duration_t duration,
                                    anj_time_unit_t unit);

/**
 * @brief Checks whether a duration value is valid.
 *
 * @param duration Duration to check.
 * @return `true` if @p duration is neither the invalid sentinel nor otherwise
 *         invalid; `false` otherwise.
 */
static inline bool anj_time_duration_is_valid(anj_time_duration_t duration) {
    if (duration.us == ANJ_TIME_US_MAX) {
        return false;
    }
    return true;
}

/**
 * @brief Adds two durations.
 *
 * @param lhs Left-hand operand.
 * @param rhs Right-hand operand.
 * @return `lhs + rhs`, or @ref ANJ_TIME_DURATION_INVALID if any operand is
 *         invalid.
 *
 * @warning No overflow checking is performed.
 */
static inline anj_time_duration_t
anj_time_duration_add(const anj_time_duration_t lhs,
                      const anj_time_duration_t rhs) {
    if (!anj_time_duration_is_valid(lhs) || !anj_time_duration_is_valid(rhs)) {
        return ANJ_TIME_DURATION_INVALID;
    }

    const anj_time_duration_t result = { lhs.us + rhs.us };
    return result;
}

/**
 * @brief Subtracts two durations.
 *
 * @param lhs Left-hand operand.
 * @param rhs Right-hand operand.
 * @return `lhs - rhs`, or @ref ANJ_TIME_DURATION_INVALID if any operand is
 *         invalid.
 *
 * @warning No overflow checking is performed.
 */
static inline anj_time_duration_t
anj_time_duration_sub(const anj_time_duration_t lhs,
                      const anj_time_duration_t rhs) {
    if (!anj_time_duration_is_valid(lhs) || !anj_time_duration_is_valid(rhs)) {
        return ANJ_TIME_DURATION_INVALID;
    }

    const anj_time_duration_t result = { lhs.us - rhs.us };
    return result;
}

/**
 * @brief Multiplies a duration by an integer factor.
 *
 * @param duration Duration to scale.
 * @param factor   Integer factor.
 * @return Scaled duration, or @ref ANJ_TIME_DURATION_INVALID if @p duration
 *         is invalid or @p factor is not supported.
 *
 * @warning No overflow checking is performed.
 */
static inline anj_time_duration_t
anj_time_duration_mul(const anj_time_duration_t duration,
                      const int32_t factor) {
    if (!anj_time_duration_is_valid(duration) || factor == INT32_MAX) {
        return ANJ_TIME_DURATION_INVALID;
    }
    const anj_time_duration_t result = { duration.us * factor };
    return result;
}

/**
 * @brief Multiplies a duration by a floating-point factor.
 *
 * The intermediate result is computed in double and then truncated when
 * converted back to microseconds.
 *
 * @param duration Duration to scale.
 * @param factor   Floating-point factor.
 * @return Scaled duration, or @ref ANJ_TIME_DURATION_INVALID if inputs are
 *         invalid or @p factor is NaN/Inf.
 *
 * @warning Precision may be lost; no overflow checking is performed.
 */
static inline anj_time_duration_t
anj_time_duration_fmul(const anj_time_duration_t duration,
                       const double factor) {
    if (!anj_time_duration_is_valid(duration) || isinf(factor)
            || isnan(factor)) {
        return ANJ_TIME_DURATION_INVALID;
    }

    const anj_time_duration_t result = { (int64_t) ((double) duration.us
                                                    * factor) };
    return result;
}

/**
 * @brief Divides a duration by an integer divisor.
 *
 * @param duration Duration to divide.
 * @param divisor  Integer divisor (must not be 0).
 * @return Quotient duration, or @ref ANJ_TIME_DURATION_INVALID if inputs are
 *         invalid or @p divisor is 0.
 *
 * @warning Division by zero is undefined. No overflow checking is performed.
 */
static inline anj_time_duration_t
anj_time_duration_div(const anj_time_duration_t duration,
                      const int32_t divisor) {
    if (!anj_time_duration_is_valid(duration) || divisor == 0) {
        return ANJ_TIME_DURATION_INVALID;
    }

    const anj_time_duration_t result = { duration.us / divisor };
    return result;
}

/** @brief Equality comparison for durations. */
static inline bool anj_time_duration_eq(const anj_time_duration_t lhs,
                                        const anj_time_duration_t rhs) {
    if (!anj_time_duration_is_valid(lhs) || !anj_time_duration_is_valid(rhs)) {
        return false;
    }

    return lhs.us == rhs.us;
}
/** @brief Less-than comparison for durations. */
static inline bool anj_time_duration_lt(const anj_time_duration_t lhs,
                                        const anj_time_duration_t rhs) {
    if (!anj_time_duration_is_valid(lhs) || !anj_time_duration_is_valid(rhs)) {
        return false;
    }

    return lhs.us < rhs.us;
}
/** @brief Greater-than comparison for durations. */
static inline bool anj_time_duration_gt(const anj_time_duration_t lhs,
                                        const anj_time_duration_t rhs) {
    if (!anj_time_duration_is_valid(lhs) || !anj_time_duration_is_valid(rhs)) {
        return false;
    }

    return lhs.us > rhs.us;
}
/** @brief Less-or-equal comparison for durations. */
static inline bool anj_time_duration_leq(const anj_time_duration_t lhs,
                                         const anj_time_duration_t rhs) {
    if (!anj_time_duration_is_valid(lhs) || !anj_time_duration_is_valid(rhs)) {
        return false;
    }

    return lhs.us <= rhs.us;
}
/** @brief Greater-or-equal comparison for durations. */
static inline bool anj_time_duration_geq(const anj_time_duration_t lhs,
                                         const anj_time_duration_t rhs) {
    if (!anj_time_duration_is_valid(lhs) || !anj_time_duration_is_valid(rhs)) {
        return false;
    }

    return lhs.us >= rhs.us;
}

/**
 * @brief Internal helper that renders a duration to a string buffer.
 *
 * Returns either:
 * - a pointer to @p buffer with the formatted integer scalar in the requested
 *   unit,
 * - the literal string "<invalid>" if @p duration equals
 *   @ref ANJ_TIME_DURATION_INVALID, or
 * - the literal string "<error>" on conversion failure.
 *
 * @param duration Duration to print.
 * @param unit     Unit to print in.
 * @param buffer   Pointer to a character array of size
 *                 @ref ANJ_TIME_DURATION_AS_STRING_MAX_LEN.
 * @return Pointer to a C string as described above.
 *
 * @internal Prefer the public convenience macros below instead of calling
 *           this function directly.
 * @see ANJ_TIME_DURATION_AS_STRING
 * @see ANJ_TIME_MONOTONIC_AS_STRING
 * @see ANJ_TIME_REAL_AS_STRING
 */
const char *_anj_time_duration_as_string_impl(
        anj_time_duration_t duration,
        anj_time_unit_t unit,
        char (*buffer)[ANJ_TIME_DURATION_AS_STRING_MAX_LEN]);

/**
 * @brief Formats a duration as a string in the given unit.
 *
 * The macro allocates a temporary stack buffer and returns a pointer to it,
 * making it convenient for logging:
 *
 * @code
 * anj_log(my_module, L_DEBUG, "Timeout=%s",
 *         ANJ_TIME_DURATION_AS_STRING(timeout, ANJ_TIME_UNIT_MS));
 * @endcode
 *
 * @param duration Duration to format.
 * @param unit     Unit to print the value in.
 * @return `const char *` to a NUL-terminated string valid until the end of
 *         the full expression (implementation uses a compound literal).
 */
#    define ANJ_TIME_DURATION_AS_STRING(duration, unit) \
        (_anj_time_duration_as_string_impl(             \
                duration,                               \
                unit,                                   \
                &(char[ANJ_TIME_DURATION_AS_STRING_MAX_LEN]){ "" }))

/**
 * @brief Constructs a monotonic timestamp from a duration since the monotonic
 * epoch.
 *
 * @param duration Duration since the monotonic epoch.
 * @return Monotonic timestamp equal to @p duration after the epoch.
 */
static inline anj_time_monotonic_t
anj_time_monotonic_from_duration(const anj_time_duration_t duration) {
    const anj_time_monotonic_t result = { duration };
    return result;
}

/**
 * @brief Converts a monotonic timestamp to a duration since the monotonic
 * epoch.
 *
 * @param time Monotonic timestamp to convert.
 * @return Duration since the monotonic epoch.
 */
static inline anj_time_duration_t
anj_time_monotonic_to_duration(const anj_time_monotonic_t time) {
    return time.since_monotonic_epoch;
}

/**
 * @brief Creates a monotonic timestamp from an integer scalar and unit.
 *
 * @param scalar Integer value.
 * @param unit   Unit of @p scalar.
 * @return Monotonic timestamp.
 */
static inline anj_time_monotonic_t
anj_time_monotonic_new(const int64_t scalar, const anj_time_unit_t unit) {
    return anj_time_monotonic_from_duration(
            anj_time_duration_new(scalar, unit));
}

/**
 * @brief Creates a monotonic timestamp from a floating-point scalar and unit.
 *
 * @param scalar Floating-point value.
 * @param unit   Unit of @p scalar.
 * @return Monotonic timestamp.
 */
static inline anj_time_monotonic_t
anj_time_monotonic_fnew(const double scalar, const anj_time_unit_t unit) {
    return anj_time_monotonic_from_duration(
            anj_time_duration_fnew(scalar, unit));
}

/**
 * @brief Checks whether a monotonic timestamp is valid.
 *
 * @param time Timestamp to check.
 * @return `true` if valid; `false` otherwise.
 */
static inline bool
anj_time_monotonic_is_valid(const anj_time_monotonic_t time) {
    return anj_time_duration_is_valid(anj_time_monotonic_to_duration(time));
}

/**
 * @brief Converts a monotonic timestamp to an integer scalar in the given unit.
 *
 * @param time Timestamp to convert.
 * @param unit Target unit.
 * @return Integer scalar.
 */
static inline int64_t
anj_time_monotonic_to_scalar(const anj_time_monotonic_t time,
                             const anj_time_unit_t unit) {
    return anj_time_duration_to_scalar(anj_time_monotonic_to_duration(time),
                                       unit);
}

/**
 * @brief Converts a monotonic timestamp to a floating-point scalar.
 *
 * @param time Timestamp to convert.
 * @param unit Target unit.
 * @return Floating-point scalar.
 */
static inline double
anj_time_monotonic_to_fscalar(const anj_time_monotonic_t time,
                              const anj_time_unit_t unit) {
    return anj_time_duration_to_fscalar(anj_time_monotonic_to_duration(time),
                                        unit);
}

/**
 * @brief Adds a relative duration to a monotonic timestamp.
 *
 * @param start    Base monotonic time.
 * @param duration Duration to add.
 * @return `start + duration`.
 */
static inline anj_time_monotonic_t
anj_time_monotonic_add(const anj_time_monotonic_t start,
                       const anj_time_duration_t duration) {
    return anj_time_monotonic_from_duration(anj_time_duration_add(
            anj_time_monotonic_to_duration(start), duration));
}

/**
 * @brief Subtracts a relative duration from a monotonic timestamp.
 *
 * @param start    Base monotonic time.
 * @param duration Duration to subtract.
 * @return `start - duration`.
 */
static inline anj_time_monotonic_t
anj_time_monotonic_sub(const anj_time_monotonic_t start,
                       const anj_time_duration_t duration) {
    return anj_time_monotonic_from_duration(anj_time_duration_sub(
            anj_time_monotonic_to_duration(start), duration));
}

/**
 * @brief Computes the difference between two monotonic timestamps.
 *
 * @param end   Later timestamp.
 * @param start Earlier timestamp.
 * @return `end - start` as a duration (may be negative if `end < start`).
 */
static inline anj_time_duration_t
anj_time_monotonic_diff(const anj_time_monotonic_t end,
                        const anj_time_monotonic_t start) {
    return anj_time_duration_sub(anj_time_monotonic_to_duration(end),
                                 anj_time_monotonic_to_duration(start));
}

/** @brief Less-than comparison for monotonic timestamps. */
static inline bool anj_time_monotonic_lt(const anj_time_monotonic_t lhs,
                                         const anj_time_monotonic_t rhs) {
    return anj_time_duration_lt(anj_time_monotonic_to_duration(lhs),
                                anj_time_monotonic_to_duration(rhs));
}
/** @brief Greater-than comparison for monotonic timestamps. */
static inline bool anj_time_monotonic_gt(const anj_time_monotonic_t lhs,
                                         const anj_time_monotonic_t rhs) {
    return anj_time_duration_gt(anj_time_monotonic_to_duration(lhs),
                                anj_time_monotonic_to_duration(rhs));
}
/** @brief Equality comparison for monotonic timestamps. */
static inline bool anj_time_monotonic_eq(const anj_time_monotonic_t lhs,
                                         const anj_time_monotonic_t rhs) {
    return anj_time_duration_eq(anj_time_monotonic_to_duration(lhs),
                                anj_time_monotonic_to_duration(rhs));
}
/** @brief Less-or-equal comparison for monotonic timestamps. */
static inline bool anj_time_monotonic_leq(const anj_time_monotonic_t lhs,
                                          const anj_time_monotonic_t rhs) {
    return anj_time_duration_leq(anj_time_monotonic_to_duration(lhs),
                                 anj_time_monotonic_to_duration(rhs));
}
/** @brief Greater-or-equal comparison for monotonic timestamps. */
static inline bool anj_time_monotonic_geq(const anj_time_monotonic_t lhs,
                                          const anj_time_monotonic_t rhs) {
    return anj_time_duration_geq(anj_time_monotonic_to_duration(lhs),
                                 anj_time_monotonic_to_duration(rhs));
}

/**
 * @brief Formats a monotonic timestamp as a duration string in the given unit.
 *
 * Useful for logs that want to show “time since boot” in milliseconds, etc.
 *
 * @param time Monotonic timestamp.
 * @param unit Unit to print the value in.
 * @return Pointer to a temporary NUL-terminated string; see
 *         @ref ANJ_TIME_DURATION_AS_STRING for lifetime details.
 */
#    define ANJ_TIME_MONOTONIC_AS_STRING(time, unit) \
        (_anj_time_duration_as_string_impl(          \
                time.since_monotonic_epoch,          \
                unit,                                \
                &(char[ANJ_TIME_DURATION_AS_STRING_MAX_LEN]){ "" }))

/**
 * @brief Constructs a real (wall-clock) timestamp from a duration since Unix
 * epoch.
 *
 * @param duration Duration since the Unix epoch.
 * @return Real timestamp equal to @p duration after the epoch.
 */
static inline anj_time_real_t
anj_time_real_from_duration(const anj_time_duration_t duration) {
    const anj_time_real_t result = { duration };
    return result;
}

/**
 * @brief Converts a real timestamp to a duration since the Unix epoch.
 *
 * @param time Real timestamp to convert.
 * @return Duration since the Unix epoch.
 */
static inline anj_time_duration_t
anj_time_real_to_duration(const anj_time_real_t time) {
    return time.since_real_epoch;
}

/**
 * @brief Creates a real timestamp from an integer scalar and unit.
 *
 * @param scalar Integer value.
 * @param unit   Unit of @p scalar.
 * @return Real timestamp.
 */
static inline anj_time_real_t anj_time_real_new(const int64_t scalar,
                                                const anj_time_unit_t unit) {
    return anj_time_real_from_duration(anj_time_duration_new(scalar, unit));
}

/**
 * @brief Creates a real timestamp from a floating-point scalar and unit.
 *
 * @param scalar Floating-point value.
 * @param unit   Unit of @p scalar.
 * @return Real timestamp.
 */
static inline anj_time_real_t anj_time_real_fnew(const double scalar,
                                                 const anj_time_unit_t unit) {
    return anj_time_real_from_duration(anj_time_duration_fnew(scalar, unit));
}

/**
 * @brief Converts a real timestamp to an integer scalar in the given unit.
 *
 * @param time Real timestamp to convert.
 * @param unit Target unit.
 * @return Integer scalar.
 */
static inline int64_t anj_time_real_to_scalar(const anj_time_real_t time,
                                              const anj_time_unit_t unit) {
    return anj_time_duration_to_scalar(anj_time_real_to_duration(time), unit);
}

/**
 * @brief Converts a real timestamp to a floating-point scalar.
 *
 * @param time Real timestamp to convert.
 * @param unit Target unit.
 * @return Floating-point scalar.
 */
static inline double anj_time_real_to_fscalar(const anj_time_real_t time,
                                              const anj_time_unit_t unit) {
    return anj_time_duration_to_fscalar(anj_time_real_to_duration(time), unit);
}

/**
 * @brief Checks whether a real timestamp is valid.
 *
 * @param time Timestamp to check.
 * @return `true` if valid; `false` otherwise.
 */
static inline bool anj_time_real_is_valid(const anj_time_real_t time) {
    return anj_time_duration_is_valid(anj_time_real_to_duration(time));
}

/**
 * @brief Adds a relative duration to a real timestamp.
 *
 * @param start    Base real time.
 * @param duration Duration to add.
 * @return `start + duration`.
 */
static inline anj_time_real_t
anj_time_real_add(const anj_time_real_t start,
                  const anj_time_duration_t duration) {
    return anj_time_real_from_duration(
            anj_time_duration_add(anj_time_real_to_duration(start), duration));
}

/**
 * @brief Subtracts a relative duration from a real timestamp.
 *
 * @param start    Base real time.
 * @param duration Duration to subtract.
 * @return `start - duration`.
 */
static inline anj_time_real_t
anj_time_real_sub(const anj_time_real_t start,
                  const anj_time_duration_t duration) {
    return anj_time_real_from_duration(
            anj_time_duration_sub(anj_time_real_to_duration(start), duration));
}

/**
 * @brief Computes the difference between two real timestamps.
 *
 * @param end   Later timestamp.
 * @param start Earlier timestamp.
 * @return `end - start` as a duration (may be negative).
 */
static inline anj_time_duration_t
anj_time_real_diff(const anj_time_real_t end, const anj_time_real_t start) {
    return anj_time_duration_sub(anj_time_real_to_duration(end),
                                 anj_time_real_to_duration(start));
}

/** @brief Less-than comparison for real timestamps. */
static inline bool anj_time_real_lt(const anj_time_real_t lhs,
                                    const anj_time_real_t rhs) {
    return anj_time_duration_lt(anj_time_real_to_duration(lhs),
                                anj_time_real_to_duration(rhs));
}
/** @brief Greater-than comparison for real timestamps. */
static inline bool anj_time_real_gt(const anj_time_real_t lhs,
                                    const anj_time_real_t rhs) {
    return anj_time_duration_gt(anj_time_real_to_duration(lhs),
                                anj_time_real_to_duration(rhs));
}
/** @brief Equality comparison for real timestamps. */
static inline bool anj_time_real_eq(const anj_time_real_t lhs,
                                    const anj_time_real_t rhs) {
    return anj_time_duration_eq(anj_time_real_to_duration(lhs),
                                anj_time_real_to_duration(rhs));
}
/** @brief Less-or-equal comparison for real timestamps. */
static inline bool anj_time_real_leq(const anj_time_real_t lhs,
                                     const anj_time_real_t rhs) {
    return anj_time_duration_leq(anj_time_real_to_duration(lhs),
                                 anj_time_real_to_duration(rhs));
}
/** @brief Greater-or-equal comparison for real timestamps. */
static inline bool anj_time_real_geq(const anj_time_real_t lhs,
                                     const anj_time_real_t rhs) {
    return anj_time_duration_geq(anj_time_real_to_duration(lhs),
                                 anj_time_real_to_duration(rhs));
}

/**
 * @brief Formats a real timestamp as a duration string in the given unit.
 *
 * This prints the numeric offset from the Unix epoch (e.g., in seconds or
 * milliseconds). For human-readable calendars, convert externally.
 *
 * @param time Real timestamp.
 * @param unit Unit to print the value in.
 * @return Pointer to a temporary NUL-terminated string; see
 *         @ref ANJ_TIME_DURATION_AS_STRING for lifetime details.
 */
#    define ANJ_TIME_REAL_AS_STRING(time, unit) \
        (_anj_time_duration_as_string_impl(     \
                time.since_real_epoch,          \
                unit,                           \
                &(char[ANJ_TIME_DURATION_AS_STRING_MAX_LEN]){ "" }))

#    ifdef __cplusplus
}
#    endif

#endif // ANJ_TIME_H
