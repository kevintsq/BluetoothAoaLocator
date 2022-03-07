/***************************************************************************//**
 * @file
 * @brief Synchronous MQTT client for text based message transmission.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef MQTT_H
#define MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "mosquitto.h"

// MQTT status codes
typedef enum mqtt_status_e {
  MQTT_SUCCESS = 0,
  MQTT_ERROR_CONNECT,
  MQTT_ERROR_PUBLISH,
  MQTT_ERROR_SUBSCRIBE,
  MQTT_ERROR_STEP
} mqtt_status_t;

// Forward declaration
typedef struct mqtt_handle_s mqtt_handle_t;

// On successful connection callback function prototype (handle)
typedef void (*mqtt_on_connect_t)(mqtt_handle_t *);

// On message callback function prototype (handle, topic, payload)
typedef void (*mqtt_on_message_t)(mqtt_handle_t *, const char *, const char *);

// MQTT client instance
struct mqtt_handle_s {
  // Public members
  char *host;
  int port;
  char *client_id;
  mqtt_on_connect_t on_connect;
  mqtt_on_message_t on_message;
  // Private members
  struct mosquitto *client;
  char *topic_list;
  size_t topic_size;
};

#define MQTT_DEFAULT_HANDLE \
  {                         \
    .host = "localhost",    \
    .port = 1883,           \
    .client_id = NULL,      \
    .on_connect = NULL,     \
    .on_message = NULL,     \
    .client = NULL,         \
    .topic_list = NULL,     \
    .topic_size = 0         \
  }

// Public functions
mqtt_status_t mqtt_init(mqtt_handle_t *handle);
mqtt_status_t mqtt_publish(mqtt_handle_t *handle, const char *topic, const char *payload);
mqtt_status_t mqtt_subscribe(mqtt_handle_t *handle, const char *topic);
mqtt_status_t mqtt_step(mqtt_handle_t *handle);
mqtt_status_t mqtt_deinit(mqtt_handle_t *handle);

#ifdef __cplusplus
};
#endif

#endif // MQTT_H
