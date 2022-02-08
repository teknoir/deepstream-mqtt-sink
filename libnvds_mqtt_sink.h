#ifndef __NVDS_MQTT_PROTO_H__
#define __NVDS_MQTT_PROTO_H__

#define NVDS_MSGAPI_VERSION "2.0"
#define NVDS_MSGAPI_PROTOCOL "MQTT"

#define CONFIG_MQTT_CLIENT_ID "client-id"
#define CONFIG_MQTT_TOPIC "topic"
#define CONFIG_MQTT_PERSIST_DIR "persist-dir"
#define CONFIG_MQTT_QOS "qos"
#define CONFIG_MQTT_TIMEOUT "timeout"
#define CONFIG_MQTT_RETRIES "retries"
#define CONFIG_MQTT_KEEP_ALIVE_INTERVAL "keep-alive-interval"
#define CONFIG_MQTT_CLEAN_SESSION "clean-session"

#define MAX_FIELD_LEN 1024
#define NVDS_MQTT_LOG_CAT "DSLOG:NVDS_MQTT_PROTO"

#endif
