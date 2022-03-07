/***************************************************************************//**
 * @file
 * @brief AoA configuration parser.
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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "aoa_util.h"
#include "aoa_parse.h"

// Helper macro.
#define CHECK_TYPE(x, t)  if (((x) == NULL) || ((x)->type != (t))) return SL_STATUS_FAIL

// Module internal variables.
static cJSON *root = NULL;
static int locator_index;
static int allowlist_index;

/**************************************************************************//**
 * Load file into memory.
 *****************************************************************************/
char* load_file(const char *filename)
{
  FILE *f = NULL;
  long fsize = 0;
  char *buffer = NULL;

  f = fopen(filename, "rb");
  if (f != NULL) {
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc(fsize + 1);
    if (buffer != NULL) {
      fread(buffer, 1, fsize, f);
      buffer[fsize] = 0;
    }
    fclose(f);
  }
  return buffer;
}

/**************************************************************************//**
 * Initialise parser module.
 *****************************************************************************/
sl_status_t aoa_parse_init(char *config)
{
  // Check preconditions.
  if (NULL != root) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }
  if (NULL == config) {
    return SL_STATUS_NULL_POINTER;
  }

  root = cJSON_Parse(config);
  if (NULL == root) {
    return SL_STATUS_INITIALIZATION;
  }

  locator_index = 0;
  allowlist_index = 0;

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Parse multilocator config.
 *****************************************************************************/
sl_status_t aoa_parse_multilocator(aoa_id_t id)
{
  cJSON *param;

  // Check preconditions.
  if (NULL == root) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (NULL == id) {
    return SL_STATUS_NULL_POINTER;
  }

  param = cJSON_GetObjectItem(root, "id");
  CHECK_TYPE(param, cJSON_String);
  aoa_id_copy(id, param->valuestring);

  return SL_STATUS_OK;
}

#ifdef RTL_LIB
/**************************************************************************//**
 * Parse next item from the locator config list.
 *****************************************************************************/
sl_status_t aoa_parse_locator(aoa_id_t id,
                              struct sl_rtl_loc_locator_item *loc)
{
  cJSON *array;
  cJSON *item;
  cJSON *param;
  cJSON *subparam;
  uint8_t address[ADR_LEN];
  uint8_t address_type;

  // Check preconditions.
  if (NULL == root) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if ((NULL == id) || (NULL == loc)) {
    return SL_STATUS_NULL_POINTER;
  }

  array = cJSON_GetObjectItem(root, "locators");
  CHECK_TYPE(array, cJSON_Array);
  // Check if locator index is valid.
  if (locator_index >= cJSON_GetArraySize(array)) {
    return SL_STATUS_NOT_FOUND;
  }
  // Get next locator element from the array.
  item = cJSON_GetArrayItem(array, locator_index);
  CHECK_TYPE(item, cJSON_Object);

  // Parse locator ID.
  param = cJSON_GetObjectItem(item, "id");
  CHECK_TYPE(param, cJSON_String);
  aoa_id_copy(id, param->valuestring);

  // Convert the id to address and back. This will take care about the case.
  aoa_id_to_address(id, address, &address_type);
  aoa_address_to_id(address, address_type, id);

  // Parse position.
  param = cJSON_GetObjectItem(item, "coordinate");
  CHECK_TYPE(param, cJSON_Object);
  subparam = cJSON_GetObjectItem(param, "x");
  CHECK_TYPE(subparam, cJSON_Number);
  loc->coordinate_x = (float)subparam->valuedouble;
  subparam = cJSON_GetObjectItem(param, "y");
  CHECK_TYPE(subparam, cJSON_Number);
  loc->coordinate_y = (float)subparam->valuedouble;
  subparam = cJSON_GetObjectItem(param, "z");
  CHECK_TYPE(subparam, cJSON_Number);
  loc->coordinate_z = (float)subparam->valuedouble;

  // Parse orientation.
  param = cJSON_GetObjectItem(item, "orientation");
  CHECK_TYPE(param, cJSON_Object);
  subparam = cJSON_GetObjectItem(param, "x");
  CHECK_TYPE(subparam, cJSON_Number);
  loc->orientation_x_axis_degrees = (float)subparam->valuedouble;
  subparam = cJSON_GetObjectItem(param, "y");
  CHECK_TYPE(subparam, cJSON_Number);
  loc->orientation_y_axis_degrees = (float)subparam->valuedouble;
  subparam = cJSON_GetObjectItem(param, "z");
  CHECK_TYPE(subparam, cJSON_Number);
  loc->orientation_z_axis_degrees = (float)subparam->valuedouble;

  // Increment locator index.
  ++locator_index;

  return SL_STATUS_OK;
}
#endif // RTL_LIB

/**************************************************************************//**
 * Parse azimuth angle mask configuration.
 *****************************************************************************/
sl_status_t aoa_parse_azimuth(float *min, float *max)
{
  cJSON *param;
  cJSON *subparam;

  // Check preconditions.
  if (NULL == root) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if ((NULL == min) || (NULL == max)) {
    return SL_STATUS_NULL_POINTER;
  }

  param = cJSON_GetObjectItem(root, "azimuth_mask");
  if (NULL == param) {
    // Azimuth angle mask configuration is optional.
    return SL_STATUS_NOT_FOUND;
  }
  subparam = cJSON_GetObjectItem(param, "min");
  CHECK_TYPE(subparam, cJSON_Number);
  *min = (float)subparam->valuedouble;
  subparam = cJSON_GetObjectItem(param, "max");
  CHECK_TYPE(subparam, cJSON_Number);
  *max = (float)subparam->valuedouble;

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Parse next item from the allowlist.
 *****************************************************************************/
sl_status_t aoa_parse_allowlist(uint8_t address[ADR_LEN], uint8_t *address_type)
{
  cJSON *array;
  cJSON *param;

  // Check preconditions.
  if (NULL == root) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (NULL == address || NULL == address_type) {
    return SL_STATUS_NULL_POINTER;
  }

  array = cJSON_GetObjectItem(root, "tag_allowlist");
  if (NULL == array) {
    // Allowlist configuration is optional.
    return SL_STATUS_NOT_FOUND;
  }
  // Check if allowlist index is valid.
  if (allowlist_index >= cJSON_GetArraySize(array)) {
    return SL_STATUS_NOT_FOUND;
  }
  // Get next allowlist element from the array.
  param = cJSON_GetArrayItem(array, allowlist_index);
  CHECK_TYPE(param, cJSON_String);
  // Convert the id to address. This will take care about the case.
  aoa_id_to_address(param->valuestring, address, address_type);

  // Increment allowlist index.
  ++allowlist_index;

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Deinitialise parser module.
 *****************************************************************************/
sl_status_t aoa_parse_deinit(void)
{
  // Check precondition.
  if (NULL == root) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  cJSON_Delete(root);
  root = NULL;

  return SL_STATUS_OK;
}
