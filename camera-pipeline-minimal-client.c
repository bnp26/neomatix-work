#include <zmq.h>
#include <assert.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <memory.h>
#include <unistd.h>

#define WIDTH 160
#define HEIGHT 120

static GMainLoop *loop;

typedef struct _Frame Frame;
struct _Frame {
	GstElement *appsrc;

	guint sourceid;
	uint8_t *data;
	guint64 offset;
	size_t size;
	int *frameNum;
	GstClockTime timestamp;
};

typedef struct _Node Node;
struct _Node {
	uint8_t data [WIDTH*HEIGHT];
	size_t size;
	struct _Node * next;
};

Frame *frameCtx;
Node *head, *tail;

pthread_t zmq_thr;

size_t peek()
{
	if(head == NULL)
	{
		return -1;
	}
	return head->size;
}

void init_queue(void)
{
	head = NULL;
	tail = NULL;
}

int push(uint8_t * newdata, size_t size)
{
	if(newdata == NULL)
	{
		printf("data can't be null!\n");
		return FALSE;
	}

	Node *temp = (Node*)malloc(sizeof(Node));
	int x;	
	for(x = 0; x < size; x++)
	{
		temp->data[x] = newdata[x];
	}	
	temp->size = size;
	temp->next = NULL;

	if(head == NULL && tail == NULL)
	{
		head = tail = temp;
		return TRUE;
	}

	tail->next = temp;
	tail = temp;
	return TRUE;
}

Node * pop(void)
{
	if(head == NULL) 
	{
		printf("Queue is Empty\n");
		return NULL;
	}
	Node *temp = head;

	if(head == tail)
		head = tail = NULL;
	else
	{
		head = head->next;
	}
	return temp;
}

void* zmq_thread(void *data)
{
	void *context, *subscriber;
	int rc;

	context = zmq_ctx_new();
	subscriber = zmq_socket(context, ZMQ_SUB);
	rc = zmq_connect (subscriber, "tcp://127.0.0.1:5556");
	if(rc == 0)
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
		rc = zmq_msg_init (&msg);
		if(rc == -1)
		{
			printf("could not init msg!\n");
			return NULL;
		}
		rc = zmq_msg_recv(&msg, subscriber, 0);
		
		size_t datasize = zmq_msg_size(&msg);
		
		uint8_t *rec_data = (uint8_t *)zmq_msg_data(&msg);
		
		if(rc == -1)
		{	
			printf("didn't recieve any data, sleeping for a second.\n");
		}
		else
		{
			push(rec_data, datasize);
		}
		rc = zmq_msg_close(&msg);
	}
	
	printf("about to close\n");

	zmq_close (subscriber);
	zmq_ctx_destroy (context);
	return NULL;
}

void gst_controller_zmq_thr_creat(void)
{
	int ret;

	pthread_attr_t attr;

	pthread_attr_init(&attr);

	ret = pthread_create(&zmq_thr, &attr, zmq_thread, NULL);
	if(ret != 0)
	{
		printf("could not create new thread!\n");
		return;
	}

	pthread_attr_destroy(&attr);
	return;
}


static gboolean read_data (Frame *frame)
{
	GstBuffer *buffer;
	GstFlowReturn ret;
	Node * pnode;
	size_t psize;
	uint8_t * pdata;
	//popped data
	//printf("pointer to data in queue: %p", pdata);
	
	g_printerr("current appsrc level of bytes = %" G_GUINT64_FORMAT "\n", gst_app_src_get_current_level_bytes(GST_APP_SRC(frame->appsrc)));	

	buffer = gst_buffer_new ();
		
	psize = (size_t)peek();
	if(psize == -1)
	{
		g_printerr("no nodes in queue\n");
		return TRUE;
	}
	if(frame->size < psize)
		gst_buffer_memset(buffer, frame->offset, 0, frame->size);
	else
	{
		pnode = pop();
		pdata = pnode->data;
		gst_buffer_fill(buffer, frame->offset, pdata, psize);
	}
	frame->offset += frame->size;

	GST_BUFFER_PTS (buffer) = frame->timestamp;	
	GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 16);
	frame->timestamp += GST_BUFFER_DURATION(buffer);

	g_signal_emit_by_name (frame->appsrc, "push-buffer", buffer, &ret);

	if (ret != GST_FLOW_OK)
	{
		g_main_loop_quit (loop);
	}
	return TRUE;
}                              

static void start_feed (GstElement * pipeline, guint size, Frame *frame)
{
    if (frame->sourceid == 0) {
		g_printerr("starting feed\n");
		frame->size = size;
        frame->sourceid = g_idle_add ((GSourceFunc) read_data, frame);
    }
}

static void stop_feed (GstElement * pipeline, Frame *frame)
{
    if (frame->sourceid != 0) {
        g_source_remove (frame->sourceid);
        frame->sourceid = 0;
		g_printerr("stopping feed\n");
    }
}
int main(int argc, char *argv[])
{
	int major, minor, patch;
	zmq_version (&major, &minor, &patch);
	GstElement *pipeline, *source, *converter, *sink;
	GstAppSrc *appsrc;
	GstCaps *filtercaps;

  	frameCtx = g_new0(Frame, 1);
	frameCtx->data = malloc(WIDTH * HEIGHT);
	frameCtx->frameNum = 0;
	frameCtx->timestamp = 0;
	frameCtx->offset = 0;

	init_queue();
	gst_controller_zmq_thr_creat();	

	gst_init (&argc, &argv);
	loop = g_main_loop_new(NULL, TRUE);

	// create gstreamer elements 
	//gst-launch-1.0 videotestsrc ! videoparse ! queue ! videoconvert ! x264enc ! udpsink host=127.0.0.1 port=8000
	pipeline = gst_pipeline_new("camera-one-video-player");	
	source = gst_element_factory_make ("appsrc", "app-src");
	converter = gst_element_factory_make ("videoconvert", "converter"); 
	encoder = gst_element_factory_make ("jpegenc", "jpgencoder");
	sink = gst_element_factory_make("xvimagesink", "sink");
	frameCtx->appsrc = source;
	if (!pipeline || !source || !converter || !encoder || !sink) {
		g_printerr ("Some elements could not be created. Exiting.\n");
		return -1;
	}
	
	filtercaps = gst_caps_new_simple ("video/x-raw",
			   "format", G_TYPE_STRING, "GRAY8",
			   "width", G_TYPE_INT, 160,
			   "height", G_TYPE_INT, 120,
			   "framerate", GST_TYPE_FRACTION, 0, 1,
			   NULL);

	appsrc = GST_APP_SRC(source);
	
	gst_app_src_set_caps (appsrc, filtercaps);
	gst_app_src_set_size (appsrc, WIDTH*HEIGHT*20);
	gst_app_src_set_stream_type (appsrc, GST_APP_STREAM_TYPE_STREAM);
	gst_app_src_set_latency (appsrc, -1,10000);
	gst_caps_unref (filtercaps);

	gst_bin_add_many (GST_BIN (pipeline), source, converter, sink, NULL);
	
	gst_element_link_many (source, converter, sink, NULL);
	
	g_printerr("configured data, frameNum, timestamp, context, and the subscriber socket\n");

	g_printerr ("Current 0MQ version is %d.%d.%d\n", major, minor, patch);

	g_signal_connect(source, "need-data", G_CALLBACK(start_feed), frameCtx);
 	g_signal_connect(source, "enough-data", G_CALLBACK(stop_feed), frameCtx);
	
	//play
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	g_printerr("PLAY");
	g_main_loop_run (loop);
	
	//stop playing
	gst_element_set_state (pipeline, GST_STATE_NULL);
	g_printerr("STOP");

	// clean up 
	
	void *retval;
	pthread_join(zmq_thr, &retval);
	pthread_exit(retval);
	free(frameCtx->data);
	gst_object_unref (GST_OBJECT (source));
	gst_object_unref (GST_OBJECT (converter));
	gst_object_unref (GST_OBJECT (sink));
	gst_object_unref (GST_OBJECT (pipeline));

	g_main_loop_unref (loop);

	return 0;
}

