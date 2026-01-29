..
   Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay Lite LwM2M SDK
   All rights reserved.

   Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
   See the attached LICENSE file for details.

Understanding Observations in LwM2M
===================================

Overview
--------

Observations allow LwM2M Clients to notify LwM2M Servers about changes
in resource values without waiting for the Server to poll for updates.
This mechanism is a part of the "Information Reporting Interface" defined
in the LwM2M Protocol Specification.

Except for the LwM2M Send operation, all operations within this interface
are centered around the observation mechanism defined in **RFC 7641**
(Observing Resources in the Constrained Application Protocol).
The LwM2M specification introduces its own model of operations and logic
related to observations, but it is still fundamentally built on top of
the same CoAP Observe mechanism.

The goal of this article is not to describe every aspect of Information Reporting,
but rather to focus on those parts that tend to be unclear in practice, are not
explicitly spelled out in the specification, or are implemented in Anjay Lite
in a way that may differ from what one would expect when reading only the LwM2M
standard and **RFC 7641**.

What is an Observation in LwM2M?
--------------------------------

At its core, an observation in LwM2M is a mechanism that allows the
LwM2M Server to *subscribe* to value of selected resources on the
Client. Instead of periodically polling the device for the current
value, the Server sends a request with the CoAP Observe option, and
from that point on the Client proactively sends notifications according
to the agreed criteria, expressed through observation attributes. These
criteria may be based on value changes (e.g., any change, thresholds),
on time (e.g., a minimum or maximum period, so that notifications can
be sent even if the underlying data does not change at all), or on a
combination of both. This makes communication more efficient and better
aligned with the constraints of typical IoT devices, where both
bandwidth and energy are limited.

In LwM2M, observations are always requested by the LwM2M Server. There
are two operations that may establish an observation: **Observe** and
**Observe-Composite**. The initial value of the observed resource(s) is
returned directly in the response to the Observe or Observe-Composite
request, so the Server does not need a separate Read operation to get
the starting state. Because multiple independent observations may be
active on the same LwM2M path at the same time, each observation is
identified by the CoAP token from the original request, and subsequent
notifications for that observation reuse the same token. Observations
can't be used when communicating with a Bootstrap Server.

Observe operation
^^^^^^^^^^^^^^^^^

The **Observe** operation allows the Server to create an observation on
a single LwM2M path. This path does not have to point directly to a
Resource - it may also refer to an Object Instance or even to a whole
Object. The path is encoded in the URI of the CoAP message carrying the
Observe request.

An Observe request may be rejected by the Client for various reasons.
The most common cases include attempts to observe a non-existent path
(``4.04 Not Found``) or attempts to observe a Resource that is not readable
(``4.05 Method Not Allowed``). 

.. note::
    Interestingly, nothing in the LwM2M specification explicitly forbids
    establishing an observation on an Object or Object Instance that currently
    has no readable Resources at all. In such a case, Anjay Lite accepts the
    Observe request responding with an empty payload, and may start producing
    notifications once readable Resources appear or because of time-based triggers.

Observe-Composite operation
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The **Observe-Composite** operation is the second way to establish an
observation. In this case, the set of observed LwM2M paths is carried
in the payload of the message. A single Observe-Composite request may
contain an arbitrary number of paths. If the trigger condition for
any single path is met, the resulting notification **must** include
current values for all paths that belong to that composite observation,
not only for the one that changed.

.. note::
    It is legal for a path included in an Observe-Composite request to not
    exist at the moment when the observation is created: it is explicitly
    exercised in the *Enabler Test Specification (Interoperability) for
    Lightweight M2M*, in test case *LightweightM2M-1.1-int-308*. However, whenever
    such a path does exist, it must not point to a Resource or Resource
    Instance that is not readable - otherwise the request is expected to
    fail.

.. note::
    The LwM2M specification does not explicitly state how to handle
    root paths ``/`` in Observe-Composite requests, but Anjay Lite
    treats them as invalid.

Attributes
----------

Attributes are parameters that control the behavior of observations. They define
conditions under which notifications are sent. Attributes may be set by the Server
using the **Write-Attributes** operation, and since LwM2M 1.2 they may also be
included in the initial Observe request to define the desired behavior from
the very beginning of the observation. In addition, the Server Object contains
Resources that provide default values for some of these attributes. In next part
of this document, we will describe how attributes from different sources interact with
each other.

The following attributes are defined in the LwM2M specification:

- **pmin** (Minimum Period): defines the minimum time interval between
  consecutive notifications. Even if the observed data changes more frequently,
  notifications will not be sent more often than this interval.
- **pmax** (Maximum Period): defines the maximum time interval between
  consecutive notifications. If no other trigger condition is met within this
  interval, a notification will be sent anyway to ensure the Server receives
  periodic updates. If **pmax** is set to ``0``, no periodic notifications are sent.
- **gt** (Greater Than): defines a threshold value. A notification is sent
  whenever the observed value crosses this threshold.
- **lt** (Less Than): defines a threshold value. A notification is sent
  whenever the observed value crosses this threshold.
- **st** (Step): defines a step value. A notification is sent whenever
  the observed value changes by at least this amount since the last notification.
- **epmin** (Evaluation Period Minimum): defines the minimum time interval
  between evaluations of the observation conditions.
- **epmax** (Evaluation Period Maximum): defines the maximum time interval
  between evaluations of the observation conditions.

Attributes available from LwM2M 1.2:

- **edge** (Edge): defines the falling edge and rising edge conditions for boolean
  Resources.
- **con** (Confirmable Notifications): defines whether notifications
  should be sent as confirmable or non-confirmable.
- **hqmax** (Maximum Historical Queue): defines how many entries of historical data
  must be stored for a given observation.

.. note::
    From the perspective of their usage, the **gt** and **lt** attributes
    are symmetric; the only constraints are that **lt < gt** and **lt + 2 * st < gt**.

.. note::
    The **gt**, **lt** and **st** attributes are only applicable to numeric Resources.
    The **edge** attribute is only applicable to boolean Resources. All of these
    attributes can only be set at the Resource or Resource Instance level.

.. important::
    The **epmin** and **epmax** attributes are accepted by Anjay Lite for
    compatibility with the LwM2M specification, but they are not used to
    control when observation conditions are evaluated. In Anjay Lite,
    value-based conditions (**gt**, **lt**, **st**, **edge**) are
    evaluated when the data model changes, typically when the application
    calls ``anj_core_data_model_changed()``. Periodic conditions related
    to **pmin** and **pmax** are handled separately in the main event loop
    inside ``anj_core_step()``. The same applies to the **hqmax** attribute
    - Anjay Lite does not store notification when the Client is offline.

Write-Attributes operation
^^^^^^^^^^^^^^^^^^^^^^^^^^

The **Write-Attributes** operation allows the Server to set or modify
observation attributes for a given LwM2M path. The operation will fail
if the path does not exist or the given attributes are not legal (e.g.
setting **gt** on a non-numeric Resource or **pmin < 0**).

When attributes are written to a path that is already being observed,
the new attributes take effect immediately, potentially triggering
notifications if the new conditions are met.

.. note::
    The LwM2M specification does not explicitly define the expected behavior
    when attributes are written to a path that already has set attributes. In
    Anjay Lite, the new attributes override the existing ones. Attributes not
    included in the Write-Attributes request remain unchanged. LwM2M Server
    can also remove specific attributes by sending a Write-Attributes request
    with those attributes set to empty values.

.. note::
    The **Write-Attributes** operation is not part of the "Information
    Reporting Interface" defined in the LwM2M specification. However, it is
    closely related to observations, as it directly influences their behavior.

Attributes in Observe operation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

From LwM2M 1.2 onwards, it is possible to include attributes directly
in the initial Observe or Observe-Composite request. This allows
the Server to define the desired observation behavior from the very
beginning, without needing a separate Write-Attributes operation.

If attributes are included in the Observe request, they override
attributes set via Write-Attributes, but only for that specific observation,
not for the entire LwM2M path. This means that other observations on the same
path will still use the attributes set via Write-Attributes:

.. code-block:: text

  When these attributes are present, all the attached attributes of the
  targeted URI MUST be ignored for this Observation.

  -- OMA-TS-LightweightM2M_Core-V1_2_2-20240613-A, section 6.4.1

Default attributes from the Server Object are still applied in this case.

Default attributes from the Server Object
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The Server Object contains Resources that provide default values for
some observation attributes. These defaults are used when no other
values have been set via Write-Attributes or included in the Observe
request:

- **Default Minimum Period** (``/1/x/5``): default value for **pmin** attribute.
- **Default Maximum Period** (``/1/x/6``): default value for **pmax** attribute.
- **Default Notification Mode** (``/1/x/26``): default value for **con** attribute.

Effective attribute values
^^^^^^^^^^^^^^^^^^^^^^^^^^

When determining the effective values of observation attributes for
a given observation, Anjay Lite follows a specific precedence order:

1. Attributes included in the initial Observe or Observe-Composite request.
   If present, attributes from Write-Attributes operations are ignored.
2. Attributes set via Write-Attributes operation for the specific LwM2M path.
3. Attributes set via Write-Attributes operation for parent paths.
4. Default attributes from the Server Object.

For example, if **pmin** is set on the Object Instance level, all observations
on Resources within that Object will inherit that **pmin** value,
unless it is overridden at the Resource level.

.. note::
    The **con** attribute has a special behavior in case of composite observations -
    if any path in the composite observation has **con** set to true, all notifications
    for that observation will be sent as confirmable: `Notification MUST be sent over
    confirmable transport if at least one of the Notification components has con=1`
    (`OMA-TS-LightweightM2M_Core-V1_2_2-20240613-A, section 7.3.2`).

.. note::
   The LwM2M specification does not explicitly define how value-related
   attributes (**gt**, **lt**, **st** and **edge**) set at the Resource
   level should be handled for multi-instance Resources.

   In Anjay Lite there is an explicit distinction between attributes
   coming from Observe and those coming from Write-Attributes:

   * Value-related attributes included in an Observe or Observe-Composite
     request and targeting a multi-instance Resource are ignored.
   * Value-related attributes set via the Write-Attributes operation at
     the Resource level are inherited by observations that are attached
     at the Resource Instance level. In other words, observations on
     individual instances can use value-related attributes configured
     on the parent Resource.
   * If an observation exists only at the Resource level (and not on a
     specific Resource Instance), value-related attributes configured on
     that multi-instance Resource are not effective in Anjay Lite.

.. note::
    In Anjay Lite, if after calculating the effective attributes for a given observation,
    they form an incorrect set (e.g. **pmin > pmax**), the observation is not deleted
    but marked as inactive until the next change of attributes.

Notifications
-------------

A notification is a message sent by the LwM2M Client that contains the
current value of the observed path. The decision to send a notification is
based on the conditions defined by the associated attributes. A notification
must use the same token as the corresponding Observe request it is related
to. Each notification also includes the **Observe** option with an
incremented value, which allows the Server to determine the order in which
notifications were sent. If a notification does not fit into a single
message, the resource value is delivered using block-wise transfer.

.. note::
    As required by standard, Anjay Lite sends a notification as
    confirmable if no confirmable notifications have been sent in the
    last 24 hours:
    `A server that transmits notifications mostly in non-confirmable
    messages MUST send a notification in a confirmable message instead of
    a non-confirmable message at least every 24 hours` (`RFC 7641, section 4.5`).


Observations lifetime
---------------------

Observation remains active until it is explicitly canceled by the LwM2M Server using:

- **Cancel Observe** for observations created with the Observe operation.
- **Cancel Observe-Composite** for observations created with the
  Observe-Composite operation.
- **CoAP RST** message in response to a confirmable or non-confirmable notification.

Observations may also be removed by the LwM2M Client in certain situations.
Below are snippets from the specification that talk about deleting existing observations:

.. code-block:: text

  The LwM2M Server MUST re-initiate the desired observation requests whenever the LwM2M
  Client registers.

  -- OMA-TS-LightweightM2M_Core-V1_2_2-20240613-A, section 6.4.1

.. code-block:: text

  When a LwM2M Client deregisters, the LwM2M Server should assume past states are nullified
  including the previous observations.

  -- OMA-TS-LightweightM2M_Core-V1_2_2-20240613-A, section 6.4.1

.. code-block:: text

  When the client deregisters, rejects a notification, or the transmission of a
  notification times out after several transmission attempts, the client is considered
  no longer interested in the resource and is removed by the server from the list of observers.

  -- RFC 7641, section 1.2

According to the provided snippets, observations in Anjay Lite are deleted before the
registration process begins. For observations/composite-observations, if sending a notification
fails, the corresponding observation is deleted. An observation is also removed when any
internal error occurs while handling a notification, or when the related data model path
is deleted. However, as noted in :ref:`Known Issues <silent-observations-drop-when-observed-path-disappears>`,
no final notification is sent in such a case.

.. important::
    By default, Anjay Lite does **not** remove an Observation when the
    transmission of a Confirmable Notify message times out. This means that the
    Observation remains active and future notifications may still be attempted.

    To change this behavior and have Anjay Lite cancel the Observation
    upon a notification timeout, enable the ``ANJ_OBSERVE_OBSERVATION_CANCEL_ON_TIMEOUT``
    configuration option. It aligns with `RFC 7641`, but may cause Observations to be
    removed unexpectedly under poor network conditions.

Attributes lifetime
-------------------

Attributes set via the Write-Attributes operation remain in effect until
they are explicitly modified or removed by another Write-Attributes
operation. They are not cleared when the observation associated with the
given path is deleted.

The registration process does not affect attributes in any way. The LwM2M
Server can check them using the Discover operation. However, Anjay Lite does
not persist attributes across Client reboots.

Application use cases
----------------------

Observations are most useful when we care about changes in specific Resources
rather than periodic reporting of all measurements. A classic example is the
Firmware Update (FOTA) object, where the Server observes the State and Update
Result Resources to be notified whenever the firmware lifecycle progresses
(e.g., from downloading to updating to updated) or when an error occurs.

Composite Observe can be used in more complex scenarios, for example when
we monitor several Resources at once and want to know their values at the
moment some alert condition is triggered (such as a fault flag or an alarm
Resource changing state) - the Server then receives a consistent snapshot
of multiple values in a single notification.

.. important::
    Observations are not always the best fit for all use cases. For regular,
    periodic reporting of measurement data (e.g., time series of sensor values),
    a better fit is usually the LwM2M Send operation, which allows the Client to
    send compact batches of records with precise timestamps and is designed
    specifically for efficient transfer of measurement data, rather than event-driven
    notifications.

.. warning::
    It is a :ref:`known issue <notification-payload-may-not-reflect-threshold-breach>`
    that in certain cases, a notification may be triggered, but the actual
    notification payload does not reflect the threshold breach that caused
    the notification to be sent.
