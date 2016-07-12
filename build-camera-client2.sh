gcc -Wall -ggdb camera-pipeline-tcp-client2.c -o camera-pipeline-tcp-client2 -lczmq -lzmq $(pkg-config --cflags --libs gstreamer-1.0  gstreamer-rtsp-1.0 gstreamer-rtsp-server-1.0 gdk-pixbuf-2.0)
