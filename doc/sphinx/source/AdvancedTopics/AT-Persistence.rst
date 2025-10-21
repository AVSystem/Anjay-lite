..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Persistence support
===================

Overview
--------

**Persistence API** in Anjay Lite lets the client store and later restore the state
of LwM2M objects across application restarts. For instance, it allows for retaining
bootstrapped configuration of LwM2M Servers (which is held in Security and Server
Objects), allowing the device to bootstrap the configuration just once and reuse
that configuration on future startups of the application.

Persistence is optional and generic: the library exposes a callback-based
persistence context so that **you can use it for your own data** as well
(e.g., app-specific configuration saved to a file or flash).

.. important::

   Persistence API is not enabled by default. To use it, set the CMake option
   ``ANJ_WITH_PERSISTENCE``.

Built-in object persistence
---------------------------

Anjay Lite provides ready-to-use helpers for two pre-implemented objects:

- **Server Object**

  - ``anj_dm_server_obj_store();``
  - ``anj_dm_server_obj_restore();``

- **Security Object**

  - ``anj_dm_security_obj_store();``
  - ``anj_dm_security_obj_restore();``

These APIs use an application-defined persistence context to serialize and
deserialize the internal state of the objects.

Example
-------

This example demonstrates how to:

1. **Restore** Server and Security objects on startup if a persistence file exists.
2. **Store** both objects **after successful bootstrap**.
3. Use persistence API to store and restore custom application data
   (**endpoint name** in this case).

.. note::
   Code related to this tutorial can be found under `examples/tutorial/AT-Persistence`
   in the Anjay Lite source directory and is based on `examples/tutorial/AT-Bootstrap`
   example.

File-based persistence callbacks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This example defines simple read/write callbacks that use standard ``fread()``
and ``fwrite()`` functions to store persistence data in a file. These callbacks
are called for each individual item stored or restored by helpers in
implementations of built-in Security and Server objects.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Persistence/src/main.c

    static int persistence_read(void *ctx, void *buf, size_t size) {
        FILE *file = (FILE *) ctx;
        if (fread(buf, 1, size, file) != size) {
            return -1;
        }
        return 0;
    }

    static int persistence_write(void *ctx, const void *buf, size_t size) {
        FILE *file = (FILE *) ctx;
        if (fwrite(buf, 1, size, file) != size) {
            return -1;
        }
        return 0;
    }

Reading on startup
~~~~~~~~~~~~~~~~~~

At startup, initialize the objects and try to open the persistence file.
If the file exists, create a **restore** context and pass it to the restore
helpers. Then install the objects in ``anj`` using the install helpers.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Persistence/src/main.c

    anj_dm_security_obj_init(&security_obj);
    anj_dm_server_obj_init(&server_obj);
    FILE *file = fopen(PERSISTENCE_OBJS_FILE, "r");
    int res = -1;
    if (file) {
        anj_persistence_context_t ctx =
                anj_persistence_restore_context_create(persistence_read, file);
        res = restore_security_obj(&anj, &security_obj, &ctx);
        if (!res) {
            res = restore_server_obj(&server_obj, &ctx);
        }
        fclose(file);
        // if any of the restores failed, we assume the persistence file is
        // corrupted and should be removed
        if (res) {
            remove(PERSISTENCE_OBJS_FILE);
        }
    }

    if (install_device_obj(&anj, &device_obj)
            || install_security_obj(&anj, &security_obj, !!res)
            || install_server_obj(&anj, &server_obj)) {
        return -1;
    }

Restore helpers calls ``anj_dm_*_obj_restore`` library functions and wraps them
with informative logging.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Persistence/src/main.c

    static int restore_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj,
                                    const anj_persistence_context_t *ctx) {
        if (anj_dm_security_obj_restore(anj, security_obj, ctx)) {
            log(L_INFO, "Security object restore failed. Using default.");
            return -1;
        }
        log(L_INFO, "Security object restored");
        return 0;
    }

    static int restore_server_obj(anj_dm_server_obj_t *server_obj,
                                  const anj_persistence_context_t *ctx) {
        if (anj_dm_server_obj_restore(server_obj, ctx)) {
            log(L_ERROR, "Server object restore failed. Using default.");
            return -1;
        }
        log(L_INFO, "Server object restored");
        return 0;
    }

Install helpers initialize object instances with default values if needed.
In this example, default configuration connects with LwM2M Bootstrap Server, so
the Server Object instance is not needed in default configuration. Then they
call ``anj_dm_*_obj_install`` to register the objects in ``anj``.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Persistence/src/main.c

    static int install_security_obj(anj_t *anj,
                                    anj_dm_security_obj_t *security_obj,
                                    const bool need_default) {
        if (need_default) {
            anj_dm_security_obj_init(security_obj);
            anj_dm_security_instance_init_t security_inst = {
                .ssid = 1,
                .bootstrap_server = true,
                .server_uri = "coap://eu.iot.avsystem.cloud:5693",
                .security_mode = ANJ_DM_SECURITY_NOSEC,
            };
            if (anj_dm_security_obj_add_instance(security_obj, &security_inst)) {
                return -1;
            }
        }
        return anj_dm_security_obj_install(anj, security_obj);
    }

    static int install_server_obj(anj_t *anj, anj_dm_server_obj_t *server_obj) {
        return anj_dm_server_obj_install(anj, server_obj);
    }

Storing after successful bootstrap
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Persistence is handled inside the connection status callback. Once the client
reports ``ANJ_CONN_STATUS_BOOTSTRAPPED``, the current state of the Security and
Server objects is written to the persistence file. This ensures that the device
does not need to repeat the bootstrap procedure on the next startup.  

Pointers to the Security and Server objects are passed to the callback,
so the function can directly access them when creating the
persistence context and invoking the ``*_store()`` helpers. If any error occurs,
the file is immediately closed and removed to avoid leaving
a corrupted or partially written persistence file. On success, both objects are
safely stored.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Persistence/src/main.c

    typedef struct {
        anj_dm_server_obj_t *server_obj;
        anj_dm_security_obj_t *security_obj;
    } persistent_objects_t;

    static void connection_status_callback(void *arg,
                                        anj_t *anj,
                                        anj_conn_status_t conn_status) {
        if (conn_status == ANJ_CONN_STATUS_BOOTSTRAPPED) {
            log(L_INFO, "Bootstrap successful");
            FILE *file = fopen(PERSISTENCE_OBJS_FILE, "w+");
            if (!file) {
                log(L_ERROR, "Could not open persistence file for writing");
                return;
            }
            persistent_objects_t *callback_arg = (persistent_objects_t *) arg;
            anj_persistence_context_t persistence_ctx =
                    anj_persistence_store_context_create(persistence_write, file);
            if (anj_dm_security_obj_store(anj, callback_arg->security_obj,
                                        &persistence_ctx)
                    || anj_dm_server_obj_store(callback_arg->server_obj,
                                            &persistence_ctx)) {
                log(L_ERROR, "Could not store persistent objects");
                fclose(file);
                remove(PERSISTENCE_OBJS_FILE);
            } else {
                log(L_INFO, "Persistent objects stored");
                fclose(file);
            }
        }
    }

.. important::
   Because both objects are stored sequentially in the same file, ensure that
   the order of calls to ``*_store()`` matches the order of ``*_restore()``.
   Alternatively, you can store each object in a separate file.

Endpoint name persistence
~~~~~~~~~~~~~~~~~~~~~~~~~

This example also demonstrates how to use the persistence API to store user
data. The endpoint name is either provided as a command-line argument or
restored from a separate persistence file if no argument is given. If provided
as an argument, it is stored in the persistence file for future runs.

This functionality may be useful in scenarios where the endpoint name is
dynamically assigned (e.g., during manufacturing) and needs to be retained
across application restarts.

.. highlight:: c
.. snippet-source:: examples/tutorial/AT-Persistence/src/main.c

    char endpoint_name[128] = { 0 };
    // check if endpoint name was provided as argument
    // if not, try to restore it from persistence
    if (argc != 2) {
        FILE *ep_file = fopen(PERSISTENCE_ENDPOINT_FILE, "r");
        if (!ep_file) {
            log(L_ERROR, "No endpoint name given, and no persistence file "
                         "found to restore it from");
            return -1;
        }
        // restore endpoint name
        anj_persistence_context_t ctx_ep =
                anj_persistence_restore_context_create(persistence_read,
                                                       ep_file);
        if (anj_persistence_string(&ctx_ep, endpoint_name,
                                   sizeof(endpoint_name))) {
            log(L_ERROR, "Failed to restore endpoint name");
            fclose(ep_file);
            return -1;
        }
        log(L_INFO, "Endpoint name restored: %s", endpoint_name);
        fclose(ep_file);
    } else {
        strncpy(endpoint_name, argv[1], sizeof(endpoint_name) - 1);
        // endpoint name provided as an argument - store it in persistence
        FILE *ep_file = fopen(PERSISTENCE_ENDPOINT_FILE, "w+");
        if (!ep_file) {
            log(L_ERROR,
                "Could not open endpoint persistence file for writing");
            return -1;
        }
        anj_persistence_context_t ctx_ep =
                anj_persistence_store_context_create(persistence_write,
                                                     ep_file);
        if (anj_persistence_string(&ctx_ep, argv[1], 0)) {
            log(L_ERROR, "Failed to store endpoint name");
            fclose(ep_file);
            remove(PERSISTENCE_ENDPOINT_FILE);
            return -1;
        }
        log(L_INFO, "Endpoint name stored");
        fclose(ep_file);
    }

Considerations
--------------

When to persist
~~~~~~~~~~~~~~~

In this example the objects' state is saved only after a successful
bootstrap. However, objects may later be modified by the application or by the
LwM2M Management Server (for example, the **Lifetime** resource).
To preserve such changes across restarts, you must implement your own mechanism
to track and persist updates.

Security
~~~~~~~~

The example writes raw, unencrypted credentials (e.g., PSK identity/key)
into a plain file. The persistence layer does not provide confidentiality
or integrity. For stronger protection, store credentials in a secure element
such as an HSM. You can achieve this by implementing
a dedicated integration layer and compiling with ``ANJ_WITH_EXTERNAL_CRYPTO_STORAGE``
enabled. The interface for such integration is defined in
``include_public/anj/compat/crypto/storage.h``. In this setup, only a key
identifier (obtained using ``anj_crypto_storage_get_persistence_info()``)
is persisted, while the actual secret material remains securely inside the HSM.

.. note::
   Applications are also free to add **additional metadata verification and
   encryption** on top of the persistence streams if desired, as long as this
   does not affect what is seen by the library when calling the persistence
   callbacks.

Portability
~~~~~~~~~~~

The persistence binary format is **not portable** across architectures,
ABIs, compiler versions, or even different library configurations. For
example, some fields are only present if options such as ``ANJ_WITH_SECURITY``
are enabled. Moving files between devices, firmware builds, or applications
compiled with different library options may cause undefined behavior.

Integrity
~~~~~~~~~

There is **no built-in integrity check** (checksum/signature). If this is
required, add it on top of the persistence file or wrap the callbacks with
some checksum and signature verification mechanism.

Versioning
~~~~~~~~~~

Each object stores its own version number at the beginning of the persistence
data. When restoring, the object compares the stored version with the version
it currently supports and rejects the file if they do not match. The version
may change whenever the object's definition evolves — for example, when new
resources are added. This ensures that outdated persistence data will not be
accidentally misinterpreted.
