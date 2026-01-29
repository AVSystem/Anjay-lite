..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

External data types
===================

Overview
--------

In some scenarios, implementing `String` or `Opaque` resources with the
``anj_bytes_or_string_value_t`` structure and the ``ANJ_DATA_TYPE_BYTES`` / 
``ANJ_DATA_TYPE_STRING`` data types may be inefficient or even impossible. This
structure requires the entire resource value to be provided in a single buffer,
which forces you to:

    - store the resource's value in program memory
    - know the resource's size in advance

.. note::
    You can find example of using ``anj_bytes_or_string_value_t`` in
    :doc:`basic object implementation tutorial
    <../BasicClient/BC-BasicObjectImplementation>`.

In such cases you can use the ``ANJ_DATA_TYPE_EXTERNAL_BYTES`` /
``ANJ_DATA_TYPE_EXTERNAL_STRING`` data types instead. They allow you to define
a callback that Anjay Lite calls to read successive chunks of the resource
value, so the value can be write in separate chunks directly to
internal Anjay Lite buffer.

.. note::
    This change is transparent to the LwM2M Server, although payload encoding
    may differ.
    Specifically, for CBOR-based content formats (CBOR, SenML CBOR, LwM2M CBOR,
    and SenML-ETCH CBOR) the server must support `Indefinite-Length Byte Strings
    and Indefinite-Length Text Strings.
    <https://www.rfc-editor.org/rfc/rfc8949.html#name-indefinite-length-byte-stri>`_

.. note::
    Code related to this tutorial can be found under
    `examples/tutorial/AT-ExternalDataTypes` in the Anjay Lite source directory
    and is based on `examples/tutorial/BC-MandatoryObjects` example.

Read a resource value from a file
---------------------------------

One practical use case for these data types is reading a resource value from a
file or from the microcontroller's external memory. In this example, you
implement the `BinaryAppDataContainer
<https://devtoolkit.openmobilealliance.org/OEditor/LWMOView?url=https%3a%2f%2fraw.githubusercontent.com%2fOpenMobileAlliance%2flwm2m-registry%2fprod%2f19.xml>`_
object with a `Opaque` Data resource. The value will be streamed from the
``examples/tutorial/AT-ExternalDataTypes/libanj.a`` file, which is generated
during the build process.

Enable external data support
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To use ``ANJ_DATA_TYPE_EXTERNAL_BYTES``, enable external data types in your
``CMakeLists.txt``:

.. highlight:: cmake
.. snippet-source:: examples/tutorial/AT-ExternalDataTypes/CMakeLists.txt
    :emphasize-lines: 9

    cmake_minimum_required(VERSION 3.16.0)

    project(anjay_lite_at_external_data_types C)

    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_EXTENSIONS OFF)

    set(ANJ_WITH_EXTRA_WARNINGS ON)
    set(ANJ_WITH_EXTERNAL_DATA ON)

Set up external data callbacks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To handle ``ANJ_DATA_TYPE_EXTERNAL_BYTES``, you need to define three callbacks:

- a function to read data from an external source,
- a function to open the external source before reading starts, and
- a function to close the source once reading ends or an error occurs.

**Read callback: anj_get_external_data_t**

The read callback must follow this interface:

.. highlight:: c
.. snippet-source:: include_public/anj/defs.h

    /**
    * Callback to read a chunk of external data.
    *
    * Called by the library when encoding a resource whose type is
    * @ref ANJ_DATA_TYPE_EXTERNAL_BYTES or @ref ANJ_DATA_TYPE_EXTERNAL_STRING.
    * It may be invoked multiple times until the entire resource value
    * has been streamed.
    *
    * The library guarantees sequential calls with monotonically increasing
    * @p offset and no overlaps.
    *
    * @param buffer      Output buffer to be filled with data.
    * @param[in,out] inout_size
    *                    - On input: size of @p buffer in bytes.
    *                    - On output: number of bytes actually written.
    * @param offset      Absolute offset (in bytes) from the beginning of
    *                    the resource value.
    * @param user_args   Application-defined pointer passed unchanged to
    *                    every callback.
    *
    * @return
    * - 0 if the end of the resource was reached (all data provided),
    * - a negative value on error,
    * - @ref ANJ_IO_NEED_NEXT_CALL if more data remains.
    *   In this case, the implementation must have filled the entire buffer
    *   (i.e., left @p inout_size unchanged).
    */
    typedef int anj_get_external_data_t(void *buffer,
                                        size_t *inout_size,
                                        size_t offset,
                                        void *user_args);

In this tutorial, the callback is:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ExternalDataTypes/src/main.c

    static int get_external_data(void *buffer,
                                size_t *inout_size,
                                size_t offset,
                                void *user_args) {
        struct external_data_user_args *args =
                (struct external_data_user_args *) user_args;
        size_t read_bytes = 0;

        while (*inout_size != read_bytes) {
            ssize_t ret_val =
                    pread(args->fd,
                        buffer,
                        *inout_size - read_bytes,
                        // We don't care about the off_t argument overflowing,
                        // because even if off_t were 32 bytes wide, an offset
                        // that large would still let us handle files bigger than
                        // the maximum file size that can be sent over CoAP
                        (off_t) (offset + read_bytes));

            if (ret_val == 0) {
                *inout_size = read_bytes;
                log(L_INFO, "The file has been completely read");
                return 0;
            } else if (ret_val < 0) {
                log(L_ERROR, "Error during reading from the file");
                return -1;
            }
            read_bytes += (size_t) ret_val;
        }
        return ANJ_IO_NEED_NEXT_CALL;
    }

Through the ``user_args`` argument we pass a pointer to the structure whose
definition is shown below:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ExternalDataTypes/src/main.c

    static struct external_data_user_args {
        int fd;
    } file_external_data_args = { -1 };

This structure only holds a file descriptor that all callbacks share.

The ``while`` loop is required because ``pread`` might read fewer bytes than
requested in its ``count`` parameter, and, as specified for
``anj_get_external_data_t``, the implementation must fill the buffer with
exactly the number of bytes indicated by ``inout_size`` if we intend to return
``ANJ_IO_NEED_NEXT_CALL``.

**Open callback: anj_open_external_data_t**

The second callback initializes the external data source before reading:

.. highlight:: c
.. snippet-source:: include_public/anj/defs.h

    /**
    * Callback to initialize the external data source.
    *
    * Invoked once before the first call to @ref anj_get_external_data_t.
    * Can be used to open files, initialize peripherals, or allocate state.
    *
    * @param user_args  Application-defined pointer.
    *
    * @return
    * - 0 on success,
    * - a negative value if initialization failed (in which case
    *   @ref anj_close_external_data_t will not be called).
    */
    typedef int anj_open_external_data_t(void *user_args);

In this example, the callback opens the file. The file path is defined by the
``FILE_PATH`` macro:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ExternalDataTypes/src/main.c

    static int open_external_data(void *user_args) {
        struct external_data_user_args *args =
                (struct external_data_user_args *) user_args;
        assert(args->fd == -1);

        args->fd = open(FILE_PATH, O_RDONLY | O_CLOEXEC);
        if (args->fd == -1) {
            log(L_ERROR, "Error during opening the file");
            return -1;
        }

        log(L_INFO, "File opened");
        return 0;
    }

**Close callback: anj_close_external_data_t**

The third callback handles de-initializing the external data source and is
called after all data have been read or when an error occurs:

.. highlight:: c
.. snippet-source:: include_public/anj/defs.h

    /**
    * Callback to clean up the external data source.
    *
    * Invoked after reading completes (successfully or with error),
    * unless @ref anj_open_external_data_t failed.
    * Can be used to close file descriptors, release memory, or reset state.
    *
    * @param user_args  Application-defined pointer.
    */
    typedef void anj_close_external_data_t(void *user_args);

In this example, the callback closes the file:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ExternalDataTypes/src/main.c

    static void close_external_data(void *user_args) {
        struct external_data_user_args *args =
                (struct external_data_user_args *) user_args;
        close(args->fd);
        args->fd = -1;
        log(L_INFO, "File closed");
    }

Assign callbacks in ``anj_res_value_t``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The addresses of the above callbacks, together with the
``external_data_user_args`` instance address, are assigned to the corresponding
pointers in the ``anj_res_value_t`` structure. It is done in the handler that
is called during the Read operation:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ExternalDataTypes/src/main.c
    :emphasize-lines: 11-13,15

    static int res_read(anj_t *anj,
                        const anj_dm_obj_t *obj,
                        anj_iid_t iid,
                        anj_rid_t rid,
                        anj_riid_t riid,
                        anj_res_value_t *out_value) {
        (void) anj;
        (void) obj;

        if (iid == 0 && rid == 0 && riid == 0) {
            out_value->external_data.get_external_data = get_external_data;
            out_value->external_data.open_external_data = open_external_data;
            out_value->external_data.close_external_data = close_external_data;

            out_value->external_data.user_args = (void *) &file_external_data_args;
            return 0;
        }

        return ANJ_DM_ERR_METHOD_NOT_ALLOWED;
    }

.. note::
    The ``anj_open_external_data_t`` and ``anj_close_external_data_t`` callbacks
    are optional. You can skip them if not needed.

Add an install function
^^^^^^^^^^^^^^^^^^^^^^^

Define and install the ``BinaryAppDataContainer`` object:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ExternalDataTypes/src/main.c

    static int install_binary_app_data_container_object(anj_t *anj) {
        static const anj_dm_handlers_t handlers = {
            .res_read = res_read,
        };

        // Definition of resource instance
        static const anj_riid_t insts[] = { 0 };

        // Definition of resource
        static const anj_dm_res_t res = {
            .rid = 0,
            .kind = ANJ_DM_RES_RM,
            .type = ANJ_DATA_TYPE_EXTERNAL_BYTES,
            .insts = insts,
            .max_inst_count = 1
        };

        // Definition of instance
        static const anj_dm_obj_inst_t obj_insts = {
            .iid = 0,
            .res_count = 1,
            .resources = &res
        };

        // Definition of object
        static const anj_dm_obj_t obj = {
            .oid = 19,
            .insts = &obj_insts,
            .handlers = &handlers,
            .max_inst_count = 1
        };

        return anj_dm_add_obj(anj, &obj);
    }

.. note::
    For more information on how to add an object in Anjay Lite, see
    :doc:`this <../BasicClient/BC-BasicObjectImplementation>` article.

Call the install function
^^^^^^^^^^^^^^^^^^^^^^^^^

Finally, call the install function from ``main``:

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-ExternalDataTypes/src/main.c
    :emphasize-lines: 4

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj)
            || install_server_obj(&anj, &server_obj)
            || install_binary_app_data_container_object(&anj)) {
        return -1;
    }

.. note::
    Large files may take a long time to transfer.
