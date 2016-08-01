# neomatix-work
The work I'm doing for neomatix

## The Project

1. First created a zmq SUB socket that can connect to a zmq PUB socket and recieve incoming 160 x 120 8bit grayscale images.
Sees these images as raw byte data
2. Created a gstreamer pipeline that parses, encodes, and ultimately pushes the data into a rtp stream.

## Build of simple-pipeline
`gcc -Wall -g -ggdb camera-rtp-client.c -o camera-rtp-client $(pkg-config --cflags gstreamer-1.0  gstreamer-plugins-base-1.0 --libs gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-app-1.0) -lpthread -lzmq -lgstvideo-1.0 -lgstapp-1.0`
