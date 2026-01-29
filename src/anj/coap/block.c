/*
 * Copyright 2023-2026 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay Lite LwM2M SDK
 * All rights reserved.
 *
 * Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
 * See the attached LICENSE file for details.
 */

#include <anj/init.h>

#define ANJ_LOG_SOURCE_FILE_ID 2

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "block.h"
#include "coap.h"
#include "options.h"

#define _ANJ_BLOCK_OPTION_MAX_SIZE 3

#define _ANJ_BLOCK_OPTION_M_MASK 0x08
#define _ANJ_BLOCK_OPTION_M_SHIFT 3
#define _ANJ_BLOCK_OPTION_SZX_MASK 0x07
#define _ANJ_BLOCK_OPTION_SZX_CALC_CONST 3
#define _ANJ_BLOCK_OPTION_NUM_MASK 0xF0
#define _ANJ_BLOCK_OPTION_NUM_SHIFT 4
#define _ANJ_BLOCK_OPTION_BYTE_SHIFT 8

#define _ANJ_BLOCK_1_BYTE_NUM_MAX_VALUE 15
#define _ANJ_BLOCK_2_BYTE_NUM_MAX_VALUE 4095
#define _ANJ_BLOCK_NUM_MAX_VALUE 0x000FFFFF

int _anj_block_decode(anj_coap_options_t *opts, _anj_block_t *block) {
    bool check_block2_opt = false;
    uint8_t block_buff[_ANJ_BLOCK_OPTION_MAX_SIZE];
    size_t block_option_size = 0;

    memset(block, 0, sizeof(_anj_block_t));

    int res = _anj_coap_options_get_data_iterate(opts, _ANJ_COAP_OPTION_BLOCK1,
                                                 NULL, &block_option_size,
                                                 block_buff,
                                                 _ANJ_BLOCK_OPTION_MAX_SIZE);
    if (res == _ANJ_COAP_OPTION_MISSING) {
        res = _anj_coap_options_get_data_iterate(opts, _ANJ_COAP_OPTION_BLOCK2,
                                                 NULL, &block_option_size,
                                                 block_buff,
                                                 _ANJ_BLOCK_OPTION_MAX_SIZE);
        check_block2_opt = true;
    }
    if (res == _ANJ_COAP_OPTION_MISSING) {
        return 0;
    } else if (res) {
        return res;
    } else if (!block_option_size) {
        // dont't allow empty block option
        return _ANJ_ERR_MALFORMED_MESSAGE;
    } else if (!res && block_option_size <= _ANJ_BLOCK_OPTION_MAX_SIZE) {
        block->block_type =
                check_block2_opt ? ANJ_OPTION_BLOCK_2 : ANJ_OPTION_BLOCK_1;
        block->more_flag = !!(block_buff[block_option_size - 1]
                              & _ANJ_BLOCK_OPTION_M_MASK);

        uint8_t SZX = (block_buff[block_option_size - 1]
                       & _ANJ_BLOCK_OPTION_SZX_MASK);

        // block size = 2**(SZX + 4)
        block->size =
                (uint16_t) (2U << (SZX + _ANJ_BLOCK_OPTION_SZX_CALC_CONST));

        if (block_option_size == 1) {
            block->number =
                    (uint32_t) (block_buff[0] & _ANJ_BLOCK_OPTION_NUM_MASK)
                    >> _ANJ_BLOCK_OPTION_NUM_SHIFT;
        } else if (block_option_size == 2) {
            // network big-endian order
            block->number =
                    (uint32_t) ((block_buff[0] << _ANJ_BLOCK_OPTION_NUM_SHIFT)
                                + ((block_buff[1] & _ANJ_BLOCK_OPTION_NUM_MASK)
                                   >> _ANJ_BLOCK_OPTION_NUM_SHIFT));
        } else {
            block->number =
                    (uint32_t) ((block_buff[0]
                                 << (_ANJ_BLOCK_OPTION_BYTE_SHIFT
                                     + _ANJ_BLOCK_OPTION_NUM_SHIFT))
                                + (block_buff[1] << _ANJ_BLOCK_OPTION_NUM_SHIFT)
                                + ((block_buff[2] & _ANJ_BLOCK_OPTION_NUM_MASK)
                                   >> _ANJ_BLOCK_OPTION_NUM_SHIFT));
        }

        return 0;
    } else {
        return _ANJ_ERR_MALFORMED_MESSAGE;
    }
}

int _anj_block_prepare(anj_coap_options_t *opts, const _anj_block_t *block) {
    uint16_t opt_number;

    if (block->block_type == ANJ_OPTION_BLOCK_1) {
        opt_number = _ANJ_COAP_OPTION_BLOCK1;
    } else if (block->block_type == ANJ_OPTION_BLOCK_2) {
        opt_number = _ANJ_COAP_OPTION_BLOCK2;
    } else {
        return _ANJ_ERR_INPUT_ARG;
    }

    // prepare SZX parameter
    uint8_t SZX = 0xFF;
    const uint16_t allowed_block_sized_values[] = { 16,  32,  64,  128,
                                                    256, 512, 1024 };
    for (uint8_t i = 0; i < sizeof(allowed_block_sized_values); i++) {
        if (block->size == allowed_block_sized_values[i]) {
            SZX = i;
            break;
        }
    }
    if (SZX == 0xFF) {
        // incorrect block_size_option
        return _ANJ_ERR_INPUT_ARG;
    }

    // determine block option size
    size_t block_opt_size = 1;
    if (block->number > _ANJ_BLOCK_1_BYTE_NUM_MAX_VALUE) {
        block_opt_size++;
    }
    if (block->number > _ANJ_BLOCK_2_BYTE_NUM_MAX_VALUE) {
        block_opt_size++;
    }
    if (block->number > _ANJ_BLOCK_NUM_MAX_VALUE) {
        // block number out of the range
        return _ANJ_ERR_INPUT_ARG;
    }

    uint8_t buff[_ANJ_BLOCK_OPTION_MAX_SIZE];

    buff[block_opt_size - 1] =
            (uint8_t) ((((uint8_t) block->more_flag
                         << _ANJ_BLOCK_OPTION_M_SHIFT)
                        & _ANJ_BLOCK_OPTION_M_MASK)
                       + SZX
                       + ((block->number << _ANJ_BLOCK_OPTION_NUM_SHIFT)
                          & _ANJ_BLOCK_OPTION_NUM_MASK));
    if (block_opt_size == 2) {
        buff[0] = (uint8_t) (block->number >> _ANJ_BLOCK_OPTION_NUM_SHIFT);
    } else if (block_opt_size == 3) {
        buff[1] = (uint8_t) (block->number >> _ANJ_BLOCK_OPTION_NUM_SHIFT);
        buff[0] = (uint8_t) (block->number >> (_ANJ_BLOCK_OPTION_NUM_SHIFT
                                               + _ANJ_BLOCK_OPTION_BYTE_SHIFT));
    }

    return _anj_coap_options_add_data(opts, opt_number, buff, block_opt_size);
}
