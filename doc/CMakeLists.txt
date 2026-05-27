# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)
project(anjay_lite_docs NONE)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# Sources
set(SPHINX_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/sphinx")
set(DOC_SOURCE_MAIN "${SPHINX_DIR}/source")
set(DOC_SOURCE_API  "${SPHINX_DIR}/source_api")
set(INCLUDE_PUBLIC  "${REPO_ROOT}/include_public")

# Output (Build dir)
set(DOC_BUILD_DIR   "${REPO_ROOT}/build/doc")
set(DOXYGEN_OUTPUT  "${DOC_BUILD_DIR}/doxygen")
set(SPHINX_OUTPUT   "${DOC_BUILD_DIR}/html")

set(DOXYGEN_INPUT_PATHS "${DOXYGEN_OUTPUT}/config")

find_program(SPHINX_EXECUTABLE sphinx-build)
find_package(Doxygen)

if(SPHINX_EXECUTABLE AND DOXYGEN_FOUND AND
   EXISTS "${DOC_SOURCE_MAIN}/conf.py.in" AND 
   EXISTS "${DOC_SOURCE_API}/conf.py.in" AND
   EXISTS "${REPO_ROOT}/Doxyfile")

    # no template for Doxyfile in this project

    # configure Sphinx config file for main documentation
    set(CURRENT_TEMPLATES_DIR "${DOC_SOURCE_MAIN}/_templates")
    configure_file(${DOC_SOURCE_MAIN}/conf.py.in
                   ${DOC_BUILD_DIR}/sphinx/conf_main/conf.py
                   @ONLY)

    # configure Sphinx config file for API docs
    set(CURRENT_TEMPLATES_DIR "${DOC_SOURCE_API}/_templates")
    configure_file(${DOC_SOURCE_API}/conf.py.in
                   ${DOC_BUILD_DIR}/sphinx/conf_api/conf.py
                   @ONLY)

    # declare the common target for docs
    add_custom_target(doc
        # ensure headers in output folder are up-to-date and generate Doxygen XML files
        # (copy and build inside this script)
        COMMAND python3 ${REPO_ROOT}/tools/build_doxygen_docs.py
                "${DOXYGEN_INPUT_PATHS}"
                "${REPO_ROOT}/Doxyfile"
                "${DOXYGEN_OUTPUT}/doxygen.md5"

        # generate Sphinx API docs
        COMMAND python3 ${REPO_ROOT}/tools/build_sphinx_docs.py
                "${DOC_SOURCE_API}"
                "${DOC_BUILD_DIR}/sphinx/conf_api"
                "${SPHINX_OUTPUT}/api"
                "${SPHINX_OUTPUT}/api/sphinx.md5"
                "${DOXYGEN_OUTPUT}/xml"

        # generate Sphinx main documentation
        COMMAND python3 ${REPO_ROOT}/tools/build_sphinx_docs.py
                "${DOC_SOURCE_MAIN}"
                "${DOC_BUILD_DIR}/sphinx/conf_main"
                "${SPHINX_OUTPUT}"
                "${SPHINX_OUTPUT}/sphinx.md5"

        WORKING_DIRECTORY ${REPO_ROOT}
        COMMENT "Building Anjay Lite Documentation (Doxygen -> Sphinx API -> Sphinx Main)"
        VERBATIM
    )

    # sphinx-based .. snippet-source:: validation
    add_custom_target(test_doc_snippet
                COMMAND ${CMAKE_COMMAND} -E env
                        "CMAKE_SOURCE_DIR=${REPO_ROOT}"
                        "SNIPPET_SOURCE_MD5FILE=${SPHINX_DIR}/snippet_sources.md5"
                        "ANJ_SPHINX_DOC_CONF_DIR=${DOC_BUILD_DIR}/sphinx/conf_main"
                        "ANJ_SPHINX_DOC_ROOT_DIR=${DOC_SOURCE_MAIN}"
                        ${SPHINX_EXECUTABLE}
                        -Q -b snippet_source_lint
                        -c ${DOC_BUILD_DIR}/sphinx/conf_main
                        ${DOC_SOURCE_MAIN}
                        ${DOC_BUILD_DIR}/lint
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sphinx)
endif()
