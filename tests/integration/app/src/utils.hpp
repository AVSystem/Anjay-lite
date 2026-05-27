/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef APP_UTILS_HPP_
#define APP_UTILS_HPP_

#include <functional>
#include <json.hpp>
#include <tuple>
#include <type_traits>

namespace utils {
using json = nlohmann::json;

/**
 * @brief Converts elements of a JSON array into a tuple of requested types.
 *
 * This helper expands the compile-time index sequence and deserializes
 * `j.at(I)` into the corresponding tuple element type.
 *
 * Argument types are decayed before deserialization, so reference and
 * cv-qualified function parameter types such as `const std::string&` are
 * converted into storable value types such as `std::string`.
 *
 * @tparam Args Types of the target tuple elements.
 * @tparam I Indices of JSON array elements to deserialize.
 * @param j JSON array containing function arguments.
 * @param[in] unused Compile-time index sequence used for parameter pack
 * expansion.
 *
 * @return Tuple containing deserialized argument values.
 */
template <typename... Args, std::size_t... I>
std::tuple<std::decay_t<Args>...>
json_to_tuple_impl(const json &j, std::index_sequence<I...>) {
    return std::tuple<std::decay_t<Args>...>(
            j.at(I).get<std::decay_t<Args>>()...);
}

/**
 * @brief Converts a JSON array into a tuple of requested argument types.
 *
 * The input must be a JSON array whose size matches the number of requested
 * argument types. Each array element is deserialized into the corresponding
 * tuple element.
 *
 * Types are decayed before deserialization, which allows this function to work
 * with function signatures containing references or cv-qualified parameters.
 *
 * @tparam Args Target argument types.
 * @param j JSON array containing serialized arguments.
 * @return Tuple containing deserialized argument values.
 *
 * @throws std::runtime_error If @p j is not a JSON array.
 * @throws std::runtime_error If the number of JSON elements does not match
 *         the number of requested argument types.
 * @throws nlohmann::json::exception If any element cannot be deserialized
 *         into the requested type.
 */
template <typename... Args>
std::tuple<std::decay_t<Args>...> json_to_tuple(const json &j) {
    if (!j.is_array()) {
        throw std::runtime_error("RPC args must be a JSON array");
    }
    if (j.size() != sizeof...(Args)) {
        throw std::runtime_error("RPC args count mismatch");
    }
    return json_to_tuple_impl<Args...>(j, std::index_sequence_for<Args...>{});
}

/**
 * @brief Invokes a function using arguments deserialized from JSON.
 *
 * The input JSON array is converted into a tuple of function arguments and then
 * passed to the target function via `std::apply`. The return value is converted
 * back into JSON.
 *
 * @tparam Ret Return type of the wrapped function.
 * @tparam Args Argument types of the wrapped function.
 * @param fn Pointer to the function to invoke.
 * @param input JSON array containing serialized function arguments.
 * @return JSON representation of the function result.
 */
template <typename Ret, typename... Args>
json call_from_json(Ret (*fn)(Args...), const json &input) {
    auto args_tuple = json_to_tuple<Args...>(input);
    Ret result = std::apply(fn, args_tuple);
    return json(result);
}

/**
 * @brief Invokes a `void` function using arguments deserialized from JSON.
 *
 * The input JSON array is converted into a tuple of function arguments and then
 * passed to the target function via `std::apply`.
 *
 * @tparam Args Argument types of the wrapped function.
 * @param fn Pointer to the function to invoke.
 * @param input JSON array containing serialized function arguments.
 * @return Empty JSON value represented as `{}`.
 */
template <typename... Args>
json call_from_json(void (*fn)(Args...), const json &input) {
    auto args_tuple = json_to_tuple<Args...>(input);
    std::apply(fn, args_tuple);
    return {};
}

/**
 * @brief Type-erased RPC handler signature.
 *
 * A handler takes JSON-encoded arguments and returns a JSON-encoded result.
 */
using Handler = std::function<nlohmann::json(const nlohmann::json &)>;

/**
 * @brief Wraps a plain function pointer as a generic JSON-based handler.
 *
 * `AutoWrap` adapts a function with a typed signature into a callable object
 * accepting a JSON array of arguments and returning a JSON result.
 */
struct AutoWrap {
    Handler fn;

    /**
     * @brief Constructs a wrapper for a typed function pointer.
     *
     * @tparam Ret Return type of the wrapped function.
     * @tparam Args Argument types of the wrapped function.
     * @param func Function pointer to wrap.
     */
    template <typename Ret, typename... Args>
    AutoWrap(Ret (*func)(Args...)) {
        fn = [func](const nlohmann::json &j) {
            return call_from_json(func, j);
        };
    }

    /**
     * @brief Converts the wrapper to the underlying handler type.
     *
     * @return Stored type-erased handler.
     */
    operator Handler() const {
        return fn;
    }
};

} // namespace utils

/**
 * @brief Extends `nlohmann::json` support for `std::optional`.
 *
 * This specialization allows optional values to be serialized and deserialized
 * as follows:
 * - engaged optional -> serialized as the contained value
 * - empty optional -> serialized as `null`
 *
 * During deserialization:
 * - `null` becomes `std::nullopt`
 * - any other JSON value is deserialized as `T`
 *
 * This is sufficient for use with structures defined via
 * `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`, including cases where
 * optional fields are omitted from the input JSON and default-initialized.
 *
 * source: https://www.kdab.com/jsonify-with-nlohmann-json/
 *
 * @tparam T Contained optional value type.
 */
template <typename T>
struct nlohmann::adl_serializer<std::optional<T>> {
    static void to_json(json &j, const std::optional<T> &opt) {
        if (opt.has_value()) {
            j = *opt;
        } else {
            j = nullptr;
        }
    }

    static void from_json(const json &j, std::optional<T> &opt) {
        if (j.is_null()) {
            opt = std::nullopt;
        } else {
            opt = j.get<T>();
        }
    }
};

#endif // APP_UTILS_HPP_
