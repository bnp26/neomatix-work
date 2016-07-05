gcc -Wall camera-pipeline-client.c -o camera-pipeline-client -lczmq -lzmq $(pkg-config --cflags --libs gstreamer-1.0  gstreamer-rtsp-1.0 gstreamer-rtsp-server-1.0)
