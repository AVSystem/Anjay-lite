# Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

cmake_minimum_required(VERSION 3.16.0)
project(anjay_lite_docs NONE)

find_program(SPHINX_EXECUTABLE sphinx-build)

add_custom_target(doc)

if(NOT SPHINX_EXECUTABLE)
    message(FATAL_ERROR "Sphinx was not found. Please install Sphinx and make sure it's in your PATH.")
endif()

set(repo_root "${CMAKE_CURRENT_LIST_DIR}/..")
set(SPHINX_DIR ${repo_root}/doc/sphinx)
set(SPHINX_SOURCE ${repo_root}/doc/sphinx/source)
set(SPHINX_BUILD ${repo_root}/doc/build/sphinx)
set(SNIPPET_SOURCE_MD5FILE ${SPHINX_DIR})
file(MAKE_DIRECTORY ${repo_root}/doc/build)

if(EXISTS "${SPHINX_SOURCE}/conf.py.in")
    set(DOC_VERSION "2.0.0")

    configure_file(${SPHINX_SOURCE}/conf.py.in
                   ${SPHINX_DIR}/conf.py
                   @ONLY)

    add_custom_target(doc_sphinx
        COMMAND ${SPHINX_EXECUTABLE}
             -b html ${SPHINX_SOURCE}
             -c ${SPHINX_DIR}
             ${SPHINX_BUILD}
        WORKING_DIRECTORY ${SPHINX_DIR}
        COMMENT "Generating HTML documentation with Sphinx"
    )
    add_dependencies(doc doc_sphinx)

    # sphinx-based .. snippet-source:: validation
    add_custom_target(test_doc_snippet
                COMMAND ${CMAKE_COMMAND} -E env
                        "CMAKE_SOURCE_DIR=${repo_root}"
                        "SNIPPET_SOURCE_MD5FILE=${SPHINX_DIR}/snippet_sources.md5"
                        "ANJ_SPHINX_DOC_CONF_DIR=${SPHINX_DIR}"
                        "ANJ_SPHINX_DOC_ROOT_DIR=${SPHINX_SOURCE}"
                        ${SPHINX_EXECUTABLE}
                        -Q -b snippet_source_lint
                        -c ${SPHINX_DIR}
                        ${SPHINX_SOURCE}
                        ${SPHINX_BUILD}/lint
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sphinx)
endif()

if(EXISTS "${repo_root}/Doxyfile")
    find_package(Doxygen)

    message(STATUS "Doxygen target being added")

    # configure anj_config.h with default options...
    configure_file("${repo_root}/include_public/anj/anj_config.h.in"
               "${repo_root}/doc/build/anj/anj_config.h")
    # ... and manually enable every option so it's visible in doxygen
    file(READ "${repo_root}/doc/build/anj/anj_config.h" ANJ_CONFIG_CONTENT)
    string(REGEX REPLACE
        "/\\* *#undef ([A-Za-z0-9_]+) *\\*/"
        "#define \\1"
        ANJ_CONFIG_CONTENT
        "${ANJ_CONFIG_CONTENT}"
    )
    file(WRITE "${repo_root}/doc/build/anj/anj_config.h" "${ANJ_CONFIG_CONTENT}")

    file(WRITE "${CMAKE_BINARY_DIR}/check_doxygen_css.cmake"
        "if(NOT EXISTS \"${repo_root}/tools/doxygen-awesome-css\" OR\n"
        "   NOT EXISTS \"${repo_root}/tools/doxygen-awesome-css/doxygen-awesome.css\")\n"
        "    message(FATAL_ERROR \"ERROR: Doxygen Awesome CSS submodule is missing or not initialized!\\n\"\n"
        "                        \"Run the following command to fix this issue:\\n\\n\"\n"
        "                        \"    git submodule update --init --recursive\\n\")\n"
        "endif()\n"
    )

    add_custom_target(doc_doxygen
        COMMAND ${CMAKE_COMMAND} -E echo "Checking Doxygen Awesome CSS..."
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/check_doxygen_css.cmake"
        COMMAND ${CMAKE_COMMAND} -E echo "Doxygen Awesome CSS found, generating documentation..."
        COMMAND ${DOXYGEN_EXECUTABLE} ${repo_root}/Doxyfile
        WORKING_DIRECTORY ${repo_root}
        DEPENDS ${repo_root}/Doxyfile
    )

    add_dependencies(doc doc_doxygen)
endif()
