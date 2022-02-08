#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "nvds_msgapi.h"

/* MODIFY: to reflect your own path */
#define SO_PATH "./"  // "/opt/nvidia/deepstream/deepstream/lib/"
#define MQTT_PROTO_SO "libnvds_mqtt_sink.so"
#define MQTT_PROTO_PATH SO_PATH MQTT_PROTO_SO
#define MQTT_CFG_FILE "./cfg.txt"
//connection string format: host;port
#define MQTT_CONNECT_STR "localhost;31883"
#define MAX_LEN 256

void sample_msgapi_connect_cb(NvDsMsgApiHandle *h_ptr, NvDsMsgApiEventType ds_evt) {}

int g_cb_count = 0;
int consumed_cnt = 0;

void test_send_cb(void *user_ptr, NvDsMsgApiErrorType completion_flag) {
    // printf("async send complete (from test_send_cb)\n");
    if (completion_flag == NVDS_MSGAPI_OK)
        printf("%s successfully \n", (char *) user_ptr);
    else
        printf("%s with failure\n", (char *) user_ptr);
    g_cb_count++;
}

void subscribe_cb(NvDsMsgApiErrorType flag, void *msg, int len, char *topic, void *user_ptr) {
    int *ptr = (int *) user_ptr;
    if (flag == NVDS_MSGAPI_ERR) {
        printf("Error in consuming message[%d] from mqtt broker\n", *ptr);
    } else {
        printf("Consuming message[%d], on topic[%s]. Payload= %.*s\n", *ptr, topic, len, (const char *) msg);
    }
    consumed_cnt++;
}

int main() {
    NvDsMsgApiHandle conn_handle;
    NvDsMsgApiHandle(*msgapi_connect_ptr)(char * connection_str, nvds_msgapi_connect_cb_t
    connect_cb, char * config_path);
    NvDsMsgApiErrorType(*msgapi_send_ptr)(NvDsMsgApiHandle
    conn, char * topic,
    const uint8_t *payload, size_t
    nbuf);
    NvDsMsgApiErrorType(*msgapi_send_async_ptr)(NvDsMsgApiHandle
    h_ptr, char * topic,
    const uint8_t *payload, size_t
    nbuf, nvds_msgapi_send_cb_t
    send_callback, void * user_ptr);
    NvDsMsgApiErrorType(*msgapi_subscribe_ptr)(NvDsMsgApiHandle
    conn, char * *topics, int
    num_topics, nvds_msgapi_subscribe_request_cb_t
    cb, void * user_ctx);
    void (*msgapi_do_work_ptr)(NvDsMsgApiHandle h_ptr);
    NvDsMsgApiErrorType(*msgapi_disconnect_ptr)(NvDsMsgApiHandle
    h_ptr);
    char *(*msgapi_getversion_ptr)(void);
    char *(*msgapi_get_protocol_name_ptr)(void);
    NvDsMsgApiErrorType(*msgapi_connection_signature_ptr)(char * connection_str, char * config_path, char * output_str, int max_len);

    void *so_handle = dlopen(MQTT_PROTO_PATH, RTLD_LAZY);
    char *error;
    const char SEND_MSG[] = "{ \
   \"messageid\" : \"84a3a0ad-7eb8-49a2-9aa7-104ded6764d0_c788ea9efa50\", \
   \"mdsversion\" : \"1.0\", \
   \"@timestamp\" : \"\", \
   \"place\" : { \
    \"id\" : \"1\", \
    \"name\" : \"HQ\", \
    \"type\" : \"building/garage\", \
    \"location\" : { \
      \"lat\" : 0, \
      \"lon\" : 0, \
      \"alt\" : 0 \
    }, \
    \"aisle\" : { \
      \"id\" : \"C_126_135\", \
      \"name\" : \"Lane 1\", \
      \"level\" : \"P1\", \
      \"coordinate\" : { \
        \"x\" : 1, \
        \"y\" : 2, \
        \"z\" : 3 \
      } \
     }\
    },\
   \"sensor\" : { \
    \"id\" : \"10_110_126_135_A0\", \
    \"type\" : \"Camera\", \
    \"description\" : \"Aisle Camera\", \
    \"location\" : { \
      \"lat\" : 0, \
      \"lon\" : 0, \
      \"alt\" : 0 \
    }, \
    \"coordinate\" : { \
      \"x\" : 0, \
      \"y\" : 0, \
      \"z\" : 0 \
     } \
    } \
   }";


    char display_str[5][100];
    printf("Refer to nvds log file for log output\n");

    for (int i = 0; i < 5; i++)
        snprintf(&(display_str[i][0]), 100, "Async send [%d] complete", i);

    if (!so_handle) {
        error = dlerror();
        fprintf(stderr, "%s\n", error);

        printf("unable to open shared library\n");
        exit(-1);
    }

    *(void **) (&msgapi_connect_ptr) = dlsym(so_handle, "nvds_msgapi_connect");
    *(void **) (&msgapi_send_async_ptr) = dlsym(so_handle, "nvds_msgapi_send_async");
    *(void **) (&msgapi_subscribe_ptr) = dlsym(so_handle, "nvds_msgapi_subscribe");
    *(void **) (&msgapi_disconnect_ptr) = dlsym(so_handle, "nvds_msgapi_disconnect");
    *(void **) (&msgapi_do_work_ptr) = dlsym(so_handle, "nvds_msgapi_do_work");
    *(void **) (&msgapi_getversion_ptr) = dlsym(so_handle, "nvds_msgapi_getversion");
    *(void **) (&msgapi_get_protocol_name_ptr) = dlsym(so_handle, "nvds_msgapi_get_protocol_name");
    *(void **) (&msgapi_connection_signature_ptr) = dlsym(so_handle, "nvds_msgapi_connection_signature");

    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "%s\n", error);
        exit(-1);
    }
    printf("Adapter protocol=%s , version=%s\n", msgapi_get_protocol_name_ptr(), msgapi_getversion_ptr());

    char query_conn_signature[MAX_LEN];
    if (msgapi_connection_signature_ptr((char *) MQTT_CONNECT_STR, (char *) MQTT_CFG_FILE, query_conn_signature,
                                        MAX_LEN) != NVDS_MSGAPI_OK) {
        printf("Error querying connection signature string\n");
    }
    printf("connection signature queried= %s\n", query_conn_signature);

    // set mqtt broker appropriately
    conn_handle = msgapi_connect_ptr((char *) MQTT_CONNECT_STR, (nvds_msgapi_connect_cb_t) sample_msgapi_connect_cb,
                                     (char *) MQTT_CFG_FILE);
    if (!conn_handle) {
        printf("Connect failed. Exiting\n");
        exit(-1);
    }

    //Subscribe to topics
    const char *topics[] = {"topic1", "topic2"};
    int num_topics = 2;
    if (msgapi_subscribe_ptr(conn_handle, (char **) topics, num_topics, subscribe_cb, &consumed_cnt) !=
        NVDS_MSGAPI_OK) {
        printf("MQTT subscription to topic[s] failed. Exiting \n");
        exit(-1);
    }
    for (int i = 0; i < 5; i++) {
        if (msgapi_send_async_ptr(conn_handle, (char *) "topic1", (const uint8_t *) SEND_MSG, strlen(SEND_MSG),
                                  test_send_cb, &(display_str[i][0])) != NVDS_MSGAPI_OK)
            printf("asend [%d] failed\n", i);
        else
            printf("sending [%d] asynchronously\n", i);
    }

    printf("before do work\n");
    msgapi_do_work_ptr(conn_handle); // need to continuously call do_work to process callbacks
    printf("do work\n");
    while (g_cb_count < 5) {
        sleep(1);
        msgapi_do_work_ptr(conn_handle); // need to continuously call do_work to process callbacks
        printf("do work\n");
    }
    printf("Disconnecting... in 3 secs\n");
    sleep(3);
    msgapi_disconnect_ptr(conn_handle);
}
