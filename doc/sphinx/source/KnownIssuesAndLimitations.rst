..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Known Issues and Limitations
============================

There are a few known issues and edge cases that have been identified during development but
have not yet been addressed. In addition, some features were deliberately not implemented due to
their limited usefulness in the LwM2M context and the priority placed on keeping Anjay Lite's
footprint small.

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

.. _notification-payload-may-not-reflect-threshold-breach:

Notification payload may not reflect threshold breach
-----------------------------------------------------

In certain cases, a notification may be triggered when the observed value
crosses a threshold (for example, configured using the **gt** attribute),
but the actual notification is not sent immediately. If the value later
returns below (or above) that threshold before the notification message is
constructed, the server will receive a notification containing a value that
itself would not have caused the notification to be triggered. This applies
to all value-based attributes: **lt**, **gt**, **st** and **edge**.

.. _default-value-of-multi-instance-resource:

Default value of Multi-Instance Resource
----------------------------------------

Write-Replace semantics in LwM2M replace the entire array of instances of a Multiple-Instance Resource.
Anjay Lite uses the ``inst_reset`` handler that clears all Resource Instances before applying the new set.
As a result, any “default” instances or values not present in the Write-Replace payload are removed.

To retain default values, include them in the Write-Replace payload or use
**Partial Update** or **Write-Composite** instead.

.. _silent-observations-drop-when-observed-path-disappears:

Silent Observations drop when observed path disappears
------------------------------------------------------

When ``anj_observe_data_model_changed()`` is called with ``ANJ_OBSERVE_CHANGE_TYPE_DELETED``,
the observation is dropped without sending a final Notify with ``4.04 Not Found``.
According to `RFC 7641`, when a resource becomes unavailable,
the notifier SHOULD send a ``4.04`` notification and MUST remove the observer entry.
Anjay Lite removes the observation but currently skips sending the final ``4.04`` notification.
Similarly, we do not inform the server about removing observations for other reasons (e.g.,
errors when building notifications).

Block-wise limitations
----------------------

Anjay Lite does not support:
 - Early Block2 size negotiation for requests with Block1 option - in LwM2M terms, this means that the
   LwM2M Server cannot force Anjay Lite to change the block size of its responses when the request
   itself is block-wise. This can occur only with Read-Composite and Observe-Composite operations.
   Anjay Lite supports block-wise transfer in both directions, but only with the same block sizes.
 - Late Block2 size negotiation - it is not possible for the LwM2M Server to change the Block2 size of
   Anjay Lite responses in the middle of the transfer.
 - Block1 size negotiation - it is not possible for the LwM2M Server to change the Block1 size of Anjay
   Lite requests.
 - Handling block-wise server requests where Block2 starts from a block other than NUM=0 - the LwM2M
   Server cannot force Anjay Lite to start sending the response from a non-zero block offset or resume a
   block-wise response from the middle of the transfer.
