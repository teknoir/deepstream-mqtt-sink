# this Makefile is to be used to build the mqtt_proto protocol adaptor .so
CXX:=g++

PKGS:= glib-2.0 
DS_INC:=/opt/nvidia/deepstream/deepstream/sources/includes
DS_LIB:=/opt/nvidia/deepstream/deepstream/lib
SRCS:=  libnvds_mqtt_sink.cpp mqtt_client.cpp json_helper.cpp nvds_utils.cpp
TARGET_LIB:= libnvds_mqtt_sink.so

CFLAGS:= -fPIC -Wall

CFLAGS+= $(shell pkg-config --cflags $(PKGS))

LIBS:= $(shell pkg-config --libs $(PKGS))
LDFLAGS:= -shared

INC_PATHS:= -I $(DS_INC)
CFLAGS+= $(INC_PATHS)

LIBPATHS = -L $(DS_LIB)
LIBS+= -l:libpaho-mqttpp3.a -l:libpaho-mqtt3as.a -lssl -ljansson -lnvds_logger -lpthread -lcrypto -lstdc++

all: $(TARGET_LIB)

$(TARGET_LIB) : $(SRCS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(INC_PATHS) $(LIBPATHS) $(LIBS)

clean:
	rm -rf $(TARGET_LIB)

