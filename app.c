/***************************************************************************//**
 * @file
 * @brief AoA locator application.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sl_bt_api.h"
#include "ncp_host.h"
#include "app_log.h"
#include "app_log_cli.h"
#include "app_assert.h"
#include "app.h"
#include "mqtt.h"
#include "app_config.h"

#include "conn.h"
#include "aoa_config.h"
#include "aoa_parse.h"
#include "aoa_serdes.h"
#include "aoa_util.h"
#ifdef AOA_ANGLE
#include "aoa_angle.h"
#include "aoa_angle_config.h"
#endif // AOA_ANGLE

// Optstring argument for getopt.
#define OPTSTRING      NCP_HOST_OPTSTRING APP_LOG_OPTSTRING "m:c:h"

// Usage info.
#define USAGE          APP_LOG_NL "%s " NCP_HOST_USAGE APP_LOG_USAGE "[-m <mqtt_address>[:<port>]] [-c <config>] [-h]" APP_LOG_NL

// Options info.
#define OPTIONS                                                                \
  "\nOPTIONS\n"                                                                \
  NCP_HOST_OPTIONS                                                             \
  APP_LOG_OPTIONS                                                              \
  "    -m  MQTT broker connection parameters.\n"                               \
  "        <mqtt_address>   Address of the MQTT broker (default: localhost)\n" \
  "        <port>           Port of the MQTT broker (default: 1883)\n"         \
  "    -c  Locator configuration file.\n"                                      \
  "        <config>         Path to the configuration file\n"                  \
  "    -h  Print this help message.\n"

static void parse_config(char *filename);
#ifdef AOA_ANGLE
static void on_message(mqtt_handle_t *handle, const char *topic, const char *payload);
static void subscribe_correction(void);
#endif // AOA_ANGLE

// Locator ID
static aoa_id_t locator_id;

// MQTT variables
static mqtt_handle_t mqtt_handle = MQTT_DEFAULT_HANDLE;
static char *mqtt_host = NULL;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(int argc, char *argv[])
{
  sl_status_t sc;
  int opt;
  char *port_str;

  aoa_allowlist_init();

  // Process command line options.
  while ((opt = getopt(argc, argv, OPTSTRING)) != -1) {
    switch (opt) {
      // MQTT broker connection parameters.
      case 'm':
        strtok(optarg, ":");
        mqtt_host = malloc(strlen(optarg) + 1);
        if (mqtt_host != NULL) {
          strcpy(mqtt_host, optarg);
          mqtt_handle.host = mqtt_host;
        }
        port_str = strtok(NULL, ":");
        if (port_str != NULL) {
          mqtt_handle.port = atoi(port_str);
        }
        break;
      // Locator configuration file.
      case 'c':
        parse_config(optarg);
        break;
      // Print help.
      case 'h':
        app_log(USAGE, argv[0]);
        app_log(OPTIONS);
        exit(EXIT_SUCCESS);

      // Process options for other modules.
      default:
        sc = ncp_host_set_option((char)opt, optarg);
        if (sc == SL_STATUS_NOT_FOUND) {
          sc = app_log_set_option((char)opt, optarg);
        }
        if (sc != SL_STATUS_OK) {
          app_log(USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
    }
  }

  // Initialize NCP connection.
  sc = ncp_host_init();
  if (sc == SL_STATUS_INVALID_PARAMETER) {
    app_log(USAGE, argv[0]);
    exit(EXIT_FAILURE);
  }
  app_assert_status(sc);
  app_log_info("NCP host initialised." APP_LOG_NL);
  app_log_info("Resetting NCP target..." APP_LOG_NL);
  // Reset NCP to ensure it gets into a defined state.
  // Once the chip successfully boots, boot event should be received.
  sl_bt_system_reset(sl_bt_system_boot_mode_normal);

  init_connection();
  app_log_info("Press Crtl+C to quit" APP_LOG_NL APP_LOG_NL);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void)
{
  mqtt_step(&mqtt_handle);
}

/**************************************************************************//**
 * Application Deinit.
 *****************************************************************************/
void app_deinit(void)
{
  app_log_info("Shutting down." APP_LOG_NL);
  ncp_host_deinit();
  mqtt_deinit(&mqtt_handle);
  if (mqtt_host != NULL) {
    free(mqtt_host);
  }
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  mqtt_status_t rc;
  bd_addr address;
  uint8_t address_type;

  // Catch boot event...
  if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_system_boot_id) {
    // Print boot message.
    app_log_info("Bluetooth stack booted: v%d.%d.%d-b%d" APP_LOG_NL,
                 evt->data.evt_system_boot.major,
                 evt->data.evt_system_boot.minor,
                 evt->data.evt_system_boot.patch,
                 evt->data.evt_system_boot.build);
    // Extract unique ID from BT Address.
    sc = sl_bt_system_get_identity_address(&address, &address_type);
    app_assert_status(sc);
    app_log_info("Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X" APP_LOG_NL,
                 address_type ? "static random" : "public device",
                 address.addr[5],
                 address.addr[4],
                 address.addr[3],
                 address.addr[2],
                 address.addr[1],
                 address.addr[0]);

    aoa_address_to_id(address.addr, address_type, locator_id);

    // Connect to the MQTT broker
    mqtt_handle.client_id = locator_id;
    mqtt_handle.on_connect = aoa_on_connect;
    rc = mqtt_init(&mqtt_handle);
    app_assert(rc == MQTT_SUCCESS, "MQTT init failed." APP_LOG_NL);

#ifdef AOA_ANGLE
    mqtt_handle.on_message = on_message;
    subscribe_correction();
#endif // AOA_ANGLE
  }
  // ...then call the connection specific event handler.
  app_bt_on_event(evt);
}

#ifdef AOA_ANGLE
/**************************************************************************//**
 * Subscribe for angle feedback messages from the multilocator.
 *****************************************************************************/
static void subscribe_correction(void)
{
  const char topic_template[] = AOA_TOPIC_CORRECTION_PRINT;
  char topic[sizeof(topic_template) + sizeof(aoa_id_t) + 1];
  mqtt_status_t rc;

  snprintf(topic, sizeof(topic), topic_template, locator_id, "+");

  app_log_info("Subscribing to topic '%s'." APP_LOG_NL, topic);

  rc = mqtt_subscribe(&mqtt_handle, topic);
  app_assert(rc == MQTT_SUCCESS, "Failed to subscribe to topic '%s'." APP_LOG_NL, topic);
}

/**************************************************************************//**
 * MQTT message arrived callback.
 *****************************************************************************/
static void on_message(mqtt_handle_t *handle, const char *topic, const char *payload)
{
  int result;
  aoa_id_t loc_id, tag_id;
  aoa_correction_t correction;
  bd_addr tag_addr;
  uint8_t tag_addr_type;
  conn_properties_t *tag = NULL;
  enum sl_rtl_error_code ec;
  sl_status_t sc;

  (void)handle;

  // Parse topic
  result = sscanf(topic, AOA_TOPIC_CORRECTION_SCAN, loc_id, tag_id);
  app_assert(result == 2, "Failed to parse correction topic: %d." APP_LOG_NL, result);

  if (aoa_id_compare(loc_id, locator_id) != 0) {
    // Accidentally got a wrong message
    return;
  }
  // Find asset tag in the database
  sc = aoa_id_to_address(tag_id, tag_addr.addr, &tag_addr_type);
  if (SL_STATUS_OK == sc) {
    tag = get_connection_by_address(&tag_addr);
  }
  if (tag != NULL) {
    // Parse payload
    sc = aoa_deserialize_correction((char *)payload, &correction);
    app_assert_status(sc);

    if (aoa_sequence_compare(tag->sequence, correction.sequence) <= MAX_CORRECTION_DELAY) {
      app_log_info("Apply correction #%d for asset tag '%s'" APP_LOG_NL, correction.sequence, tag_id);
      ec = aoa_set_correction(&tag->aoa_state, &correction);
      app_assert(ec == SL_RTL_ERROR_SUCCESS,
                 "[E: %d] Failed to set correction values" APP_LOG_NL, ec);
    } else {
      app_log_info("Omit correction #%d for asset tag '%s'" APP_LOG_NL, correction.sequence, tag_id);
    }
  }
}
#endif // AOA_ANGLE

/**************************************************************************//**
 * IQ report callback.
 *****************************************************************************/
void app_on_iq_report(conn_properties_t *tag, aoa_iq_report_t *iq_report)
{
  aoa_id_t tag_id;
  mqtt_status_t rc;
  char *payload;
  sl_status_t sc;

#ifndef AOA_ANGLE
  const char topic_template[] = AOA_TOPIC_IQ_REPORT_PRINT;

  // Compile payload
  sc = aoa_serialize_iq_report(iq_report, &payload);
#else
  enum sl_rtl_error_code ec;
  aoa_angle_t angle;
  const char topic_template[] = AOA_TOPIC_ANGLE_PRINT;

  ec = aoa_calculate(&tag->aoa_state, iq_report, &angle);
  if (ec == SL_RTL_ERROR_ESTIMATION_IN_PROGRESS) {
    // No valid angles are available yet.
    return;
  }
  app_assert(ec == SL_RTL_ERROR_SUCCESS,
             "[E: %d] Failed to calculate angle" APP_LOG_NL, ec);

  // Store the latest sequence number for the tag.
  tag->sequence = iq_report->event_counter;

  // Compile payload
  sc = aoa_serialize_angle(&angle, &payload);
#endif // AOA_ANGLE
  app_assert_status(sc);

  // Compile topic
  char topic[sizeof(topic_template) + sizeof(aoa_id_t) + sizeof(aoa_id_t)];
  aoa_address_to_id(tag->address.addr, tag->address_type, tag_id);
  snprintf(topic, sizeof(topic), topic_template, locator_id, tag_id);

  // Send message
  rc = mqtt_publish(&mqtt_handle, topic, payload);
  app_assert(rc == MQTT_SUCCESS, "Failed to publish to topic '%s'." APP_LOG_NL, topic);

  // Clean up
  free(payload);
}

/**************************************************************************//**
 * Configuration file parser
 *****************************************************************************/
static void parse_config(char *filename)
{
  sl_status_t sc;
  char *buffer;
  aoa_id_t id;
  uint8_t address[ADR_LEN], address_type;

  buffer = load_file(filename);
  app_assert(buffer != NULL, "Failed to load file: %s" APP_LOG_NL, filename);

  sc = aoa_parse_init(buffer);
  app_assert_status(sc);

#ifdef AOA_ANGLE
  sc = aoa_parse_azimuth(&aoa_azimuth_min, &aoa_azimuth_max);
  app_assert((sc == SL_STATUS_OK) || (sc == SL_STATUS_NOT_FOUND),
             "[E: 0x%04x] aoa_parse_azimuth failed" APP_LOG_NL,
             (int)sc);
#endif // AOA_ANGLE

  do {
    sc = aoa_parse_allowlist(address, &address_type);
    if (sc == SL_STATUS_OK) {
      aoa_address_to_id(address, address_type, id);
      app_log_info("Adding tag id '%s' to the allowlist." APP_LOG_NL, id);
      sc = aoa_allowlist_add(address);
    } else {
      app_assert(sc == SL_STATUS_NOT_FOUND,
                 "[E: 0x%04x] aoa_parse_allowlist failed" APP_LOG_NL,
                 (int)sc);
    }
  } while (sc == SL_STATUS_OK);

  sc = aoa_parse_deinit();
  app_assert_status(sc);

  free(buffer);
}
