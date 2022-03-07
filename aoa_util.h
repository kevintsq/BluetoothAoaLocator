/***************************************************************************//**
 * @file
 * @brief AoA Utilities.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef AOA_UTIL_H
#define AOA_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include "aoa_types.h"
#include "sl_status.h"

#define ADR_LEN 6

void aoa_id_copy(aoa_id_t dst, aoa_id_t src);
int aoa_id_compare(aoa_id_t id1, aoa_id_t id2);

void aoa_address_to_id(uint8_t address[ADR_LEN], uint8_t address_type, aoa_id_t id);
sl_status_t aoa_id_to_address(aoa_id_t id, uint8_t address[ADR_LEN], uint8_t *address_type);
sl_status_t aoa_allowlist_find(uint8_t address[ADR_LEN]);
sl_status_t aoa_allowlist_add(uint8_t address[ADR_LEN]);
void aoa_allowlist_init(void);

/**************************************************************************//**
 * Compare two sequence numbers.
 *
 * @param[in] seq1 First sequence number.
 * @param[in] seq2 Second sequence number.
 *
 * @return The absolute distance of the sequence numbers.
 * @retval 0 The sequence numbers are equal.
 * @retval INT32_MAX Any of the sequence numbers are negative.
 *****************************************************************************/
int32_t aoa_sequence_compare(int32_t seq1, int32_t seq2);

/***************************************************************************//**
 * Find given service UUID in an Advertising or Scan Response packet.
 *
 * @param[in] adv_data Advertising or Scan Response data
 * @param[in] adv_len Length of the Advertising or Scan Response data
 * @param[in] uuid 16 or 128-bit Service Class UUID
 * @param[in] uuid_len Length of 16 or 128-bit Service Class UUID in bytes
 *
 * @retval true The service UUID is found
 * @retval false The service UUID is not found or invalid input parameters
 ******************************************************************************/
bool find_service_in_advertisement(uint8_t *adv_data,
                                   uint8_t adv_len,
                                   const uint8_t *uuid,
                                   size_t uuid_len);

#endif // AOA_UTIL_H
