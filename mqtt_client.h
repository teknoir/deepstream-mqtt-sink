#ifndef __NVDS_MQTT_CLIENT_H__
#define __NVDS_MQTT_CLIENT_H__

#include "mqtt/async_client.h"
#include "nvds_msgapi.h"

using namespace std;

#define CONFIG_MQTT_CLIENT_ID "client-id"
#define CONFIG_MQTT_TOPIC "topic"
#define CONFIG_MQTT_PERSIST_DIR "persist-dir"
#define CONFIG_MQTT_QOS "qos"
#define CONFIG_MQTT_TIMEOUT "timeout"
#define CONFIG_MQTT_RETRIES "retries"
#define CONFIG_MQTT_KEEP_ALIVE_INTERVAL "keep-alive-interval"
#define CONFIG_MQTT_CLEAN_SESSION "clean-session"

#define MAX_FIELD_LEN 1024
#define NVDS_MQTT_LOG_CAT "DSLOG:NVDS_MQTT_SINK"

/**
 * A subscribe action listener to handle subscriptions.
 */
class subscribe_listener : public virtual mqtt::iaction_listener {
    void on_failure(const mqtt::token &tok) override;
    void on_success(const mqtt::token &tok) override;
};

/**
 * A derived action listener for async publish events.
 */
class delivery_action_listener : public virtual mqtt::iaction_listener {
    nvds_msgapi_send_cb_t _send_callback;
    void *_user_ptr;

    void on_failure(const mqtt::token &tok) override;
    void on_success(const mqtt::token &tok) override;

public:
    delivery_action_listener(nvds_msgapi_send_cb_t send_callback, void *user_ptr) : _send_callback(send_callback),
                                                                                    _user_ptr(user_ptr) {}
};



/**
 * Local async client class to store config.
 * Inherits callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
*/
class async_client : public virtual mqtt::callback, public virtual mqtt::iaction_listener {
    mqtt::async_client* _cli;

    // Deepstream connection callback
    nvds_msgapi_connect_cb_t _connect_cb;

    string _client_id;
    string _persist_dir;
    int _qos;
    int _timeout;
    int _retries;
    int _keep_alive_interval;
    bool _clean_session;
    string _address;
    int _nretry;
    mqtt::connect_options _conn_opts;
    subscribe_listener _subscribe_listener;

    // Subscribe CB <topic, cb, user_ctx>
    vector<tuple<string, nvds_msgapi_subscribe_request_cb_t, void*> > _topic_arrived_vec;
    // Delivery CB <message_id, delivery_action_listener_ptr>
    vector<tuple<int, delivery_action_listener*> > _delivery_cb_vec;

public:
    async_client(nvds_msgapi_connect_cb_t connect_cb);

    bool read_config(char *config_path);
    bool set_connection_str(char *connection_str);
    bool connect();
    bool subscribe(char* topic, nvds_msgapi_subscribe_request_cb_t cb, void* user_ctx);
    bool send(mqtt::string_ref topic, const void* payload, size_t n);
    bool send_async(mqtt::string_ref topic, const void* payload, size_t n, nvds_msgapi_send_cb_t cb, void *user_ctx);
    void do_work();
    bool disconnect();

    // Callbacks

    void reconnect();
    // Re-connection failure
    void on_failure(const mqtt::token &tok) override;
    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token &tok) override;
    // (Re)connection success
    void connected(const std::string &cause) override;
    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string &cause) override;
    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override;
    // Callback for when a sent message is ack´ed.
    void delivery_complete(mqtt::delivery_token_ptr tok) override;

};

#endif
