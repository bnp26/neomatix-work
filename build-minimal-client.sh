gcc -Wall -g -ggdb camera-pipeline-minimal-client.c -o camera-pipeline-minimal-client $(pkg-config --cflags gstreamer-1.0  gstreamer-plugins-base-1.0 --libs gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-app-1.0) -lpthread -lzmq -lgstvideo-1.0
