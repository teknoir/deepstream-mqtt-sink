# MQTT Sink for NVIDIA DeepStream by Teknoir
This project creates a sink for NVIDIA DeepStream SDK to pipe data to MQTT broker.
The adaptor implements a MQTT message format for client applications to publish inference results or metadata.

## Pre-requisites
* A Jetson device with DeepStream SDK installed
* Gstreamer installation as described in the [NVIDIA documentation](https://docs.nvidia.com/jetson/l4t/index.html#page/Tegra%20Linux%20Driver%20Package%20Development%20Guide/accelerated_gstreamer.html)

### More libraries
paho.mqtt.c
paho.mqtt.cpp
libjansson-dev



rsync -v -r -d . jetson@jetsonnano-b00.local:~/git/deepstream-mqtt-sink/ && ssh -t jetson@jetsonnano-b00.local "cd ~/git/deepstream-mqtt-sink; make; make -f Makefile.test; ./test_mqtt_sink_async"

https://github.com/eclipse/paho.mqtt.c/blob/master/src/samples/MQTTAsync_publish.c
https://github.com/marcoslucianops/DeepStream-Yolo
https://catalog.ngc.nvidia.com/orgs/nvidia/containers/deepstream-l4t
https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_ref_app_deepstream.html
