/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#ifndef IPC_H_
#define IPC_H_

int ipc_init(const char *port_str);
int ipc_loop(bool &running);
int ipc_shutdown();

#endif // IPC_H_
