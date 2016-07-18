#include <zmq.h>
#include <assert.h>
#include <gst/gst.h>
#include <gst/video/video.h>
//#include <gst/app/gstappsrc.h>
#include <stdlib.h>
#include <pthread.h>
#include <memory.h>
#include <unistd.h>

static GMainLoop *loop;

typedef struct _Frame Frame;

struct _Frame {
	guint sourceid;
	uint8_t *data;
	int *frameNum;
	GstClockTime timestamp;
};

Frame *frameCtx;

pthread_t zmq_thr;

void* zmq_thread(void *data)
{
	void *context;
	void *subscriber;
	int rc;

	context = zmq_ctx_new();
	subscriber = zmq_socket(context, ZMQ_SUB);
	
	rc = zmq_connect (subscriber, "tcp://127.0.0.1:5556");
	
	if(rc != -1)
		printf("connected to tcp://127.0.0.1:5556\n");
	else
	{
		printf("failed to connect to server\n");
		return NULL;
	}
	
	rc = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
	
	if(rc != -1)
		printf("subscribed to client\n");
	else
	{
		printf("could not subscribe to client\n");
		return NULL;
	}
	
	while (1) {
		zmq_msg_t msg;
		zmq_msg_init (&msg);
		rc = zmq_msg_recv (subscriber, &msg, 0);
		
		if(rc != -1)
		{	
			printf("recieved a message from the server!\n");

			frameCtx->data = zmq_msg_data(&msg);
			int h,w;
			printf("printing message byte style\n");
			for(h = 0; h < 120; h++)
			{
				for(w = 0; w < 160; w++)
				{
					printf("[0x%x]", frameCtx->data[(h*160)+w]);
				}
				printf("\n");
			}
		}
		rc = zmq_msg_close(&msg);
	}

	zmq_close (subscriber);
	zmq_ctx_destroy (context);
	return NULL;
}

void gst_controller_zmq_thr_creat(void)
{
	int ret;
	pthread_t zmq_thr;	

	ret = pthread_create(&zmq_thr, NULL, zmq_thread, NULL);
	if(ret != 0)
	{
		printf("could not create new thread!\n");
		return;
	}
}

static gboolean need_data (GstElement * appsrc, guint size, Frame * frame)
{

	GstBuffer *buffer;
	GstFlowReturn ret;
		
	buffer = gst_buffer_new_wrapped((gpointer)frame->data, sizeof(frame->data));

	GST_BUFFER_PTS (buffer) = frame->timestamp;	
	GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 16);
	frame->timestamp += GST_BUFFER_DURATION(buffer);
	frame->frameNum += 1;
	
	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);	

	if (ret != GST_FLOW_OK)
	{
		g_main_loop_quit (loop);
	}
	return TRUE;
}                              

int main(int argc, char *argv[])
{
	int major, minor, patch;
	zmq_version (&major, &minor, &patch);
	GstElement *pipeline, *source, *parser, *converter, *encoder, /* *payloader,*/ *sink;
	GstElementFactory *parser_factory;

  	frameCtx = g_new0(Frame, 1);
	frameCtx->data = malloc(160 * 120);
	frameCtx->frameNum = 0;
	frameCtx->timestamp = 0;
	gst_controller_zmq_thr_creat();	

	gst_init (&argc, &argv);
	loop = g_main_loop_new(NULL, FALSE);

	/* create gstreamer elements */
	//gst-launch-1.0 videotestsrc ! videoparse ! queue ! videoconvert ! x264enc ! udpsink host=127.0.0.1 port=8000
	pipeline = gst_pipeline_new("camera-one-video-player");	
	source = gst_element_factory_make ("appsrc", "app-src");
	converter = gst_element_factory_make("videoconvert","video-converter");
	encoder = gst_element_factory_make("x264enc", "h264-encoder");
	/*payloader = gst_element_factory_make("rtph264pay", "h264-payloader");*/
	sink = gst_element_factory_make("tcpserversink", "tcp-sink");
	
	if (!pipeline || !source || !converter || !encoder /*|| !payloader*/ || !sink) {
		g_printerr ("Some elements could not be created. Exiting.\n");
		return -1;
	}


	g_object_set(G_OBJECT (source),
		"stream-type", 0,
		"format", GST_FORMAT_TIME,
		NULL);
	
	g_object_set (G_OBJECT (converter),
		"format",GST_VIDEO_FORMAT_GRAY8,
		"framerate", GST_TYPE_FRACTION, 1, 16,
		"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
		"width", 160,
		"height", 120,
		NULL);

	g_object_set(	G_OBJECT (sink),
		"host", "localhost",
		"port", 5004,
		NULL);	

	
	gst_bin_add_many (GST_BIN (pipeline), source, parser, converter, encoder, /*payloader,*/ sink, NULL);
	
	gst_element_link_many (source, parser, converter, encoder, /*payloader,*/ sink, NULL);
	

	g_printerr("configured data, frameNum, timestamp, context, and the subscriber socket\n");

	g_printerr ("Current 0MQ version is %d.%d.%d\n", major, minor, patch);

	g_signal_connect (source, "need-data", G_CALLBACK (need_data), frameCtx);
	
	/*play */
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	g_printerr("PLAY");
	g_main_loop_run (loop);
	
	/*stop playing*/
	gst_element_set_state (pipeline, GST_STATE_NULL);
	g_printerr("STOP");

	/* clean up */
	
	void *retval;
	pthread_join(zmq_thr, &retval);
	pthread_exit(retval);
	free(frameCtx->data);
	gst_object_unref (GST_OBJECT (source));
	gst_object_unref (GST_OBJECT (parser));
	gst_object_unref (GST_OBJECT (converter));
	gst_object_unref (GST_OBJECT (encoder));
	/*gst_object_unref (GST_OBJECT (payloader));*/
	gst_object_unref (GST_OBJECT (sink));
	gst_object_unref (GST_OBJECT (pipeline));

	g_main_loop_unref (loop);

	return 0;
}

