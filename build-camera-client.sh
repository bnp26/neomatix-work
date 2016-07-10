gcc -Wall camera-pipeline-tcp-client.c -o camera-pipeline-tcp-client -lczmq -lzmq $(pkg-config --cflags --libs gstreamer-1.0  gstreamer-rtsp-1.0 gstreamer-rtsp-server-1.0)
