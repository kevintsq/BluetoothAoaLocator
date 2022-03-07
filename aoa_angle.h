/***************************************************************************//**
 * @file
 * @brief Estimate angle data from IQ samples.
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

#ifndef AOA_ANGLE_H
#define AOA_ANGLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "aoa_types.h"
#include "sl_rtl_clib_api.h"

/***************************************************************************//**
 * AoA angle estimation handler type, one instance for each asset tag
 ******************************************************************************/
typedef struct aoa_state_s {
  sl_rtl_aox_libitem libitem;
  sl_rtl_util_libitem util_libitem;
  uint8_t correction_timeout;
} aoa_state_t;

/***************************************************************************//**
 * Gloabal value for azimuth mask (minimum)
 ******************************************************************************/
extern float aoa_azimuth_min;

/***************************************************************************//**
 * Gloabal value for azimuth mask (maximum)
 ******************************************************************************/
extern float aoa_azimuth_max;

/***************************************************************************//**
 * Initialize angle calculation libraries
 * @param[in] aoa_state Angle calculation handler
 * @return Status returned by the RTL library
 ******************************************************************************/
enum sl_rtl_error_code aoa_init(aoa_state_t *aoa_state);

/***************************************************************************//**
 * Estimate angle data from IQ samples
 * @param[in] aoa_state Angle calculation handler
 * @param[in] iq_report IQ report to convert
 * @param[out] angle Estimated angle data
 * @return Status returned by the RTL library
 ******************************************************************************/
enum sl_rtl_error_code aoa_calculate(aoa_state_t *aoa_state,
                                     aoa_iq_report_t *iq_report,
                                     aoa_angle_t *angle);

/***************************************************************************//**
 * Set correction data for the estimator
 * @param[in] aoa_state Angle calculation handler
 * @param[in] correction Correction data
 * @return Status returned by the RTL library
 ******************************************************************************/
enum sl_rtl_error_code aoa_set_correction(aoa_state_t *aoa_state,
                                          aoa_correction_t *correction);

/***************************************************************************//**
 * Deinitialize angle calculation libraries
 * @param[in] aoa_state Angle calculation handler
 * @return Status returned by the RTL library
 ******************************************************************************/
enum sl_rtl_error_code aoa_deinit(aoa_state_t *aoa_state);

#ifdef __cplusplus
};
#endif

#endif // AOA_ANGLE_H
