..
    Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
    AVSystem Anjay Lite LwM2M SDK
    All rights reserved.

    Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
    See the attached LICENSE file for details.

.. _anjay-object-stub-generator:

Object Stub Generator
=====================

.. contents:: :local:

Overview
^^^^^^^^

Anjay Lite includes the ``./tools/anjay_codegen.py`` script to streamline the implementation of LwM2M objects.
This tool parses LwM2M Object Definition XML files and generates code skeletons, 
so you can focus on implementing object-specific logic instead of boilerplate code.

Key features:

* Download of LwM2M Object Definition XML files from the `OMA LwM2M Registry <https://www.openmobilealliance.org/specifications/registries/objects>`_.
* Generation of code stubs for objects.
* Support for static and dynamic multiple instance resources.
* Support for static and dynamic multiple instance objects.
* Generation of empty handlers for transactional operations.

Download Object Definition XML Files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Use the ``lwm2m_object_registry.py`` script to retrieve
LwM2M Object Definition XML files from the `OMA LwM2M Registry <https://www.openmobilealliance.org/specifications/registries/objects>`_.

With this tool, you can:

- List available objects in the registry 
- Download specific object definitions
- Use the XML files to generate object code stubs

.. important::
     If you do not specify a version, the script downloads the latest available version of the object.

.. code-block:: bash

    # List available LwM2M objects
    ./tools/lwm2m_object_registry.py --list

    # Download Object Definition XML for object 3 (Device) to device.xml
    ./tools/lwm2m_object_registry.py --get-xml 3 --output device.xml

    # Download a specific object version
    ./tools/lwm2m_object_registry.py --get-xml 3 --object-version 1.1 --output device.xml

Once you have the XML definition, you can generate the Object stub using the ``anjay_codegen.py`` script:

.. code-block:: bash

    ./tools/anjay_codegen.py -i device.xml

To adjust the stub to the object's instances count and resources set, see the description of additional options below.

Generate Object Stub Code
^^^^^^^^^^^^^^^^^^^^^^^^^

The stub generator creates a C source file that provides a skeleton implementation of your LwM2M object. The file includes ``TODO`` comments that indicate where to insert your custom logic.
To simplify the generated code, remove any unused resources and handlers.
Use the :ref:`-r flag<dropping-resources>` to include only the resources you need in the output.

After completing the implementation:

- Instantiate the object
- Register it in the Anjay Data Model

For instructions, see :ref:`Add the Object to Anjay-Lite<registering-objects>`.

To get a reference to your object in Anjay-Lite, use:

.. code-block:: c

    anj_dm_obj_t *some_object_name_object_create(void);

.. _single-instance-generator:

Single Instance Object
~~~~~~~~~~~~~~~~~~~~~~
The generator creates a single instance object by default, even if object is specified as multiple instance in the XML definition.

.. code-block:: bash

    ./tools/anjay_codegen.py -i temperature_obj.xml -o temperature_obj.c 

See `Basic Object Implementation <../BasicClient/BC-BasicObjectImplementation.html>`_ for more details.

.. _multi-instance-generator:

Multiple Instance Object (Static)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To generate an object with multiple instances, use the ``-ni <count>`` option to define the number of instances.
By default, the instances are static—this means the instance count is fixed at compile time.

.. code-block:: bash

    ./tools/anjay_codegen.py -i temperature_obj.xml -o temperature_obj.c -ni 5

.. important::
    If the object is defined as single-instance in the XML, ``-ni`` is ignored.

See `Multiple Instance Object Implementation <../AdvancedTopics/AT-MultiInstanceObjectStatic.html>`_ for more information.

.. _multi-dynamic-instance-generator:

Multiple Instance Object (Dynamic)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use the ``-di`` flag to generate code stubs that support dynamic instance management.
Combine it with the ``-ni <count>`` option to set the maximum number of instances.
The actual number of instances can vary at runtime.

.. code-block:: bash

    ./tools/anjay_codegen.py -i temperature_obj.xml -o temperature_obj.c -ni 5 -di

See `Dynamic Multiple Instance Object Implementation <../AdvancedTopics/AT-MultiInstanceObjectDynamic.html>`_ for details.

.. _multi-resource-instances-generator:

Multiple Resource Instances (Static)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, resource instances are generated statically.

To configure the number of instances for specific resources, use the ``-nri <rid> <count>`` option.
You can repeat the option to configure multiple resources.

.. code-block:: bash

    ./tools/anjay_codegen.py -i binary_app_data_container.xml -o binary_app_data_container.c -nri <rid1> <n> -nri <rid2> <m> <...>

- ``<rid1>`` and ``<rid2>`` are the Resource IDs
- ``<n>`` and ``<m>`` specify the number of instances for each resource.

See `Multiple Resource Instances Implementation <../AdvancedTopics/AT-MultiInstanceResourceStatic.html>`_ for more.

.. _multi-resource-instances-dynamic-generator:

Multiple Resource Instances (Dynamic)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To handle resource instances dynamically, use the ``-dri``. Combine it with the ``-nri <rid> <max>`` option to set the maximum
number of instances for each resource. This allows resources to have a variable number of instances at runtime.

.. code-block:: bash

    ./tools/anjay_codegen.py -i binary_app_data_container.xml -o binary_app_data_container.c -dri -nri <rid1> <n> -nri <rid2> <m> <...>

- ``<rid1>`` and ``<rid2>`` are the Resource IDs
- ``<n>`` and ``<m>`` specify the **maximum** number of instances for each resource.

See `Dynamic Multiple Resource Instances Implementation <../AdvancedTopics/AT-MultiInstanceResourceDynamic.html>`_ for more information.

Transactional Operations Handlers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Use the ``-t`` flag to generate empty transactional operation handlers, even for static objects.

.. code-block:: bash

    ./tools/anjay_codegen.py -i some_object.xml -o some_object.c -t

Handlers with basic logic to cache instance ID arrays are automatically generated only for dynamic instance objects and resources, regardless of the ``-t`` flag.

See :ref:`Transactional Operations<transactional_writes>` for more.

.. _instance_reset_handler:

Instance Reset Handler
~~~~~~~~~~~~~~~~~~~~~~

If the object includes writable resources or supports multiple instances, a reset handler is automatically generated to enable Write-Replace operations.

See :ref:`Reset Instance Context<reset-instance-context>` for details.

.. _dropping-resources:

Drop Unused Resources
^^^^^^^^^^^^^^^^^^^^^
To exclude unnecessary resources from the generated code, use the ``-r`` flag followed by the list of Resource IDs you want to include.

Only the specified resources will be generated—any others will be omitted.
Resources not defined in the XML file are ignored without warning.

For example, the following command generates only resources with IDs 2, 5, and 7:

.. code-block:: bash

    ./tools/anjay_codegen.py -i some_object.xml -o some_object.c -r 2 5 7

Additional Examples
^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

    # Generate object code stub from device.xml
    ./tools/anjay_codegen.py -i device.xml -o device.c

    # Generate code stub with 10 object instances and dynamic multi-instance resources
    ./tools/anjay_codegen.py -i device.xml -o device.c -ni 10 -dri

    # Download Object Definition XML for object 3 and generate code stub without an intermediate file
    ./tools/lwm2m_object_registry.py --get-xml 3 | ./tools/anjay_codegen.py -i - -o device.c

    # Maximum 5 dynamic object instances, with 20 resource instances for resource 5 and 10 for resource 2 (handled statically)
    ./tools/anjay_codegen.py -i some_object.xml -o some_object.c -ni 5 -dri -nri 2 10 -nri 5 20 
