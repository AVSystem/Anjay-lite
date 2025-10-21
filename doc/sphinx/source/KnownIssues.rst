..
   Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Known Issues
============

Multi-Instance Bootstrap Write is not allowed
---------------------------------------------

Bootstrap-Write requests that target only an Object and include multiple
Object Instances in one payload aren't supported.
Use separate Bootstrap-Write requests for each instance instead.
LwM2M 1.2 explicitly allows object-level Bootstrap-Write with multiple instances in one message.

Notification storing when offline
---------------------------------

The ``Notification Storing When Disabled or Offline`` Resource (``/1/x/6``)
is not supported.
When values change while the client is offline or the server account is
disabled, notifications are not queued and are lost. Changes become reportable
again only after the client is online/enabled.

LwM2M 1.2 defines the ``/1/x/6`` Resource (default: ``true``) and the ``hqmax``
attribute for historical queuing, but neither has any effect in this client.

Observations not removed after CoAP RST message
-----------------------------------------------

If non-confirmable notification receives a CoAP RST, we currently do not cancel
the related observation. This is not compliant with `RFC 7641`: a Reset to a
notification is a passive cancel, and the notifier should remove the observation.
Some LwM2M Servers rely on this behavior. Use Cancel Observation operation instead.

.. _default-value-of-multi-instance-resource:

Default value of Multi-Instance Resource
----------------------------------------

Write-Replace semantics in LwM2M replace the entire array of instances of a Multiple-Instance Resource.
Anjay Lite uses the ``inst_reset`` handler that clears all Resource Instances before applying the new set.
As a result, any “default” instances or values not present in the Write-Replace payload are removed.

To retain default values, include them in the Write-Replace payload or use
**Partial Update** or **Write-Composite** instead.

Silent Observations drop when observed path disappears
------------------------------------------------------

When ``anj_observe_data_model_changed()`` is called with ``ANJ_OBSERVE_CHANGE_TYPE_DELETED``,
the observation is dropped without sending a final Notify with ``4.04 Not Found``.
According to `RFC 7641`, when a resource becomes unavailable,
the notifier SHOULD send a ``4.04`` notification and MUST remove the observer entry.
Anjay Lite removes the observation but currently skips sending the final ``4.04`` notification.

Write operation may fail when CoAP header contains a path to a Resource Instance
--------------------------------------------------------------------------------

Write requests that include a path to a specific Resource Instance in the CoAP header may fail.
If the targeted Resource Instance does not exist, the client responds with 4.04 Not Found instead of creating it.
When the payload is encoded in TLV format, the Write operation may also fail even if the Resource Instance already exists.

As a workaround, provide the path to the Resource or to the Object Instance in the CoAP header instead of a full Resource Instance path.
