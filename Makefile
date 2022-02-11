# this Makefile is to be used to build the mqtt_proto protocol adaptor .so
CXX:=g++-9

PKGS:= glib-2.0 
DS_INC:=/opt/nvidia/deepstream/deepstream/sources/includes
DS_LIB:=/opt/nvidia/deepstream/deepstream/lib

SRCS:=  libnvds_mqtt_sink.cpp mqtt_client.cpp json_helper.cpp nvds_utils.cpp
TARGET_LIB:= libnvds_mqtt_sink.so
TARGET_DEBUG_LIB:= libnvds_mqtt_sink_d.so

CFLAGS:= -fPIC -Wall -std=c++17
CFLAGS+= $(shell pkg-config --cflags $(PKGS))

LIBS:= $(shell pkg-config --libs $(PKGS))
LDFLAGS:= -shared

INC_PATHS:= -I $(DS_INC)
CFLAGS+= $(INC_PATHS)

LIBPATHS = -L /usr/local/lib -L $(DS_LIB)
LIBS+= -l:libpaho-mqttpp3.a -l:libpaho-mqtt3as.a -lssl -ljansson -lnvds_logger -lpthread -lcrypto -lstdc++

SYNC_SEND_SRCS:=test_mqtt_sink_sync.cpp
ASYNC_SEND_SRCS:=test_mqtt_sink_async.cpp
SYNC_SEND_BIN:= test_mqtt_sink_sync
ASYNC_SEND_BIN:= test_mqtt_sink_async

CXXFLAGS:= -I$(DS_INC) -rdynamic -std=c++17
LDFLAGS_TEST:= -L$(DS_LIB) -ldl -Wl,-rpath=$(DS_LIB)

all: $(TARGET_LIB) $(TARGET_DEBUG_LIB) $(SYNC_SEND_BIN) $(ASYNC_SEND_BIN)

$(TARGET_DEBUG_LIB) : $(SRCS)
	$(CC) -g -o $@ $^ $(CFLAGS) $(LDFLAGS) $(INC_PATHS) $(LIBPATHS) $(LIBS)

$(TARGET_LIB) : $(SRCS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(INC_PATHS) $(LIBPATHS) $(LIBS)

$(SYNC_SEND_BIN) : $(SYNC_SEND_SRCS)
	$(CXX) -g -o $@ $^  $(CXXFLAGS) $(LDFLAGS_TEST)

$(ASYNC_SEND_BIN) : $(ASYNC_SEND_SRCS)
	$(CXX) -g -o $@ $^  $(CXXFLAGS) $(LDFLAGS_TEST)

clean:
	rm -rf $(TARGET_LIB) $(TARGET_DEBUG_LIB) $(SYNC_SEND_BIN) $(ASYNC_SEND_BIN)

