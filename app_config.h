/***************************************************************************//**
 * @file
 * @brief Application configuration values.
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

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "aoa_board.h"

// Maximum number of asset tags handled by the application.
#define AOA_MAX_TAGS                   8

// Measurement interval expressed as the number of connection events.
#define CTE_SAMPLING_INTERVAL          3

// Minimum CTE length requested in 8 us units. Ranges from 16 to 160 us.
#define CTE_MIN_LENGTH                 20

// Maximum number of sampled CTEs in each advertising interval.
// 0: Sample and report all available CTEs.
#define CTE_COUNT                      0

// Switching and sampling slots in us (1 or 2).
#define CTE_SLOT_DURATION              1

#endif // APP_CONFIG_H
