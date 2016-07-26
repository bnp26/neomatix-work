# neomatix-work
The work I'm doing for neomatix
## Build of simple-pipeline
gcc -Wall -g -ggdb sample-pipeline.c -o sample-pipeline $(pkg-config --cflags gstreamer-1.0  gstreamer-plugins-base-1.0 --libs gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-app-1.0) -lpthread -lzmq -lgstvideo-1.0 -lgstapp-1.0
