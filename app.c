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
#include "tcp.h"

#include "conn.h"
#include "aoa_parse.h"
#include "aoa_util.h"
#ifdef AOA_ANGLE
#include "aoa_angle.h"
#endif // AOA_ANGLE

// Optstring argument for getopt.
#define OPTSTRING      NCP_HOST_OPTSTRING APP_LOG_OPTSTRING "s:c:h"

// Usage info.
#define USAGE          APP_LOG_NL "%s " NCP_HOST_USAGE APP_LOG_USAGE "[-s <server_address>[:<port>]] [-c <config>] [-h]" APP_LOG_NL

// Options info.
#define OPTIONS                                                                  \
  "\nOPTIONS\n"                                                                  \
  NCP_HOST_OPTIONS                                                               \
  APP_LOG_OPTIONS                                                                \
  "    -s  Socket connection parameters.\n"                                      \
  "        <server_address> Address of the socket server (default: localhost)\n" \
  "        <port>           Port of the socket server (default: 8080)\n"         \
  "    -c  Locator configuration file.\n"                                        \
  "        <config>         Path to the configuration file\n"                    \
  "    -h  Print this help message.\n"

static void parse_config(char *filename);

// Locator ID
static aoa_id_t locator_id;

// Socket
#define SOCKET_BUFFER_SIZE 1024
#define PORT_DIGIT_LEN 6
static char *host, port_str[PORT_DIGIT_LEN] = "8080", payload[SOCKET_BUFFER_SIZE];
#if defined(POSIX) && POSIX == 1
static int32_t handle = -1;
#else // defined(POSIX) && POSIX == 1
#include <windows.h>
static SOCKET handle = -1;
#endif // defined(POSIX) && POSIX == 1

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(int argc, char *argv[])
{
  sl_status_t sc;
  int opt;

  aoa_allowlist_init();

  // Process command line options.
  while ((opt = getopt(argc, argv, OPTSTRING)) != -1) {
    switch (opt) {
      // Socket connection parameters.
      case 's':
        printf("%s\n", optarg);
        strtok(optarg, ":");
        printf("%s\n", optarg);
        host = malloc(strlen(optarg) + 1);
        if (host != NULL) {
          strcpy(host, optarg);
        }
        printf("%s\n", host);
        char *port = strtok(NULL, ":");
        if (port != NULL) {
          strcpy(port_str, port);
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
 * Application Deinit.
 *****************************************************************************/
void app_deinit(void)
{
  static bool freed = false;
  if (!freed) {
    app_log_info("Shutting down." APP_LOG_NL);
    ncp_host_deinit();
    if (handle != -1) {
      tcp_close(&handle);
    }
    if (host != NULL) {
      free(host);
    }
  }
  freed = true;
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
  int rc;
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

    // Connect to the socket server
    rc = tcp_open(&handle, host, port_str);
    if (rc < 0) {
      app_deinit();
      exit(EXIT_FAILURE);
    }
  }
  // ...then call the connection specific event handler.
  app_bt_on_event(evt);
}

/**************************************************************************//**
 * IQ report callback.
 *****************************************************************************/
void app_on_iq_report(conn_properties_t *tag, aoa_iq_report_t *iq_report)
{
  aoa_id_t tag_id;
  int rc;
  enum sl_rtl_error_code ec;
  aoa_angle_t angle;

  aoa_address_to_id(tag->address.addr, tag->address_type, tag_id);

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
  rc = snprintf(payload, SOCKET_BUFFER_SIZE,
                "{\n\t\"timeStamp\": %d,\n\t\"type\": \"auditory\", \n\t\"tagId\": %s,\n\t\"azimuth\": %f,\n\t\"distance\": %f,\n\t\"elevation\": %f,\n\t\"quality\": %u\n}\r\n",
                angle.sequence, tag_id, angle.azimuth, angle.distance, angle.elevation, angle.quality);
  printf("%s", payload);
  if (rc < 0) {
    perror("snprintf");
    app_deinit();
    exit(EXIT_FAILURE);
  }

  // Send message
  rc = tcp_tx(&handle, SOCKET_BUFFER_SIZE, (uint8_t *)&payload);
  if (rc < 0) {
    app_log_info("Connection Closed." APP_LOG_NL);
    app_deinit();
    exit(EXIT_SUCCESS);
  }
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
