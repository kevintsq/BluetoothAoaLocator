/***************************************************************************//**
 * @file
 * @brief Synchronous MQTT client for text based message transmission.
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

#if defined(_WIN32)
#include <windows.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "app_log.h"
#include "mqtt.h"

// Check if libmosquitto version is at least 1.5.7
#if LIBMOSQUITTO_VERSION_NUMBER < 1005007
#warning Untested libmosquitto version!
#endif

#define QOS                    1
#define KEEPALIVE_INTERVAL_SEC 30
#define LOOP_TIMEOUT_MS        1
#define LOG_MASK               MOSQ_LOG_NONE

static void mqtt_on_connect(struct mosquitto *mosq, void *obj, int rc);
static void mqtt_on_disconnect(struct mosquitto *mosq, void *obj, int rc);
static void mqtt_on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
static void mqtt_on_log(struct mosquitto *mosq, void *obj, int level, const char *str);
static const char * mqtt_err2str(int rc);

mqtt_status_t mqtt_init(mqtt_handle_t *handle)
{
  mqtt_status_t ret = MQTT_SUCCESS;
  int rc = MOSQ_ERR_ERRNO;           // return code if mosquitto_new() fails
  struct mosquitto *mosq;

  mosquitto_lib_init();

  mosq = mosquitto_new(handle->client_id, true, handle);
  if (mosq != NULL) {
    mosquitto_connect_callback_set(mosq, mqtt_on_connect);
    mosquitto_disconnect_callback_set(mosq, mqtt_on_disconnect);
    mosquitto_message_callback_set(mosq, mqtt_on_message);
    mosquitto_log_callback_set(mosq, mqtt_on_log);

    rc = mosquitto_connect(mosq, handle->host, handle->port, KEEPALIVE_INTERVAL_SEC);
  }

  if (rc != MOSQ_ERR_SUCCESS) {
    app_log_error("MQTT init failed: '%s'" APP_LOG_NL, mqtt_err2str(rc));
    ret = MQTT_ERROR_CONNECT;
    handle->client = NULL;
    if (mosq != NULL) {
      mosquitto_destroy(mosq);
    }
  } else {
    handle->client = mosq;
  }

  handle->topic_list = NULL;
  handle->topic_size = 0;

  return ret;
}

mqtt_status_t mqtt_publish(mqtt_handle_t *handle, const char *topic, const char *payload)
{
  mqtt_status_t ret = MQTT_SUCCESS;
  int rc;
  int mid;

  if (handle->client != NULL) {
    rc = mosquitto_publish(handle->client, &mid, topic, strlen(payload), payload, QOS, false);
    if (rc != MOSQ_ERR_SUCCESS) {
      app_log_error("MQTT publish attempt failed: '%s'" APP_LOG_NL, mqtt_err2str(rc));
      ret = MQTT_ERROR_PUBLISH;
    }
  } else {
    ret = MQTT_ERROR_PUBLISH;
  }

  return ret;
}

mqtt_status_t mqtt_step(mqtt_handle_t *handle)
{
  mqtt_status_t ret = MQTT_SUCCESS;
  int rc;

  if (handle->client != NULL) {
    rc = mosquitto_loop(handle->client, LOOP_TIMEOUT_MS, 1);
    if (rc != MOSQ_ERR_SUCCESS) {
      app_log_error("MQTT loop failed: '%s'" APP_LOG_NL, mqtt_err2str(rc));
      ret = MQTT_ERROR_STEP;
    }
  } else {
    ret = MQTT_ERROR_STEP;
  }

  return ret;
}

mqtt_status_t mqtt_subscribe(mqtt_handle_t *handle, const char *topic)
{
  mqtt_status_t ret = MQTT_SUCCESS;
  int rc;
  size_t topic_size;

  if (handle->client != NULL) {
    // Try to subscribe to topic.
    rc = mosquitto_subscribe(handle->client, NULL, topic, QOS);

    if ((rc != MOSQ_ERR_SUCCESS) && (rc != MOSQ_ERR_NO_CONN)) {
      app_log_error("MQTT subscribe attempt failed to topic '%s': '%s'" APP_LOG_NL, topic, mqtt_err2str(rc));
      ret = MQTT_ERROR_SUBSCRIBE;
    }

    // Append topic to topic list.
    topic_size = strlen(topic) + 1;
    handle->topic_list = realloc(handle->topic_list, handle->topic_size + topic_size);
    if (handle->topic_list == NULL) {
      app_log_error("MQTT failed to append topic to topic list." APP_LOG_NL);
      ret = MQTT_ERROR_SUBSCRIBE;
    } else {
      strcpy(&handle->topic_list[handle->topic_size], topic);
      handle->topic_size += topic_size;
    }
  } else {
    ret = MQTT_ERROR_SUBSCRIBE;
  }

  return ret;
}

mqtt_status_t mqtt_deinit(mqtt_handle_t *handle)
{
  int rc;

  if (handle->client != NULL) {
    rc = mosquitto_disconnect(handle->client);

    if (rc != MOSQ_ERR_SUCCESS) {
      app_log_error("MQTT failed to disconnect: '%s', continue deinit." APP_LOG_NL,
                    mqtt_err2str(rc));
    }

    mosquitto_destroy(handle->client);
    mosquitto_lib_cleanup();
    if (handle->topic_list != NULL) {
      free(handle->topic_list);
    }
  }

  return MQTT_SUCCESS;
}

static void mqtt_on_connect(struct mosquitto *mosq, void *obj, int rc)
{
  mqtt_handle_t *handle = (mqtt_handle_t *)obj;
  size_t topic_start = 0;
  char *topic;
  int ret = MOSQ_ERR_SUCCESS;

  app_log_info("MQTT connect status '%s'" APP_LOG_NL, mosquitto_connack_string(rc));

  if (rc == 0) {
    if (handle->on_connect != NULL) {
      handle->on_connect(handle);
    }
    while (topic_start < handle->topic_size) {
      topic = &handle->topic_list[topic_start];
      ret = mosquitto_subscribe(mosq, NULL, topic, QOS);
      topic_start += strlen(topic) + 1;
      if (ret != MOSQ_ERR_SUCCESS) {
        app_log_error("MQTT subscribe attempt failed to topic '%s': '%s'" APP_LOG_NL,
                      topic,
                      mqtt_err2str(ret));
      }
    }
  }
}

static void mqtt_on_disconnect(struct mosquitto *mosq, void *obj, int rc)
{
  int ret;

  app_log_info("MQTT disconnected with reason '%d'" APP_LOG_NL, rc);

  if (rc != 0) {
    ret = mosquitto_reconnect(mosq);
    app_log_level(ret == MOSQ_ERR_SUCCESS ? APP_LOG_LEVEL_INFO : APP_LOG_LEVEL_ERROR,
                  "MQTT reconnection attempt with status '%s'" APP_LOG_NL, mqtt_err2str(ret));
  }
}

static void mqtt_on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
  mqtt_handle_t *handle = (mqtt_handle_t *)obj;
  char *payload;

  if (handle->on_message != NULL) {
    payload = malloc(message->payloadlen + 1);
    if (NULL == payload) {
      app_log_error("MQTT failed to allocate payload buffer." APP_LOG_NL);
    } else {
      memcpy(payload, message->payload, message->payloadlen);
      // Make sure that payload is NULL terminated.
      payload[message->payloadlen] = 0;

      handle->on_message(handle, message->topic, payload);

      free(payload);
    }
  }
}

static void mqtt_on_log(struct mosquitto *mosq, void *obj, int level, const char *str)
{
  if (level & LOG_MASK) {
    app_log("MQTT log (%d): %s" APP_LOG_NL, level, str);
  }
}

#if defined(_WIN32)
static const char * mqtt_err2str(int rc)
{
  char *ret = NULL;
  static char err_str[256];

  if (MOSQ_ERR_ERRNO == rc) {
    // Make sure to have a default output if FormatMessage fails
    // or if error code is not available in errno.
    strncpy(err_str, "Unknown system error", sizeof(err_str));
    if (errno != 0) {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // dwFlags
                    NULL,                // lpSource
                    errno,               // dwMessageId
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // dwLanguageId
                    err_str,             // lpBuffer
                    sizeof(err_str),     // nSize
                    NULL);               // Arguments
    }
    // Make sure that err_str is NULL terminated.
    err_str[sizeof(err_str) - 1] = 0;
    ret = err_str;
  } else {
    ret = (char *)mosquitto_strerror(rc);
  }
  return ret;
}
#else
static const char * mqtt_err2str(int rc)
{
  return (MOSQ_ERR_ERRNO == rc) ? strerror(errno) : mosquitto_strerror(rc);
}
#endif // _WIN32
