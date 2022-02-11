#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <openssl/sha.h>
#include <glib.h>
#include <algorithm>
#include "nvds_logger.h"
#include "nvds_msgapi.h"
#include "nvds_utils.h"
#include "libnvds_mqtt_sink.h"
#include "mqtt_client.h"

using namespace std;

/////////////////////////////////////////////////////////////////////////////

/**
 * Connects to a remote mqtt broker based on connection string.
 */
NvDsMsgApiHandle nvds_msgapi_connect(char *connection_str, nvds_msgapi_connect_cb_t connect_cb, char *config_path) {
    nvds_log_open();

    async_client *ch = new(nothrow) async_client(connect_cb);

    if (config_path) {
        if (!ch->read_config(config_path)) {
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Connection failed \n");
            return NULL;
        }
    }

    if (!ch->set_connection_str(connection_str)){
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT connection string parsing failed. Connection failed \n");
        return NULL;
    }

    if (!ch->connect()){
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT Connection failed \n");
        return NULL;
    }

    return (NvDsMsgApiHandle)(ch);
}


/**
 * This api will be used to create & intialize mqtt consumer
 * A consumer thread is spawned to listen & consume messages on topics specified in the api
 */
NvDsMsgApiErrorType nvds_msgapi_subscribe(NvDsMsgApiHandle h_ptr, char **topics, int num_topics,
                                          nvds_msgapi_subscribe_request_cb_t cb, void *user_ctx) {
    async_client *ch = (async_client *) h_ptr;
    if (ch == NULL) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR,
                 "MQTT connection handle passed for nvds_msgapi_subscribe() = NULL. Subscribe failed\n");
        return NVDS_MSGAPI_ERR;
    }

    if (topics == NULL || num_topics <= 0) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Topics not specified for subscribe. Subscription failed\n");
        return NVDS_MSGAPI_ERR;
    }

    if (!cb) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Subscribe callback cannot be NULL. subscription failed\n");
        return NVDS_MSGAPI_ERR;
    }

    for (int i = 0; i < num_topics; i++) {
        if (!ch->subscribe(topics[i], cb, user_ctx)) {
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Subscription failed\n");
            return NVDS_MSGAPI_ERR;
        }
    }

    return NVDS_MSGAPI_OK;
}

/**
* There could be several synchronous and asychronous send operations in flight.
* Once a send operation callback is received the course of action depends on if it's synch or async
* -- if it's sync then the associated complletion flag should  be set
* -- if it's asynchronous then completion callback from the user should be called
*/
 NvDsMsgApiErrorType nvds_msgapi_send(NvDsMsgApiHandle h_ptr, char *topic, const uint8_t *payload, size_t nbuf) {
    async_client *ch = (async_client *) h_ptr;
    if (h_ptr == NULL) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT connection handle passed for send() = NULL. Send failed\n");
        return NVDS_MSGAPI_ERR;
    }
    if (topic == NULL || !strcmp(topic, "")) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT topic not specified.Send failed\n");
        return NVDS_MSGAPI_ERR;
    }
    if (payload == NULL || nbuf <= 0) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "mqtt: Either send payload is NULL or payload length <=0. Send failed\n");
        return NVDS_MSGAPI_ERR;
    }

    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "nvds_msgapi_send: payload=%.*s \n topic = %s\n", nbuf, payload, topic);

    if (!ch->send(topic, payload, nbuf)) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Send failed\n");
        return NVDS_MSGAPI_ERR;
    }

    return NVDS_MSGAPI_OK;
}

NvDsMsgApiErrorType nvds_msgapi_send_async(NvDsMsgApiHandle h_ptr, char *topic, const uint8_t *payload, size_t nbuf,
                                           nvds_msgapi_send_cb_t cb, void *user_ctx) {
    async_client *ch = (async_client *) h_ptr;
    if (h_ptr == NULL) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT connection handle passed for send_async() = NULL. Send failed\n");
        return NVDS_MSGAPI_ERR;
    }
    if (topic == NULL || !strcmp(topic, "")) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT topic not specified. Send failed\n");
        return NVDS_MSGAPI_ERR;
    }
    if (payload == NULL || nbuf <= 0) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR,
                 "mqtt: send_async() either payload is NULL or payload length <=0. Send failed\n");
        return NVDS_MSGAPI_ERR;
    }

    if (!cb) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Send callback cannot be NULL. Async send failed\n");
        return NVDS_MSGAPI_ERR;
    }

    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "nvds_msgapi_send_async: payload=%.*s \n topic = %s\n", nbuf, payload, topic);

    if (!ch->send_async(topic, payload, nbuf, cb, user_ctx)) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Send async failed\n");
        return NVDS_MSGAPI_ERR;
    }

    return NVDS_MSGAPI_OK;
}

void nvds_msgapi_do_work(NvDsMsgApiHandle h_ptr) {
    if (h_ptr == NULL) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT connection handle passed for dowork() = NULL. No actions taken\n");
        return;
    }
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "nvds_msgapi_do_work\n");

    async_client *ch = (async_client *) h_ptr;
    ch->do_work();
}

NvDsMsgApiErrorType nvds_msgapi_disconnect(NvDsMsgApiHandle h_ptr) {
    if (!h_ptr) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "nvds_msgapi_disconnect called with null handle\n");
        return NVDS_MSGAPI_ERR;
    }

    async_client *ch = (async_client *) h_ptr;

    if (!ch->disconnect()) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Disconnect failed\n");
        return NVDS_MSGAPI_ERR;
    }

    delete ch;

    nvds_log_close();
    return NVDS_MSGAPI_OK;
}

/**
  * Returns version of API supported by this adaptor
  */
char *nvds_msgapi_getversion() {
    return (char *) NVDS_MSGAPI_VERSION;
}

//Returns underlying framework/protocol name
char *nvds_msgapi_get_protocol_name() {
    return (char *) NVDS_MSGAPI_PROTOCOL;
}

//Query connection signature
NvDsMsgApiErrorType nvds_msgapi_connection_signature(char *broker_str, char *cfg, char *output_str, int max_len) {
    strcpy(output_str, "");

    if (broker_str == NULL || cfg == NULL) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "nvds_msgapi_connection_signature: broker_str or cfg path cant be NULL");
        return NVDS_MSGAPI_ERR;
    }

    // TODO: Create a valid signature, but for now do this:
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO,
             "nvds_msgapi_connection_signature: MQTT connection sharing disabled. Hence connection signature cant be returned");
    return NVDS_MSGAPI_OK;
}
