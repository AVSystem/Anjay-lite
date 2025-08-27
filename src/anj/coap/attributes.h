/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef SRC_ANJ_COAP_ATTRIBUTES_H
#    define SRC_ANJ_COAP_ATTRIBUTES_H

#    include <stdbool.h>

#    include "coap.h"
#    include "options.h"

int anj_attr_notification_attr_decode(const anj_coap_options_t *opts,
                                      _anj_attr_notification_t *attr);
int anj_attr_discover_decode(const anj_coap_options_t *opts,
                             _anj_attr_discover_t *attr);
int anj_attr_register_prepare(anj_coap_options_t *opts,
                              const _anj_attr_register_t *attr);
int anj_attr_bootstrap_prepare(anj_coap_options_t *opts,
                               const _anj_attr_bootstrap_t *attr,
                               bool bootstrap_pack);

#endif // SRC_ANJ_COAP_ATTRIBUTES_H
