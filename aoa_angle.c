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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "app_log.h"
#include "aoa_angle.h"
#include "aoa_angle_config.h"

#define CHECK_ERROR(x)           if ((x) != SL_RTL_ERROR_SUCCESS) return (x)

// -----------------------------------------------------------------------------
// Public variables

float aoa_azimuth_min = AOA_AZIMUTH_MASK_MIN_DEFAULT;
float aoa_azimuth_max = AOA_AZIMUTH_MASK_MAX_DEFAULT;

// -----------------------------------------------------------------------------
// Private variables

static float **ref_i_samples;
static float **ref_q_samples;
static float **i_samples;
static float **q_samples;

// -----------------------------------------------------------------------------
// Private function declarations

static void init_buffers(void);
static uint32_t allocate_2D_float_buffer(float*** buf, uint32_t rows, uint32_t cols);
static float channel_to_frequency(uint8_t channel);
static void get_samples(aoa_iq_report_t *iq_report);

/***************************************************************************//**
 * Initialize angle calculation libraries
 ******************************************************************************/
enum sl_rtl_error_code aoa_init(aoa_state_t *aoa_state)
{
  enum sl_rtl_error_code ec;
  // Initialize local buffers
  init_buffers();
  // Initialize AoX library
  ec = sl_rtl_aox_init(&aoa_state->libitem);
  CHECK_ERROR(ec);
  // Set the number of snapshots, i.e. how many times the antennas are scanned
  // during one measurement
  ec = sl_rtl_aox_set_num_snapshots(&aoa_state->libitem, AOA_NUM_SNAPSHOTS);
  CHECK_ERROR(ec);
  // Set the antenna array type
  ec = sl_rtl_aox_set_array_type(&aoa_state->libitem, AOX_ARRAY_TYPE);
  CHECK_ERROR(ec);
  // Select mode (high speed/high accuracy/etc.)
  ec = sl_rtl_aox_set_mode(&aoa_state->libitem, AOX_MODE);
  CHECK_ERROR(ec);
  // Enable IQ sample quality analysis processing
  ec = sl_rtl_aox_iq_sample_qa_configure(&aoa_state->libitem);
  CHECK_ERROR(ec);
  // Add azimuth constraint if min and max values are valid
  if (!isnan(aoa_azimuth_min) && !isnan(aoa_azimuth_max)) {
    app_log_info("Disable azimuth values between %f and %f" APP_LOG_NL,
                 aoa_azimuth_min, aoa_azimuth_max);
    ec = sl_rtl_aox_add_constraint(&aoa_state->libitem,
                                   SL_RTL_AOX_CONSTRAINT_TYPE_AZIMUTH,
                                   aoa_azimuth_min,
                                   aoa_azimuth_max);
    CHECK_ERROR(ec);
  }
  // Create AoX estimator
  ec = sl_rtl_aox_create_estimator(&aoa_state->libitem);
  CHECK_ERROR(ec);
  // Initialize an util item
  ec = sl_rtl_util_init(&aoa_state->util_libitem);
  CHECK_ERROR(ec);
  ec = sl_rtl_util_set_parameter(&aoa_state->util_libitem,
                                 SL_RTL_UTIL_PARAMETER_AMOUNT_OF_FILTERING,
                                 AOA_FILTERING_AMOUNT);
  CHECK_ERROR(ec);
  // Initialize correction timeout counter
  aoa_state->correction_timeout = 0;

  return ec;
}

/***************************************************************************//**
 * Estimate angle data from IQ samples
 ******************************************************************************/
enum sl_rtl_error_code aoa_calculate(aoa_state_t *aoa_state,
                                     aoa_iq_report_t *iq_report,
                                     aoa_angle_t *angle)
{
  enum sl_rtl_error_code ec;
  float phase_rotation;

  // Copy IQ samples into preallocated buffers.
  get_samples(iq_report);

  // Calculate phase rotation from reference IQ samples.
  ec = sl_rtl_aox_calculate_iq_sample_phase_rotation(&aoa_state->libitem,
                                                     2.0f,
                                                     ref_i_samples[0],
                                                     ref_q_samples[0],
                                                     AOA_REF_PERIOD_SAMPLES,
                                                     &phase_rotation);
  CHECK_ERROR(ec);

  // Provide calculated phase rotation to the estimator.
  ec = sl_rtl_aox_set_iq_sample_phase_rotation(&aoa_state->libitem,
                                               phase_rotation);
  CHECK_ERROR(ec);

  // Estimate Angle of Arrival from IQ samples.
  // sl_rtl_aox_process will return SL_RTL_ERROR_ESTIMATION_IN_PROGRESS
  // until it has received enough packets for angle estimation.
  ec = sl_rtl_aox_process(&aoa_state->libitem,
                          i_samples,
                          q_samples,
                          channel_to_frequency(iq_report->channel),
                          &angle->azimuth,
                          &angle->elevation);
  CHECK_ERROR(ec);

  // Calculate distance from RSSI.
  ec = sl_rtl_util_rssi2distance(TAG_TX_POWER,
                                 (float)iq_report->rssi,
                                 &angle->distance);
  CHECK_ERROR(ec);
  ec = sl_rtl_util_filter(&aoa_state->util_libitem,
                          angle->distance,
                          &angle->distance);
  CHECK_ERROR(ec);

  // Copy sequence counter.
  angle->sequence = iq_report->event_counter;

  // Fetch the quality result.
  angle->quality = sl_rtl_aox_iq_sample_qa_get_results(&aoa_state->libitem);

  if (aoa_state->correction_timeout > 0) {
    // Decrement timeout counter.
    --aoa_state->correction_timeout;
    if (aoa_state->correction_timeout == 0) {
      // Timer expired, clear correction values.
      ec = sl_rtl_aox_clear_expected_direction(&aoa_state->libitem);
      app_log_info("Clear correction values" APP_LOG_NL);
    }
  }
  return ec;
}

/***************************************************************************//**
 * Set correction data for the estimator
 ******************************************************************************/
enum sl_rtl_error_code aoa_set_correction(aoa_state_t *aoa_state,
                                          aoa_correction_t *correction)
{
  enum sl_rtl_error_code ec;
  ec = sl_rtl_aox_set_expected_direction(&aoa_state->libitem,
                                         correction->direction.azimuth,
                                         correction->direction.elevation);
  CHECK_ERROR(ec);
  ec = sl_rtl_aox_set_expected_deviation(&aoa_state->libitem,
                                         correction->deviation.azimuth,
                                         correction->deviation.elevation);
  CHECK_ERROR(ec);

  aoa_state->correction_timeout = CORRECTION_TIMEOUT;
  return ec;
}

/***************************************************************************//**
 * Deinitialize angle calculation libraries
 ******************************************************************************/
enum sl_rtl_error_code aoa_deinit(aoa_state_t *aoa_state)
{
  enum sl_rtl_error_code ec;

  ec = sl_rtl_aox_deinit(&aoa_state->libitem);
  CHECK_ERROR(ec);
  ec = sl_rtl_util_deinit(&aoa_state->util_libitem);

  return ec;
}

// -----------------------------------------------------------------------------
// Private function declarations

static void init_buffers(void)
{
  static bool initialized = false;

  if (!initialized) {
    initialized = true;
    allocate_2D_float_buffer(&ref_i_samples, AOA_NUM_SNAPSHOTS, AOA_NUM_ARRAY_ELEMENTS);
    allocate_2D_float_buffer(&ref_q_samples, AOA_NUM_SNAPSHOTS, AOA_NUM_ARRAY_ELEMENTS);

    allocate_2D_float_buffer(&i_samples, AOA_NUM_SNAPSHOTS, AOA_NUM_ARRAY_ELEMENTS);
    allocate_2D_float_buffer(&q_samples, AOA_NUM_SNAPSHOTS, AOA_NUM_ARRAY_ELEMENTS);
  }
}

static uint32_t allocate_2D_float_buffer(float*** buf, uint32_t rows, uint32_t cols)
{
  *buf = malloc(sizeof(float*) * rows);
  if (*buf == NULL) {
    return 0;
  }

  for (uint32_t i = 0; i < rows; i++) {
    (*buf)[i] = malloc(sizeof(float) * cols);
    if ((*buf)[i] == NULL) {
      return 0;
    }
  }

  return 1;
}

static float channel_to_frequency(uint8_t channel)
{
  static const uint8_t logical_to_physical_channel[40] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
    0, 12, 39
  };

  // Return the center frequency of the given channel.
  return 2402000000 + 2000000 * logical_to_physical_channel[channel];
}

static void get_samples(aoa_iq_report_t *iq_report)
{
  uint32_t index = 0;
  // Write reference IQ samples into the IQ sample buffer (sampled on one antenna)
  for (uint32_t sample = 0; sample < AOA_REF_PERIOD_SAMPLES; ++sample) {
    ref_i_samples[0][sample] = iq_report->samples[index++] / 127.0;
    if (index == iq_report->length) {
      break;
    }
    ref_q_samples[0][sample] = iq_report->samples[index++] / 127.0;
    if (index == iq_report->length) {
      break;
    }
  }
  index = AOA_REF_PERIOD_SAMPLES * 2;
  // Write antenna IQ samples into the IQ sample buffer (sampled on all antennas)
  for (uint32_t snapshot = 0; snapshot < AOA_NUM_SNAPSHOTS; ++snapshot) {
    for (uint32_t antenna = 0; antenna < AOA_NUM_ARRAY_ELEMENTS; ++antenna) {
      i_samples[snapshot][antenna] = iq_report->samples[index++] / 127.0;
      if (index == iq_report->length) {
        break;
      }
      q_samples[snapshot][antenna] = iq_report->samples[index++] / 127.0;
      if (index == iq_report->length) {
        break;
      }
    }
    if (index == iq_report->length) {
      break;
    }
  }
}
