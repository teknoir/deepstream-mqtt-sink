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
#include "libnvds_mqtt_sink.h"
#include "nvds_utils.h"
#include "mqtt/async_client.h"

using namespace std;

/**
 * MQTTSink_Config defines params that control what MQTT server the
 * client connects to.
 */
typedef struct {
    /** The hostname or IP address to the mqtt host. */
    string host;
    /** The port to the MQTT server on the host. */
    int port;
    /** The id of the client identifying itself to the server. */
    string client_id;
    /** The topic to publish messages to. */
    string topic;
    /** The directory to store persistent data. */
    string persist_dir;
    /** The quality of service setting. */
    int qos;
    /** The timeout setting. */
    int timeout;
    /** The number of connection retry attempts setting. */
    int retries;
    /** The interval between sending keep-alive message. */
    int keep_alive_interval;
    /** Sets whether the server should remember state for the client across reconnects. (MQTT v3.x only) */
    bool clean_session;
} MQTTSink_Config;


NvDsMsgApiErrorType nvds_mqtt_read_config(MQTTSink_Config &config, char *config_path);
bool is_valid_mqtt_connection_str(char *connection_str, string &burl, string &port);


/**
 * internal function to read settings from config file
 * Documentation needs to indicate that mqtt config parameters are:
  (1) located within application level config file passed to connect
  (2) within the message broker group of the config file
  (3) the various options to rdmqtt are specified based on 'key=value' format, within various entries semi-colon separated
Eg:
[mqtt-sink]
client-id = deepstream-app
topic = deepstream/event
persist-dir = ./persist
*/
NvDsMsgApiErrorType nvds_mqtt_read_config(MQTTSink_Config &config, char *config_path) {
    char client_id[MAX_FIELD_LEN] = "";
    char persist_dir[MAX_FIELD_LEN] = "";
    char qos[MAX_FIELD_LEN] = "";
    char timeout[MAX_FIELD_LEN] = "";
    char retries[MAX_FIELD_LEN] = "";
    char keep_alive_interval[MAX_FIELD_LEN] = "";
    char clean_session[MAX_FIELD_LEN] = "";

    if (fetch_config_value(config_path, CONFIG_MQTT_CLIENT_ID, client_id, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "client_id");
        return NVDS_MSGAPI_ERR;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_PERSIST_DIR, persist_dir, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "persist_dir");
        return NVDS_MSGAPI_ERR;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_QOS, qos, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "qos");
        return NVDS_MSGAPI_ERR;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_TIMEOUT, timeout, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "timeout");
        return NVDS_MSGAPI_ERR;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_RETRIES, retries, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "retries");
        return NVDS_MSGAPI_ERR;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_KEEP_ALIVE_INTERVAL, keep_alive_interval, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "keep_alive_interval");
        return NVDS_MSGAPI_ERR;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_CLEAN_SESSION, clean_session, MAX_FIELD_LEN,NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "clean_session");
        return NVDS_MSGAPI_ERR;
    }

    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "client_id", client_id);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "persist_dir", persist_dir);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "qos", qos);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "timeout", timeout);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "retries", retries);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "keep_alive_interval", keep_alive_interval);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "clean_session", clean_session);

    config.client_id = string(move(client_id));
    config.persist_dir = string(persist_dir);
    config.qos = atoi(qos);
    config.timeout = atoi(timeout);
    config.retries = atoi(retries);
    config.keep_alive_interval = atoi(keep_alive_interval);
    config.clean_session = (bool)atoi(clean_session);

    return NVDS_MSGAPI_OK;
}


/**
 * Validate mqtt connection string format
 * Valid format host;port or (host;port;topic to support backward compatibility)
 */
bool is_valid_mqtt_connection_str(char *connection_str, string &url, string &port) {
    if (connection_str == NULL) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT connection string cant be NULL");
        return false;
    }

    string str(connection_str);
    size_t n = count(str.begin(), str.end(), ';');
    if (n > 2) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT connection string format is invalid");
        return false;
    }

    istringstream iss(connection_str);
    getline(iss, url, ';');
    getline(iss, port, ';');

    if (url == "" || port == "") {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT connection string is invalid. hostname or port is empty\n");
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////

/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback,
                 public virtual mqtt::iaction_listener {
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client *cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options &connOpts_;
    // MQTT deepstream sink config
    MQTTSink_Config config_;
    // Deepstream connection callback
    nvds_msgapi_connect_cb_t connection_cb_;

    void reconnect() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_->connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception &exc) {
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Client %s lost connection and failed to reconnect\n", config_.client_id);
            connection_cb_((NvDsMsgApiHandle) cli_, NVDS_MSGAPI_EVT_DISCONNECT);
        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token &tok) override {
        std::cout << "Connection attempt failed" << std::endl;
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "(Re)Connection attempt failed for client %s\n", config_.client_id);
        if (++nretry_ > config_.retries) {
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Client %s service down\n", config_.client_id);
            connection_cb_((NvDsMsgApiHandle) cli_, NVDS_MSGAPI_EVT_SERVICE_DOWN);
        }
        reconnect();
    }

    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token &tok) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Connection success for client %s\n", config_.client_id);
    }

    // (Re)connection success
    void connected(const std::string &cause) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Connection success for client %s\n", config_.client_id);
//        std::cout << "\nConnection success" << std::endl;
//        std::cout << "\tfor client " << config_.client_id
//                  << " using QoS" << config_.qos << std::endl;
    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string &cause) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_WARNING, "Client %s lost connection: %s\n", config_.client_id, cause.c_str());
        nretry_ = 0;
        reconnect();
    }





    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Message arrived...\n");
//        std::cout << "Message arrived" << std::endl;
//        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
//        std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
        // TODO: Add cb per subscription for passing payload to deepstream
        //      cb(NVDS_MSGAPI_OK, (void *) rkm->payload, (int) rkm->len, (char *) rd_mqtt_topic_name(rkm->rkt), kh->c_instance.cinfo.user_ptr);
    }

    void delivery_complete(mqtt::delivery_token_ptr tok) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Delivery complete...\n");
//        cout << "\tDelivery complete for token: "
//             << (tok ? tok->get_message_id() : -1) << endl;
    }

public:
    callback(mqtt::async_client *cli, mqtt::connect_options &connOpts, MQTTSink_Config &config,
             nvds_msgapi_connect_cb_t connect_cb)
            : nretry_(0), cli_(cli), connOpts_(connOpts), config_(config), connection_cb_(connect_cb) {}
};


/**
 * Local async client class to store config.
 */
class async_client : public virtual mqtt::async_client {
    // MQTT deepstream sink config
    MQTTSink_Config config_;
public:
    async_client(const string &serverURI, MQTTSink_Config &config)
            : mqtt::async_client(serverURI, config.client_id, config.persist_dir), config_(config) {}

    MQTTSink_Config get_config() const {
        return config_;
    }
};

/**
 * Connects to a remote mqtt broker based on connection string.
 */
NvDsMsgApiHandle nvds_msgapi_connect(char *connection_str, nvds_msgapi_connect_cb_t connect_cb, char *config_path) {
    nvds_log_open();

    MQTTSink_Config config;

    if (config_path) {
        if (nvds_mqtt_read_config(config, config_path) == NVDS_MSGAPI_ERR) {
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Connection failed \n");
            return NULL;
        }
    }

    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "client_id", config.client_id);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "persist_dir", config.persist_dir);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "qos", config.qos);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "timeout", config.timeout);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "retries", config.retries);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "keep_alive_interval", config.keep_alive_interval);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "clean_session", config.clean_session);

    string url = "", port = "";
    if (!is_valid_mqtt_connection_str(connection_str, url, port))
        return NULL;

    string address = url + ":" + port;
    async_client *ch = new(nothrow) async_client(address, config);

    auto connOpts = mqtt::connect_options_builder()
            .clean_session(config.clean_session)
            .keep_alive_interval(chrono::seconds(config.keep_alive_interval))
            .finalize();

    callback cb(ch, connOpts, config, connect_cb);
    ch->set_callback(cb);

    try {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "\nConnecting...\n");
        mqtt::token_ptr conntok = ch->connect(connOpts, nullptr, cb);
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Waiting for the connection...\n");
        conntok->wait();
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "  ...OK\n");
    }
    catch (const mqtt::exception &exc) {
        cerr << exc.what() << endl;
        return NULL;
    }

    return (NvDsMsgApiHandle)(ch);
}


/**
 * A subscribe action listener to handle subscriptions.
 */
class subscribe_listener : public virtual mqtt::iaction_listener {

protected:
    void on_failure(const mqtt::token &tok) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "MQTT Subscribe failed\n");
        auto top = tok.get_topics();
        if (top && !top->empty())
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT Subscribe error: topic[%s]\n", (*top)[0].c_str());
        else
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT Subscribe error: unknown topic\n");
    }

    void on_success(const mqtt::token &tok) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "MQTT Subscribed\n");
        auto top = tok.get_topics();
        if (top && !top->empty())
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "MQTT Subscribed: topic[%s],\n", (*top)[0].c_str());
    }

};

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

    try {
        for (int i = 0; i < num_topics; i++) {
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "\nSubscribing to topic: %s ...\n", topics[i]);
            subscribe_listener subscribeListener;
            mqtt::token_ptr subtok = ch->subscribe(topics[i], 2, nullptr, subscribeListener); // ch->get_config().qos); //, nullptr, subscribeListener);
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Waiting for subscription...\n");
            subtok->wait();
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "  ...OK\n");
        }
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT subscribe() : Subscribe failed: %s\n", exc.what());
        return NVDS_MSGAPI_ERR;
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

    try {
        mqtt::delivery_token_ptr pubtok = ch->publish(topic, payload, nbuf, 2, false); //  ch->get_config().qos, false);
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "...with token: %d\n", pubtok->get_message_id());
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "....for message with %d bytes\n", pubtok->get_message()->get_payload().size());
        pubtok->wait();  // wait_for(ch->get_config().timeout);
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "  ...OK\n");
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT publish() : Publish failed: %s\n", exc.what());
        return NVDS_MSGAPI_ERR;
    }

    return NVDS_MSGAPI_OK;
}

/////////////////////////////////////////////////////////////////////////////

/**
 * A derived action listener for async publish events.
 */
class delivery_action_listener : public virtual mqtt::iaction_listener {
    nvds_msgapi_send_cb_t _send_callback;
    void *_user_ptr;

    void on_failure(const mqtt::token &tok) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT publish failed for token: %d\n", tok.get_message_id());
        _send_callback(_user_ptr, NVDS_MSGAPI_ERR);
    }

    void on_success(const mqtt::token &tok) override {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "MQTT publish OK for token: %d\n", tok.get_message_id());
        _send_callback(_user_ptr, NVDS_MSGAPI_OK);
    }

public:
    delivery_action_listener(nvds_msgapi_send_cb_t send_callback, void *user_ptr) : _send_callback(send_callback),
                                                                                    _user_ptr(user_ptr) {}
};

NvDsMsgApiErrorType nvds_msgapi_send_async(NvDsMsgApiHandle h_ptr, char *topic, const uint8_t *payload, size_t nbuf,
                                           nvds_msgapi_send_cb_t send_callback, void *user_ptr) {
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

    if (!send_callback) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Send callback cannot be NULL. Async send failed\n");
        return NVDS_MSGAPI_ERR;
    }

    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "nvds_msgapi_send_async: payload=%.*s \n topic = %s\n", nbuf, payload, topic);

    try {
        mqtt::message_ptr pubmsg = mqtt::make_message(topic, (const char*)payload);
        pubmsg->set_qos(2); // ch->get_config().qos);
        delivery_action_listener deliveryListener(send_callback, user_ptr);
        ch->publish(pubmsg, nullptr, deliveryListener);
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT async publish() : Publish failed: %s\n", exc.what());
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
    auto msg = ch->consume_message();
}

NvDsMsgApiErrorType nvds_msgapi_disconnect(NvDsMsgApiHandle h_ptr) {
    if (!h_ptr) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "nvds_msgapi_disconnect called with null handle\n");
        return NVDS_MSGAPI_ERR;
    }

    async_client *ch = (async_client *) h_ptr;

    try {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "\nDisconnecting...\n");
        ch->disconnect()->wait();
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT disconnect() : Publish failed: %s\n", exc.what());
        return NVDS_MSGAPI_ERR;
    }
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "\n  ...OK\n");

    nvds_log_close();
    return NVDS_MSGAPI_OK;
}

/**
  * Returns version of API supported byh this adaptor
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
