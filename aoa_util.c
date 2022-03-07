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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "aoa_util.h"

// Adjust maximal allowlist size if needed.
#define MAX_ALLOWLIST_SIZE 73
static uint8_t allowlist[MAX_ALLOWLIST_SIZE][ADR_LEN];
static uint32_t allowlist_size;
static sl_status_t aoa_address_compare(uint8_t address1[ADR_LEN], uint8_t address2[ADR_LEN]);

void aoa_id_copy(aoa_id_t dst, aoa_id_t src)
{
  strncpy(dst, src, AOA_ID_MAX_SIZE);
  dst[AOA_ID_MAX_SIZE - 1] = 0;
}

int aoa_id_compare(aoa_id_t id1, aoa_id_t id2)
{
  return strncasecmp(id1, id2, AOA_ID_MAX_SIZE);
}

static sl_status_t aoa_address_compare(uint8_t address1[ADR_LEN], uint8_t address2[ADR_LEN])
{
  sl_status_t ret_val = SL_STATUS_OK;
  int i;

  for (i = 0; i < ADR_LEN; i++) {
    if (address1[i] != address2[i]) {
      ret_val = SL_STATUS_NOT_FOUND;
      break;
    }
  }

  return ret_val;
}

void aoa_address_to_id(uint8_t address[ADR_LEN], uint8_t address_type, aoa_id_t id)
{
  snprintf(id, AOA_ID_MAX_SIZE, "ble-%s-%02X%02X%02X%02X%02X%02X",
           address_type ? "sr" : "pd",
           address[5],
           address[4],
           address[3],
           address[2],
           address[1],
           address[0]);
}

sl_status_t aoa_id_to_address(aoa_id_t id, uint8_t address[ADR_LEN], uint8_t *address_type)
{
  sl_status_t ret;
  int retval, i;
  unsigned int address_cache[ADR_LEN];
  char *token;
  const char delimiter[2] = "-";
  char id_cache[AOA_ID_MAX_SIZE];
  char *saveptr;

  strncpy(id_cache, id, AOA_ID_MAX_SIZE);

  ret = SL_STATUS_OK;

  for (i = 0; i < ADR_LEN; i++) {
    address_cache[i] = 0;
  }

  do {
    token = strtok_r(id_cache, delimiter, &saveptr);
    // Look for the "ble" preffix.
    // If there is no BLE string found in the beginning, abort.
    if (token == NULL || strcasecmp("ble", token) != 0) {
      ret = SL_STATUS_NOT_FOUND;
      break;
    }
    // Parse address type.
    token = strtok_r(NULL, delimiter, &saveptr);
    if (token != NULL) {
      if (strcasecmp("sr", token) == 0) {
        *address_type = 1;
      } else {
        *address_type = 0;
      }
    } else {
      ret = SL_STATUS_NOT_FOUND;
      break;
    }

    // Parse address.
    token = strtok_r(NULL, delimiter, &saveptr);
    if (token != NULL) {
      retval = sscanf(token, "%2X%2X%2X%2X%2X%2X",
                      &address_cache[5],
                      &address_cache[4],
                      &address_cache[3],
                      &address_cache[2],
                      &address_cache[1],
                      &address_cache[0]);
      if (retval == ADR_LEN) {
        for (i = 0; i < ADR_LEN; i++) {
          address[i] = address_cache[i];
        }
      } else {
        ret = SL_STATUS_NOT_FOUND;
        break;
      }
    } else {
      ret = SL_STATUS_NOT_FOUND;
      break;
    }
  } while (0);

  return ret;
}

void aoa_allowlist_init(void)
{
  allowlist_size = 0;
}

sl_status_t aoa_allowlist_add(uint8_t address[ADR_LEN])
{
  sl_status_t ret_val;
  sl_status_t ret;
  int i;

  ret = aoa_allowlist_find(address);

  if (ret == SL_STATUS_OK) {
    ret_val = SL_STATUS_ALREADY_EXISTS;
  } else if (allowlist_size < MAX_ALLOWLIST_SIZE) {
    for (i = 0; i < ADR_LEN; i++) {
      allowlist[allowlist_size][i] = address[i];
    }
    ret_val = SL_STATUS_OK;
    allowlist_size++;
  } else {
    ret_val = SL_STATUS_FULL;
  }

  return ret_val;
}

sl_status_t aoa_allowlist_find(uint8_t address[ADR_LEN])
{
  sl_status_t ret_val;
  uint32_t i;

  ret_val = SL_STATUS_NOT_FOUND;

  for (i = 0; i < allowlist_size; i++) {
    if (aoa_address_compare(allowlist[i], address) == SL_STATUS_OK) {
      ret_val = SL_STATUS_OK;
      break;
    }
  }

  // This is the case when there is no allowlisting.
  if (allowlist_size == 0) {
    ret_val = SL_STATUS_EMPTY;
  }

  return ret_val;
}

/**************************************************************************//**
 * Compare two sequence numbers.
 *****************************************************************************/
int32_t aoa_sequence_compare(int32_t seq1, int32_t seq2)
{
  int32_t diff;

  if ((seq1 < 0) || (seq2 < 0)) {
    // Negative sequence numbers are considered as invalid.
    return INT32_MAX;
  }

  diff = abs(seq2 - seq1);
  return (diff < ((UINT16_MAX + 1) / 2)) ? diff : UINT16_MAX + 1 - diff;
}

/***************************************************************************//**
 * Find given service UUID in an Advertising or Scan Response packet.
 ******************************************************************************/
bool find_service_in_advertisement(uint8_t *adv_data,
                                   uint8_t adv_len,
                                   const uint8_t *uuid,
                                   size_t uuid_len)
{
  uint8_t ad_field_length;
  uint8_t ad_field_type;
  uint8_t incomplete_list;
  uint8_t complete_list;
  uint8_t *ad_uuid_field;
  uint32_t i = 0;
  uint32_t next_ad_structure;
  bool ret = false;

  if ((uuid_len == 2) || (uuid_len == 16)) {
    // Incomplete List of 16 or 128-bit  Service Class UUIDs.
    incomplete_list = (uuid_len == 2) ? 0x02 : 0x06;
    // Complete List of 16 or 128-bit  Service Class UUIDs.
    complete_list = (uuid_len == 2) ? 0x03 : 0x07;

    // Parse advertisement packet.
    while (i < adv_len) {
      ad_field_length = adv_data[i];
      ad_field_type = adv_data[i + 1];
      next_ad_structure = i + ad_field_length + 1;
      // Find AD types of interest.
      if (ad_field_type == incomplete_list || ad_field_type == complete_list) {
        // Compare each UUID to the service UUID to be found.
        for (ad_uuid_field = adv_data + i + 2;
             ad_uuid_field < adv_data + next_ad_structure;
             ad_uuid_field += uuid_len) {
          if (memcmp(ad_uuid_field, uuid, uuid_len) == 0) {
            ret = true;
            break;
          }
        }
        if (ret == true) {
          break;
        }
      }
      // Advance to the next AD structure.
      i = next_ad_structure;
    }
  }
  return ret;
}
