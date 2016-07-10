#include <zmq.h>
#include <assert.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp/gstrtspurl.h>

#define CAPS "video/x-raw,format=,width=160,height=120,pixel-aspect-ratio=1/1" 

#define CHUNK_SIZE 153600

typedef struct _Frame Frame;

struct _Frame {
	void *data;
	int *frameNum;
	GstClockTime timestamp;
	void *context;
	void *subscriber;
};

Frame *frameCtx;

static gboolean need_data (GstElement * appsrc, guint size, Frame * frame)
{
	GstBuffer *buffer;
	GstFlowReturn ret;
	zmq_msg_t msg;
	int rc;
	guint len = CHUNK_SIZE;
	
	rc = zmq_msg_init (&msg);
	assert (rc == 0);

	rc = zmq_msg_recv(&msg, frame->subscriber, 0);
	
	if(rc != -1)
	{
		frame->data = zmq_msg_data(&msg);
	}

	rc = zmq_msg_close(&msg);
	assert(rc == 0);

	gst_buffer_new_wrapped((gpointer)frame->data, sizeof(frame->data));

	GST_BUFFER_PTS (buffer) = frame->timestamp;	
	GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 16);
	frame->timestamp += GST_BUFFER_DURATION(buffer);
	frame->frameNum += 1;

	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
	gst_buffer_unref (buffer);

	if (ret != GST_FLOW_OK)
	{
		return FALSE;
	}
	
	return TRUE;
}                              

int main(int argc, char *argv[])
{
	GMainLoop *loop;

	GstElement *pipeline, *source, *parser, *queue, *converter, *encoder, *payloader, *sink;
	
	int rc;

	gst_init (&argc, &argv);
	loop = g_main_loop_new(NULL, FALSE);

	/* create gstreamer elements */
	pipeline = gst_pipeline_new("camera-one-video-player");	
	source = gst_element_factory_make ("appsrc", "app-src");
	parser = gst_element_factory_make ("videoparse", "video-parser");
	queue = gst_element_factory_make("queue", "queue");
	converter = gst_element_factory_make("videoconvert","video-converter");
	encoder = gst_element_factory_make("x264enc", "h264-encoder");
	payloader = gst_element_factory_make("rtph264pay", "h264-payloader");
	sink = gst_element_factory_make("tcpserversink", "tcp-server-sink");
	
	
	if (!pipeline || !source || !parser || !queue || !converter || !encoder || !payloader || !sink) {
		g_printerr ("Some elements could not be created. Exiting.\n");
		return -1;
	}
	
	g_object_set(G_OBJECT (sink), 
		"host", "localhost",
		"port", 8556, NULL);	

	g_object_set (G_OBJECT (source), 
		"stream-type", GST_APP_STREAM_TYPE_STREAM, 
		"format", GST_FORMAT_TIME, 
		"caps", gst_caps_new_simple ("video/x-raw",
			"format", G_TYPE_STRING, "GRAY8",
			"framerate", GST_TYPE_FRACTION, 16, 1,
			"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
			"width", G_TYPE_INT, 160,
			"height", G_TYPE_INT, 120,
		NULL), NULL);

	gst_bin_add_many (GST_BIN (pipeline), source, parser, queue, converter, encoder, payloader, sink, NULL);
	
	g_printerr("added elements to pipeline\n");
	
	gst_element_link_many (source, parser, queue, converter, encoder, payloader, sink, NULL);

	g_printerr("linked elements in pipeline\n");

  	frameCtx = g_new0(Frame, 1);
  	frameCtx->data = NULL;
	frameCtx->frameNum = 0;
	frameCtx->timestamp = 0;	
	frameCtx->context = zmq_ctx_new();
	frameCtx->subscriber = zmq_socket(frameCtx->context, ZMQ_SUB);
	
	g_printerr("configured data, frameNum, timestamp, context, and the subscriber socket\n");

	// configuring the subscriber socket's sockoptions
	rc = zmq_setsockopt (frameCtx->subscriber, ZMQ_SUBSCRIBE, "", 0);
	assert (rc == 0);

	g_printerr("set up subscriber socket options and is about to try to connect to the server\n");

	// connecting to the server
	rc = zmq_connect (frameCtx->subscriber, "tcp://localhost:5456");
	assert(rc == 0);
	printf("connected to tcp://localhost:5456");

	g_signal_connect (source, "need-data", G_CALLBACK (need_data), NULL);
	
	/*play */
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	g_main_loop_run (loop);

	/* clean up */
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (source));
	gst_object_unref (GST_OBJECT (parser));
	gst_object_unref (GST_OBJECT (queue));
	gst_object_unref (GST_OBJECT (converter));
	gst_object_unref (GST_OBJECT (encoder));
	gst_object_unref (GST_OBJECT (payloader));
	gst_object_unref (GST_OBJECT (sink));
	gst_object_unref (GST_OBJECT (pipeline));

	g_main_loop_unref (loop);

	return 0;
}

