gcc -Wall -g -ggdb camera-rtp-client.c -o camera-rtp-client $(pkg-config --cflags gstreamer-1.0 gstreamer-plugins-base-1.0 --libs gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-app-1.0) -lpthread -lzmq -lgstvideo-1.0 -lgstapp-1.0

gcc -Wall -g -ggdb camera-rtp-local-client.c -o camera-rtp-local-client $(pkg-config --cflags gstreamer-1.0 gstreamer-plugins-base-1.0 --libs gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-app-1.0) -lpthread -lzmq -lgstvideo-1.0 -lgstapp-1.0
