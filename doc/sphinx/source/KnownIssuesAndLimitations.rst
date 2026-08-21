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

Mbed TLS integration with asynchronous network backends
--------------------------------------------------------

The default Mbed TLS DTLS integration is not compatible with network backends
where ``anj_net_recv()`` may return ``ANJ_NET_EINPROGRESS`` after starting a
receive operation that remains active across multiple calls.

The Anjay Lite network API requires an operation that returns
``ANJ_NET_EINPROGRESS`` to be retried with the same parameters before another
network operation is performed. Mbed TLS exposes non-blocking BIO callbacks
through ``MBEDTLS_ERR_SSL_WANT_READ`` and ``MBEDTLS_ERR_SSL_WANT_WRITE``, but its
public API does not currently guarantee that an in-progress network operation
will be resumed before another network operation is attempted.

In particular, during a DTLS handshake, expiry of the retransmission timer may
cause Mbed TLS to attempt to send a retransmission while a receive operation is
still in progress.

See :doc:`/AdvancedTopics/AT-MbedTLSBestPractices` for additional details and configuration
considerations.

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

.. _notification-payload-may-not-reflect-threshold-crossing:

Notification payload may not reflect threshold crossing
-------------------------------------------------------

In certain cases, a notification may be triggered when the observed value
crosses a threshold (for example, configured using the **gt** attribute),
but the actual notification is not sent immediately. If the value goes back
below (or above) that threshold before the notification message is constructed,
the server will receive a notification containing the new value that itself would
not have caused the notification to be triggered. This applies to all value-based 
attributes: **lt**, **gt**, **st** and **edge**.

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

Non valid hostname may appear in SNI extension
----------------------------------------------

The DTLS Server Name Indication (SNI) extension is designed to communicate
the expected server hostname during a DTLS handshake, particularly when
it differs from the connection URI. According to `RFC6066`, the SNI extension
must contain a valid hostname, not an IP address.

In Anjay Lite, if the LwM2M Server URI (``/0/x/0``) is configured with a raw IP
address and the dedicated SNI field in the LwM2M Server Object (``/0/x/14``)
is left unset, the default MbedTLS integration layer falls back to the URI host
when populating SNI. As a result, the IP address may be sent in the SNI
extension, which is non-compliant with RFC6066.

.. _oscore_limitations:

OSCORE limitations
------------------

Anjay Lite has some limitations related to OSCORE support:
 - When using OSCORE Appendix B2, Context ID negotiation may collide with an
   ongoing block-wise transfer. If the server attempts to start B2 in the
   middle of such a transfer, Anjay Lite rejects the B2 procedure and
   terminates the current exchange. For client-initiated exchanges, this may
   result in fallback to re-registration, potentially followed by a new B2
   negotiation during Register. B2 is still allowed when it starts on the
   first block of the transfer, as this is required to support block-wise
   registration and other first-block exchanges.
 - On the first use of an OSCORE security context, the LwM2M specification
   requires using the Echo option and performing the procedure described in
   Appendix B.2. Performing this procedure derives a new security context, so
   using the Echo option after completing Appendix B.2 procedure seems
   redundant. Accordingly, Echo is not implemented in Anjay Lite.

Firmware Update cleanup after failed or timed-out Write
-------------------------------------------------------

The Firmware Update process is not properly terminated and cleaned up when a
FOTA Push transfer times out while a Write operation is in progress. A similar
issue occurs when a Write operation fails in either Push or Pull mode.

In these cases, the Firmware Update cleanup procedure is not executed
correctly, which may leave resources associated with the ongoing update
allocated and the Firmware Update process in an inconsistent state.
