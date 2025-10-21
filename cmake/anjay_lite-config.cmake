# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)

project(anjay_lite)

set(gcc_or_clang
    CMAKE_C_COMPILER_ID
    MATCHES
    "GNU"
    OR
    CMAKE_C_COMPILER_ID
    MATCHES
    "Clang")

macro(define_overridable_option NAME TYPE DEFAULT_VALUE DESCRIPTION)
  if(DEFINED ${NAME})
    set(${NAME}_DEFAULT "${${NAME}}")
  else()
    set(${NAME}_DEFAULT "${DEFAULT_VALUE}")
  endif()
  set(${NAME}
      "${${NAME}_DEFAULT}"
      CACHE ${TYPE} "${DESCRIPTION}")
endmacro()

# Internal options
define_overridable_option(ANJ_TESTING BOOL OFF "Enable code unit tests")
define_overridable_option(ANJ_IWYU_PATH STRING "" "IWYU executable path")

# General options
define_overridable_option(ANJ_WITH_EXTRA_WARNINGS BOOL ON "Enable extra compilation warnings")

# input/output buffer sizes
define_overridable_option(ANJ_IN_MSG_BUFFER_SIZE STRING 1200 "Input message buffer size")
define_overridable_option(ANJ_OUT_MSG_BUFFER_SIZE STRING 1200 "Output message buffer size")
define_overridable_option(ANJ_OUT_PAYLOAD_BUFFER_SIZE STRING 1024 "Payload buffer size")

# data model configuration
define_overridable_option(ANJ_DM_MAX_OBJECTS_NUMBER STRING 10 "Max LwM2M Objects defined in data model")
define_overridable_option(ANJ_WITH_COMPOSITE_OPERATIONS BOOL ON "Enable composite operations support")
define_overridable_option(ANJ_DM_MAX_COMP_READ_ENTRIES STRING 5 "Max entries (paths) in a composite read operation")

# device object configuration
define_overridable_option(ANJ_WITH_DEFAULT_DEVICE_OBJ BOOL ON "Enable default implementation of Device Object")

# security object configuration
define_overridable_option(ANJ_WITH_DEFAULT_SECURITY_OBJ BOOL ON "Enable default implementation of Security Object")
define_overridable_option(ANJ_SEC_OBJ_MAX_PUBLIC_KEY_OR_IDENTITY_SIZE STRING 255 "Max Public Key or Identity Resource size")
define_overridable_option(ANJ_SEC_OBJ_MAX_SERVER_PUBLIC_KEY_SIZE STRING 255 "Max Server Public Key Resource size")
define_overridable_option(ANJ_SEC_OBJ_MAX_SECRET_KEY_SIZE STRING 255 "Max Secret Key Resource size")

# server object configuration
define_overridable_option(ANJ_WITH_DEFAULT_SERVER_OBJ BOOL ON "Enable default implementation of Server Object")

# FOTA object configuration
define_overridable_option(ANJ_WITH_DEFAULT_FOTA_OBJ BOOL ON "Enable default implementation of FW Update Object")
define_overridable_option(ANJ_FOTA_WITH_PULL_METHOD BOOL ON "Enable PULL method as FW delivery")
define_overridable_option(ANJ_FOTA_WITH_PUSH_METHOD BOOL ON "Enable PUSH method as FW delivery")
define_overridable_option(ANJ_FOTA_WITH_COAP BOOL ON "Enable CoAP support in FW Update Object")
define_overridable_option(ANJ_FOTA_WITH_COAPS BOOL OFF "Enable CoAPs support in FW Update Object")
define_overridable_option(ANJ_FOTA_WITH_HTTP BOOL OFF "Enable HTTP support in FW Update Object")
define_overridable_option(ANJ_FOTA_WITH_HTTPS BOOL OFF "Enable HTTPS support in FW Update Object")
define_overridable_option(ANJ_FOTA_WITH_COAP_TCP BOOL OFF "Enable TCP support in FW Update Object")
define_overridable_option(ANJ_FOTA_WITH_COAPS_TCP BOOL OFF "Enable TLS support in FW Update Object")

# CoAP downloader configuration
define_overridable_option(ANJ_WITH_COAP_DOWNLOADER BOOL OFF "Enable CoAP Downloader support")
define_overridable_option(ANJ_COAP_DOWNLOADER_MAX_PATHS_NUMBER STRING 3 "Max CoAP Paths number in CoAP Downloader")
define_overridable_option(ANJ_COAP_DOWNLOADER_MAX_MSG_SIZE STRING 1200 "Max CoAP message size used in CoAP Downloader")

# observe configuration
define_overridable_option(ANJ_WITH_OBSERVE BOOL ON "Enable Observe-Notify mechanism")
define_overridable_option(ANJ_WITH_OBSERVE_COMPOSITE BOOL OFF "Enable Observe-Composite support")
define_overridable_option(ANJ_OBSERVE_MAX_OBSERVATIONS_NUMBER STRING 10 "Max number of enabled observations")
define_overridable_option(ANJ_OBSERVE_MAX_WRITE_ATTRIBUTES_NUMBER STRING 10 "Max number of Attributes set with Write-Attributes")

# bootstrap configuration
define_overridable_option(ANJ_WITH_BOOTSTRAP BOOL ON "Enable Bootstrap Interface")
define_overridable_option(ANJ_WITH_BOOTSTRAP_DISCOVER BOOL ON "Enable Bootstrap-Discover support")

# discover configuration
define_overridable_option(ANJ_WITH_DISCOVER BOOL ON "Enable Discover support")
define_overridable_option(ANJ_WITH_DISCOVER_ATTR BOOL ON "Enable Discover to read Observation-Class attributes")

# LwM2M Send
define_overridable_option(ANJ_WITH_LWM2M_SEND BOOL ON "Enable LwM2M SEND operation support")
define_overridable_option(ANJ_LWM2M_SEND_QUEUE_SIZE STRING 1 "Max LwM2M SEND messages queued number")

# compat layer configuration
define_overridable_option(ANJ_WITH_TIME_POSIX_COMPAT BOOL ON "Enable POSIX-compliant integration of time API")
define_overridable_option(ANJ_WITH_RNG_POSIX_COMPAT BOOL ON "Enable POSIX-compliant integration of RNG API")
define_overridable_option(ANJ_WITH_SOCKET_POSIX_COMPAT BOOL ON "Enable POSIX-compliant integration of socket API")
define_overridable_option(ANJ_NET_WITH_IPV4 BOOL ON "Enable communication over IPv4")
define_overridable_option(ANJ_NET_WITH_IPV6 BOOL OFF "Enable communication over IPv6")
define_overridable_option(ANJ_NET_WITH_UDP BOOL ON "Enable communication over UDP")
define_overridable_option(ANJ_NET_WITH_TCP BOOL OFF "Enable communication over TCP")
define_overridable_option(ANJ_NET_WITH_DTLS BOOL OFF "Enable communication over DTLS")
define_overridable_option(ANJ_WITH_CRYPTO_STORAGE_DEFAULT BOOL OFF "Enable default implementation of crypto storage API")
define_overridable_option(ANJ_WITH_MBEDTLS BOOL OFF "Enable MbedTLS support")
define_overridable_option(ANJ_MBEDTLS_PSK_IDENTITY_MAX_LEN STRING 128 "Max PSK Identity length")
define_overridable_option(ANJ_MBEDTLS_TLS_VERSION STRING "MBEDTLS_SSL_VERSION_TLS1_2" "(D)TLS version supported by client, supported values are 
                                                                                       MBEDTLS_SSL_VERSION_TLS1_2 and MBEDTLS_SSL_VERSION_TLS1_3")
define_overridable_option(ANJ_MBEDTLS_ALLOWED_CIPHERSUITES STRING "MBEDTLS_TLS_PSK_WITH_AES_128_CCM_8,MBEDTLS_TLS_PSK_WITH_AES_256_CCM_8"
                                                                                     "List of allowed ciphersuites for MbedTLS")
define_overridable_option(ANJ_MBEDTLS_HS_INITIAL_TIMEOUT_VALUE_MS STRING 1000 "Initial handshake timeout value in milliseconds")
define_overridable_option(ANJ_MBEDTLS_HS_MAXIMUM_TIMEOUT_VALUE_MS STRING 60000 "Maximum handshake timeout value in milliseconds")

# security configuration
define_overridable_option(ANJ_WITH_SECURITY BOOL OFF "Enable security support")
define_overridable_option(ANJ_WITH_CERTIFICATES BOOL OFF "Enable certificates support")
define_overridable_option(ANJ_WITH_EXTERNAL_CRYPTO_STORAGE BOOL OFF "Enable external crypto storage API")

# data formats configuration
define_overridable_option(ANJ_WITH_CBOR BOOL ON "Enable CBOR format support")
define_overridable_option(ANJ_WITH_CBOR_DECODE_DECIMAL_FRACTIONS BOOL ON "Enable decimal fractions support in CBOR")
define_overridable_option(ANJ_WITH_CBOR_DECODE_HALF_FLOAT BOOL ON "Enable 16bit half floats support in CBOR")
define_overridable_option(ANJ_WITH_CBOR_DECODE_INDEFINITE_BYTES BOOL ON "Enable Indefinite bytes strings and arrays support in CBOR")
define_overridable_option(ANJ_WITH_CBOR_DECODE_STRING_TIME BOOL ON "Enable string representations of timestamp support in CBOR")
define_overridable_option(ANJ_WITH_LWM2M_CBOR BOOL ON "Enable LwM2M CBOR format support")
define_overridable_option(ANJ_WITH_SENML_CBOR BOOL ON "Enable SenML CBOR format support")
define_overridable_option(ANJ_WITH_PLAINTEXT BOOL ON "Enable Plaintext format support")
define_overridable_option(ANJ_WITH_OPAQUE BOOL ON "Enable Opaque format support")
define_overridable_option(ANJ_WITH_TLV BOOL ON "Enable TLV format support (decoder only)")
define_overridable_option(ANJ_WITH_EXTERNAL_DATA BOOL OFF "Enable External Data Type support")

# CoAP related configuration
define_overridable_option(ANJ_COAP_WITH_UDP BOOL ON "Enable CoAP over UDP transport")
define_overridable_option(ANJ_COAP_WITH_TCP BOOL OFF "Enable CoAP over TCP transport")
define_overridable_option(ANJ_COAP_MAX_OPTIONS_NUMBER STRING 15 "Max number of CoAP options in CoAP header")
define_overridable_option(ANJ_COAP_MAX_ATTR_OPTION_SIZE STRING 40 "Max Attribute-related CoAP option size")
define_overridable_option(ANJ_COAP_MAX_LOCATION_PATHS_NUMBER STRING 2 "Max CoAP Location-Paths number in Registration Interface")
define_overridable_option(ANJ_COAP_MAX_LOCATION_PATH_SIZE STRING 40 "Max size of a single CoAP Location-Path in Registration Interface")
define_overridable_option(ANJ_WITH_CACHE BOOL ON "Enable responses caching")
define_overridable_option(ANJ_CACHE_ENTRIES_NUMBER STRING 10 "Non-recent cache entries number")

# logger configuration
define_overridable_option(ANJ_LOG_FULL BOOL ON "Enable full logger: includes module, level, file, and line info")
define_overridable_option(ANJ_LOG_ALT_IMPL_HEADER STRING "" "Path to custom logger implementation header")
define_overridable_option(ANJ_LOG_FORMATTER_PRINTF BOOL ON "Use vsnprintf() for log message formatting")
define_overridable_option(ANJ_LOG_FORMATTER_BUF_SIZE STRING 512 "Log formatting buffer size")
define_overridable_option(ANJ_LOG_HANDLER_OUTPUT_STDERR BOOL ON "Output log messages to stderr")
define_overridable_option(ANJ_LOG_HANDLER_OUTPUT_ALT BOOL OFF "Use user-defined log output function")
define_overridable_option(ANJ_LOG_STRIP_CONSTANTS BOOL OFF "Replace disposable logs (ANJ_LOG_DISPOSABLE) with a space string")
define_overridable_option(ANJ_LOG_LEVEL_DEFAULT STRING L_INFO "Default log level threshold")
define_overridable_option(ANJ_LOG_FILTERING_CONFIG_HEADER STRING "" "Path to header with per-module log level overrides")

# persistence configuration
define_overridable_option(ANJ_WITH_PERSISTENCE BOOL OFF "Enable Persistence support")

# other configuration
define_overridable_option(ANJ_WITH_LWM2M12 BOOL ON "Enable LwM2M protocol version 1.2 support")
define_overridable_option(ANJ_WITH_CUSTOM_CONVERSION_FUNCTIONS BOOL ON "Enable custom string<->number conversion function")
define_overridable_option(ANJ_PLATFORM_BIG_ENDIAN BOOL OFF "Define platform endianess as big endian")

define_overridable_option(MBEDTLS_VERSION STRING "" "MbedTLS version to use when MBEDTLS_ROOT_DIR is not set, default is 3.6.0")
define_overridable_option(MBEDTLS_ROOT_DIR STRING "" "Path to MbedTLS root directory (if not set, MbedTLS will be fetched from GitHub)")

set(repo_root "${CMAKE_CURRENT_LIST_DIR}/..")
set(config_root "${CMAKE_BINARY_DIR}/anj_config")
set(config_base_path anj/anj_config.h)

configure_file("${repo_root}/include_public/${config_base_path}.in"
               "${config_root}/${config_base_path}")

file(GLOB_RECURSE anj_sources "${repo_root}/src/anj/*.c")

if(NOT HAVE_MATH_LIBRARY)
    foreach(MATH_LIBRARY_IT "" "m")
        file(WRITE ${CMAKE_BINARY_DIR}/CMakeFiles/CMakeTmp/fmod.c "#include <math.h>\nint main() { volatile double a = 4.0, b = 3.2; return (int) fmod(a, b); }\n\n")
        try_compile(HAVE_MATH_LIBRARY ${CMAKE_BINARY_DIR}/CMakeFiles/CMakeTmp ${CMAKE_BINARY_DIR}/CMakeFiles/CMakeTmp/fmod.c CMAKE_FLAGS "-DLINK_LIBRARIES=${MATH_LIBRARY_IT}")
        if(HAVE_MATH_LIBRARY)
            set(MATH_LIBRARY "${MATH_LIBRARY_IT}" CACHE STRING "Library that provides C math functions. Can be empty if no extra library is required." FORCE)
            break()
        endif()
    endforeach()
    if(NOT HAVE_MATH_LIBRARY)
        message(FATAL_ERROR "Math library is required. Provide it using HAVE_MATH_LIBRARY variable.")
    endif()
endif()

add_library(anj STATIC ${anj_sources})
target_include_directories(
  anj PUBLIC "${repo_root}/include_public" "${config_root}")
if(ANJ_TESTING)
  target_include_directories(anj PRIVATE "${repo_root}/tests/anj")
endif()

target_link_libraries(anj PUBLIC ${MATH_LIBRARY})

set_target_properties(anj PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED TRUE
  C_EXTENSIONS OFF
)

add_library(anj_extra_warning_flags INTERFACE)

if(gcc_or_clang)
  target_compile_options(anj_extra_warning_flags INTERFACE
          -pedantic
          -Wall
          -Wextra
          -Winit-self
          -Wmissing-declarations
          -Wc++-compat
          -Wsign-conversion
          -Wconversion
          -Wcast-qual
          -Wvla
          -Wno-variadic-macros
          -Wno-long-long
          -Wshadow
          )
  
  if (ANJ_WITH_EXTRA_WARNINGS)
    target_link_libraries(anj PRIVATE anj_extra_warning_flags)
  endif()

  if(ANJ_TESTING)
    target_compile_options(anj PUBLIC -Wno-pedantic -Wno-c++-compat)
  endif()
endif()

if(ANJ_WITH_MBEDTLS)
  include("${repo_root}/cmake/anjay_lite_mbedtls.cmake")
  target_link_libraries(anj PUBLIC ${MBEDTLS_TARGETS})
endif()
