/*
 * Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#ifndef ANJ_SRC_CORE_SERVER_H
#    define ANJ_SRC_CORE_SERVER_H

#    include <stdbool.h>
#    include <stdint.h>
#    include <string.h>

#    include <anj/compat/net/anj_net_api.h>
#    include <anj/core.h>
#    include <anj/defs.h>

/**
 * Establishes a connection to the server. If @ref ANJ_NET_EAGAIN is returned,
 * this function must be called again with the same arguments.
 *
 * @param ctx              Server connection context.
 * @param type             Type of the network socket.
 * @param net_socket_cfg   Pointer to the network socket configuration, or NULL.
 * @param hostname         Hostname of the server to connect to.
 * @param port             Port of the server to connect to.
 * @param reconnect        Indicates if @ref anj_net_reuse_last_port should be
 *                         used.
 *
 * @return @ref ANJ_NET_OK or @ref ANJ_NET_EAGAIN on success, a negative value
 *         in case of an error.
 */
int _anj_server_connect(_anj_server_connection_ctx_t *ctx,
                        anj_net_binding_type_t type,
                        const anj_net_config_t *net_socket_cfg,
                        const char *hostname,
                        const char *port,
                        bool reconnect);

/**
 * Closes the connection to the server. If @ref ANJ_NET_EAGAIN is returned, this
 * function must be called again with the same arguments.
 *
 * @param ctx      Server connection context.
 * @param cleanup  Indicates if the context should be cleaned up.
 *
 * @return @ref ANJ_NET_OK or @ref ANJ_NET_EAGAIN on success, a negative value
 *         in case of an error.
 */
int _anj_server_close(_anj_server_connection_ctx_t *ctx, bool cleanup);

/**
 * Sends a message to the server. If this function returns error, connection
 * must be closed. If @ref ANJ_NET_EAGAIN is returned, this function must be
 * called again with the same arguments.
 *
 * @param ctx     Server connection context.
 * @param buffer  Pointer to the buffer containing the message to send.
 * @param length  Length of the message.
 *
 * @return @ref ANJ_NET_OK or @ref ANJ_NET_EAGAIN on success, a negative value
 *        in case of an error.
 */
int _anj_server_send(_anj_server_connection_ctx_t *ctx,
                     const uint8_t *buffer,
                     size_t length);

/**
 * Receives a message from the server. If this function returns error,
 * connection must be closed. @ref ANJ_NET_OK returned means that new message
 * was received.
 *
 * @param      ctx        Server connection context.
 * @param[out] buffer     Pointer to the buffer to store the received message.
 * @param[out] out_length Pointer to the length of the received message.
 * @param      length     Length of the buffer.
 *
 * @return @ref ANJ_NET_OK or @ref ANJ_NET_EAGAIN on success, a negative value
 *         in case of an error.
 */
int _anj_server_receive(_anj_server_connection_ctx_t *ctx,
                        uint8_t *buffer,
                        size_t *out_length,
                        size_t length);

/**
 * Calculates the maximum payload size that can be sent in a single message.
 * Calculation is based on given arguments and the MTU of the network
 * socket.
 *
 * @param      ctx                 Server connection context.
 * @param      msg                 Pointer to the CoAP message.
 * @param      payload_buff_size   Size of the payload buffer.
 * @param      out_msg_buffer_size Size of the message buffer.
 * @param      server_request      Indicates if the message is a server request.
 * @param[out] out_payload_size    Pointer to the calculated payload size.
 *
 * @return 0 on success, a negative value in case of an error.
 */
int _anj_server_calculate_max_payload_size(_anj_server_connection_ctx_t *ctx,
                                           _anj_coap_msg_t *msg,
                                           size_t payload_buff_size,
                                           size_t out_msg_buffer_size,
                                           bool server_request,
                                           size_t *out_payload_size);

/**
 * Handles the LwM2M request. If @ref ANJ_NET_EAGAIN is returned, this function
 * must be called again. If different value is returned, the exchange is
 * finished. Before the first use of this function, the exchange must be already
 * started with @ref _anj_exchange_new_client_request or @ref
 * _anj_exchange_new_server_request. This function is integration layer between
 * exchange API and network API.
 *
 * It immediately returns 0 if there is no ongoing exchange.
 *
 * IMPORTANT: Exchange success doesn't always mean success on operation level -
 * server may return error response.
 *
 * @param anj Anjay instance.
 *
 * @return 0 or @ref ANJ_NET_EAGAIN on success, a negative value
 *         in case of an error.
 */
int _anj_server_handle_request(anj_t *anj);

/**
 * Starts new LwM2M client request exchange. This function can't return @ref
 * ANJ_NET_EAGAIN.
 *
 * @param anj         Anjay instance.
 * @param new_request Pointer to the CoAP message.
 * @param handlers    Pointer to the exchange handlers.
 *
 * @return 0 on success, a negative value in case of an error.
 */
int _anj_server_prepare_client_request(anj_t *anj,
                                       _anj_coap_msg_t *new_request,
                                       _anj_exchange_handlers_t *handlers);

/**
 * Starts new LwM2M server request exchange. This function can't return @ref
 * ANJ_NET_EAGAIN.
 *
 * @param anj           Anjay instance.
 * @param request       Pointer to the CoAP message.
 * @param response_code Response code to be used in the response.
 * @param handlers      Pointer to the exchange handlers.
 *
 * @return 0 on success, a negative value in case of an error.
 */
int _anj_server_prepare_server_request(anj_t *anj,
                                       _anj_coap_msg_t *request,
                                       uint8_t response_code,
                                       _anj_exchange_handlers_t *handlers);

/**
 * Calculates MAX_TRANSMIT_WAIT based on the given CoAP transmission parameters.
 *
 * @param params  CoAP transmission parameters.
 *
 * @return MAX_TRANSMIT_WAIT value in milliseconds.
 */
uint64_t _anj_server_calculate_max_transmit_wait(
        const anj_exchange_udp_tx_params_t *params);

#endif // ANJ_SRC_CORE_SERVER_H
