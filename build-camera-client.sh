gcc -Wall -g -ggdb camera-pipeline-tcp-client.c -o camera-pipeline-tcp-client -lpthread -lzmq $(pkg-config --cflags gstreamer-1.0 --libs gstreamer-1.0)
