/***************************************************************************//**
 * @file
 * @brief Configuration values for AoA angle estimation.
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

#ifndef AOA_ANGLE_CONFIG_H
#define AOA_ANGLE_CONFIG_H

#include <math.h>
#include "sl_rtl_clib_api.h"
#include "aoa_board.h"

// AoA estimator mode
#define AOX_MODE                       SL_RTL_AOX_MODE_REAL_TIME_BASIC

// Reference RSSI value of the asset tag at 1.0 m distance in dBm.
#define TAG_TX_POWER                   (-45.0)

// Filter weight applied on the estimated distance. Ranges from 0 to 1.
#define AOA_FILTERING_AMOUNT           0.6f

// Default value for the lower bound of the azimuth mask.
// Can be overridden with runtime configuration. Use NAN to disable.
#define AOA_AZIMUTH_MASK_MIN_DEFAULT   NAN

// Default value for the upper bound of the azimuth mask.
// Can be overridden with runtime configuration. Use NAN to disable.
#define AOA_AZIMUTH_MASK_MAX_DEFAULT   NAN

// Direction correction will be cleared if this amout of IQ reports are received
// without receiving a correction message.
#define CORRECTION_TIMEOUT             5

// Correction values with a sequence number more than MAX_CORRECTION_DELAY apart
// from the last IQ report are considered outdated and will be ignored.
#define MAX_CORRECTION_DELAY           3

#endif // AOA_ANGLE_CONFIG_H
