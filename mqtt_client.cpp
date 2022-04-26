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
#include "mqtt_client.h"


void subscribe_listener::on_failure(const mqtt::token &tok) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "MQTT Subscribe failed\n");
    auto top = tok.get_topics();
    if (top && !top->empty())
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT Subscribe error: topic[%s]\n", (*top)[0].c_str());
    else
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT Subscribe error: unknown topic\n");
}

void subscribe_listener::on_success(const mqtt::token &tok) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "MQTT Subscribed\n");
    auto top = tok.get_topics();
    if (top && !top->empty())
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "MQTT Subscribed: topic[%s],\n", (*top)[0].c_str());
}

void mqtt_send_complete::ack(NvDsMsgApiErrorType result_code) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "nvds_msgapi_send_cb_t send callback invoked\n");
    _send_cb(_user_ctx, result_code);
}

void delivery_action_listener::on_failure(const mqtt::token &tok) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT publish failed for token: %d\n", tok.get_message_id());
    mqtt_send_complete* msc = static_cast<mqtt_send_complete*>(tok.get_user_context());
    thread t(msc->_send_cb, msc->_user_ctx, NVDS_MSGAPI_ERR);
    t.detach();
}

void delivery_action_listener::on_success(const mqtt::token &tok) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "MQTT publish OK for token: %d\n", tok.get_message_id());
    mqtt_send_complete* msc = static_cast<mqtt_send_complete*>(tok.get_user_context());
    thread t(msc->_send_cb, msc->_user_ctx, NVDS_MSGAPI_OK);
    t.detach();
}


async_client::async_client(nvds_msgapi_connect_cb_t connect_cb) :
        _cli(NULL),
        _connect_cb(connect_cb),
        _client_id("deepstream-mqtt-app"),
        _persist_dir("./persist"),
        _qos(2),
        _timeout(10),
        _retries(5),
        _keep_alive_interval(20),
        _clean_session(false),
        _address("tcp://localhost:1883"),
        _nretry(0) {

}

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
bool async_client::read_config(char *config_path) {
    char client_id[MAX_FIELD_LEN];
    char persist_dir[MAX_FIELD_LEN];
    char qos[MAX_FIELD_LEN];
    char timeout[MAX_FIELD_LEN];
    char retries[MAX_FIELD_LEN];
    char keep_alive_interval[MAX_FIELD_LEN];
    char clean_session[MAX_FIELD_LEN];

    if (fetch_config_value(config_path, CONFIG_MQTT_CLIENT_ID, client_id, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "client_id");
        return false;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_PERSIST_DIR, persist_dir, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "persist_dir");
        return false;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_QOS, qos, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "qos");
        return false;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_TIMEOUT, timeout, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "timeout");
        return false;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_RETRIES, retries, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "retries");
        return false;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_KEEP_ALIVE_INTERVAL, keep_alive_interval, MAX_FIELD_LEN, NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "keep_alive_interval");
        return false;
    }

    if (fetch_config_value(config_path, CONFIG_MQTT_CLEAN_SESSION, clean_session, MAX_FIELD_LEN,NVDS_MQTT_LOG_CAT) != NVDS_MSGAPI_OK) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT config parsing failed. Failed on key: %s\n", "clean_session");
        return false;
    }

    _client_id = client_id;
    _persist_dir = persist_dir;
    _qos = atoi(qos);
    _timeout = atoi(timeout);
    _retries = atoi(retries);
    _keep_alive_interval = atoi(keep_alive_interval);
    _clean_session = atoi(clean_session);

    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "client_id", _client_id.c_str());
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %s\n", "persist_dir", _persist_dir.c_str());
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %d\n", "qos", _qos);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %d\n", "timeout", _timeout);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %d\n", "retries", _retries);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %d\n", "keep_alive_interval", _keep_alive_interval);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_INFO, "key: %s, value: %d\n", "clean_session", _clean_session);

    return true;
}


/**
 * Validate mqtt connection string format
 * Valid format host;port or (host;port;topic to support backward compatibility)
 */
bool async_client::set_connection_str(char *connection_str) {
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

    string url;
    string port;
    istringstream iss(connection_str);
    getline(iss, url, ';');
    getline(iss, port, ';');

    if (url == "" || port == "") {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT connection string is invalid. hostname or port is empty\n");
        return false;
    }

    _address = url + ":" + port;
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "key: %s, value: %s\n", "address", _address.c_str());

    return true;
}

bool async_client::connect() {

    if (_cli != NULL) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT async client already initialized\n");
        return false;
    }
    lock_guard<mutex> g(_lock);

    _cli = new(nothrow) mqtt::async_client(_address, _client_id);

    _conn_opts = mqtt::connect_options_builder()
            .clean_session(_clean_session)
            .keep_alive_interval(chrono::seconds(_keep_alive_interval))
            .finalize();

    _cli->set_callback(*this);

    try {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "\nConnecting...\n");
        mqtt::token_ptr conntok = _cli->connect(_conn_opts, nullptr, *this);
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Waiting for the connection...\n");
        conntok->wait();
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "  ...OK\n");
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT Connection failed: %s \n", exc.what());
        return false;
    }
    
    return true;
}

bool async_client::subscribe(char* topic, nvds_msgapi_subscribe_request_cb_t cb, void* user_ctx) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "\nSubscribing to topic: %s ...\n", topic);

    lock_guard<mutex> g(_lock);
    try {
        mqtt::token_ptr subtok = _cli->subscribe(topic, _qos, nullptr, _subscribe_listener);
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Waiting for subscription...\n");
        subtok->wait();
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "  ...OK\n");
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT subscribe() : Subscribe failed: %s\n", exc.what());
        return false;
    }

    _topic_arrived_vec.push_back({ topic, cb, user_ctx });

    return true;
}

bool async_client::send(mqtt::string_ref topic, const void* payload, size_t n) {
    /**
     * Looks like this is not used in gst-nvmsgbroker that is the middleware gstreamer<--->deepstream
     */
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "SEND QoS%d on topic = %s\n", _qos, topic.c_str());
    lock_guard<mutex> g(_lock);
    try {
        mqtt::delivery_token_ptr pubtok = _cli->publish(topic, payload, n, _qos, false); //  ch->get_config().qos, false);
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "...with token: %d\n", pubtok->get_message_id());
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "....for message with %d bytes\n", pubtok->get_message()->get_payload().size());
        pubtok->wait_for(_timeout);
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "  ...OK\n");
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT publish() : Publish failed: %s\n", exc.what());
        return false;
    }
    return true;
}

bool async_client::send_async(mqtt::string_ref topic, const void* payload, size_t n, nvds_msgapi_send_cb_t cb, void *user_ctx) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "SEND ASYNC QoS%d on topic = %s\n", _qos, topic.c_str());

    mqtt::delivery_token_ptr pubtok;
    mqtt::message_ptr pubmsg = mqtt::make_message(topic, (const char*)payload, n);
    pubmsg->set_qos(_qos);

    lock_guard<mutex> g(_lock);
        try {
            mqtt_send_complete* ctx = new mqtt_send_complete(cb, user_ctx);
            pubtok = _cli->publish(pubmsg, ctx, _delivery_action_listener);
        }
        catch (const mqtt::exception &exc) {
            nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT async publish(), QoS%d : Publish failed: %s\n", _qos, exc.what());
            return false;
        }

    return true;
}

void async_client::do_work() {
    lock_guard<mutex> g(_lock);
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "MQTT receive topic vector size: %d\n", _topic_arrived_vec.size());
}

bool async_client::disconnect(){
    lock_guard<mutex> g(_lock);
    try {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "\nDisconnecting...\n");
        _cli->disconnect()->wait();
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "MQTT disconnect() : Disconnect failed: %s\n", exc.what());
        return false;
    }
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "\n  ...OK\n");
    return true;
}

void async_client::reconnect() {
    this_thread::sleep_for(chrono::milliseconds(2500));
    lock_guard<mutex> g(_lock);
    try {
        _cli->connect(_conn_opts, nullptr, *this);
    }
    catch (const mqtt::exception &exc) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Client %s lost connection and failed to reconnect\n", _client_id.c_str());
        _connect_cb((NvDsMsgApiHandle) _cli, NVDS_MSGAPI_EVT_DISCONNECT);
    }
}

// Re-connection failure
void async_client::on_failure(const mqtt::token &tok) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "(Re)Connection attempt failed for client %s\n", _client_id.c_str());
    if (++_nretry > _retries) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_ERR, "Client %s service down\n", _client_id.c_str());
        _connect_cb((NvDsMsgApiHandle) _cli, NVDS_MSGAPI_EVT_SERVICE_DOWN);
    }
    reconnect();
}

// (Re)connection success
// Either this or connected() can be used for callbacks.
void async_client::on_success(const mqtt::token &tok) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Connection success for client %s\n", _client_id.c_str());
}

// (Re)connection success
void async_client::connected(const string &cause) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Client %s is connected\n", _client_id.c_str());
}

// Callback for when the connection is lost.
// This will initiate the attempt to manually reconnect.
void async_client::connection_lost(const string &cause) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_WARNING, "Client %s lost connection: %s\n", _client_id.c_str(), cause.c_str());
    _nretry = 0;
    reconnect();
}

// Callback for when a message arrives.
void async_client::message_arrived(mqtt::const_message_ptr msg) {
    nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "MQTT message arrived: payload= %s \n topic = %s\n", msg->to_string().c_str(), msg->get_topic().c_str());

    // Find correct callback and user context and invoke it
    auto it = find_if(_topic_arrived_vec.begin(), _topic_arrived_vec.end(), [msg](const tuple<string, nvds_msgapi_subscribe_request_cb_t, void*>& e) {return get<0>(e) == msg->get_topic();});
    if (it != _topic_arrived_vec.end()) {
        nvds_log(NVDS_MQTT_LOG_CAT, LOG_DEBUG, "Matching callback for: topic = %s\n", get<0>(*it).c_str());
        get<1>(*it)(NVDS_MSGAPI_OK, (void*)msg->to_string().c_str(), (int)msg->to_string().length(), (char*)get<0>(*it).c_str(), get<2>(*it));
    }
}