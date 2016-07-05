#include <zmq.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp/gstrtspurl.h>

#define CAPS "video/x-raw,format=,width=160,height=120,pixel-aspect-ratio=1/1" 

#define CHUNK_SIZE 153600

typedef struct _Frame Frame;

struct _Frame {
	byte *data;
	int *frameNum;
	GstClockTime timestamp;
};

static gboolean                                                                                               
need_data (GstElement * appsrc, guint size, Frame * frame)                                                        
{                                                                                                             
    GstBuffer *buffer;                                                                                        
    guint len;                                                                                                
    GstFlowReturn ret;                                                                                        
    
    len = CHUNK_SIZE;                                                                                         
	void *context = zmq_ctx_new ();
	void *subscriber = zmq_socket (context, ZMQ_SUB);
	int rc = zmq_connect (subscriber, "tcp://localhost:5556");
	zframe_t *recvFrame = zframe_new_empty();
	
	zchunk_t *recvChunk;
	// setting subscriber socket.	
	zsock_t *subscriber = zsock_new_sub(">tcp://localhost:5556", "sub0");	

	sleep(1);
	
	if(zsock_recv(subscriber, "i124488zsbcfUhp", &recvFrame) != 0)
		printf("did not recieve frame from server\n");
	else
		printf("recieved frame from server\n");

	if(recvFrame !=  NULL)
	{
		recvChunk = zchunk_unpack(recvFrame); 
		frame->data = zchunk_data(recvChunk);
	}
	
	gst_buffer_new_wrapped((gpointer)frame->data, sizeof(frame->data));

	GST_BUFFER_PTS (buffer) = frame->timestamp;	
	GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 16);
	frame->timestamp += GST_BUFFER_DURATION(buffer);

	frame->frameNum += 1;

    g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);                                         
    gst_buffer_unref (buffer);                                                                                
    
	zchunk_destroy(&recvChunk);
	zframe_destroy(&recvFrame);	

	if (ret != GST_FLOW_OK) 
	{                                                                                 
		/* some error, stop sending data */                                                                   
		return FALSE;                                                                                         
	}                                                                                                         

	return TRUE;                                                                                              
}                              

/* called when a new media pipeline is constructed. We can query the
 * pipeline and configure our appsrc */

static void media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
	GstElement *element, *appsrc;
	Frame *frameCtx;

	/* get the element used for providing the streams of the media */
	element = gst_rtsp_media_get_element (media);

	/* get our appsrc, we named it 'mysrc' with the name property */
	appsrc = gst_bin_get_by_name_recurse_up (GST_BIN (element), "app-src");

	/* this instructs appsrc that we will be dealing with timed buffer */
	gst_util_set_object_arg (G_OBJECT (appsrc), "format", "bytes");
	/* configure the caps of the video */
  	g_object_set (G_OBJECT (appsrc), "caps",
		gst_caps_new_simple ("video/x-raw",
			"format", G_TYPE_STRING, "GRAY8",
			"framerate", GST_TYPE_FRACTION, 16, 1,
			"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
			"width", G_TYPE_INT, 160,
			"height", G_TYPE_INT, 120,
			NULL));
  	frameCtx = g_new0 (Frame, 1);
  	frameCtx->data = NULL;
	frameCtx->frameNum = 0;
	frameCtx->timestamp = 0;	
	/* make sure ther datais freed when the media is gone */
  	g_object_set_data_full (G_OBJECT (media), "my-extra-data", frameCtx, (GDestroyNotify) g_free);

	/* install the callback that will be called when a buffer is needed */
 	g_signal_connect (appsrc, "need-data", (GCallback) need_data, frameCtx);
	gst_object_unref (appsrc);
	gst_object_unref (element);
}

int main(int argc, char *argv[])
{
	GMainLoop *loop;

	GstRTSPServer *server;
	GstRTSPMountPoints *mounts;
	GstRTSPMediaFactory *factory;
	GstRTSPMedia *media;

	//GstElement *pipeline, *source, *parser, *queue, *converter, *encoder, *payloader, *sink;
	
	gst_init (&argc, &argv);
	loop = g_main_loop_new(NULL, FALSE);

	/* create gstreamer elements */
	/*pipeline = gst_pipeline_new("camera-one-video-player");	
	source = gst_element_factory_make ("appsrc", "app-src");
	parser = gst_element_factory_make ("videoparse", "video-parser");
	queue = gst_element_factory_make("queue", "queue");
	converter = gst_element_factory_make("videoconvert","video-converter");
	encoder = gst_element_factory_make("x264enc", "h264-encoder");
	payloader = gst_element_factory_make("rtph264pay", "h264-payloader");
	sink = gst_element_factory_make("udpsink", "rtsp-sink");
	
	
	if (!pipeline || !source || !parser || !queue || !converter || !encoder || !payloader || !sink || !factory) {
		g_printerr ("Some elements could not be created. Exiting.\n");
		return -1;
	}
	
	gst_bin_add_many (GST_BIN (pipeline), source, parser, queue, converter, encoder, payloader, sink, NULL);
	
	g_printerr("added elements to pipeline\n");
	
	gst_element_link_many (source, parser, queue, converter, encoder, payloader, sink, NULL);

	g_printerr("linked elements in pipeline\n");
	*/
	
	server = gst_rtsp_server_new();
	g_printerr("created new rtsp server.\n");

	mounts = gst_rtsp_server_get_mount_points (server);
	g_printerr("recieved mount points.\n");	

	factory = gst_rtsp_media_factory_new();
	g_printerr("created new factory.\n");

	gst_rtsp_media_factory_set_launch (factory,
	      "( appsrc name=app-src ! videoparse ! queue ! videoconvert ! x264enc ! rtph264pay name=pay0 pt=96 )");
	g_printerr("launched new pipeline.\n");

	g_signal_connect (factory, "media-configure", (GCallback) media_configure, NULL);
	g_printerr("connected callback from factory to media_configure.\n");

	gst_rtsp_server_set_address (server, "127.0.0.1"); 
	gst_rtsp_server_set_service (server, "8554"); 
	gst_rtsp_mount_points_add_factory (mounts, "/media", factory);
	g_object_unref(mounts);	
	gst_rtsp_server_attach (server, NULL);
	
	g_print("stream ready at rtsp://127.0.0.1:8554/media.\n");
	g_main_loop_run(loop);
	return 0;
}

