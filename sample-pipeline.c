// example appsrc for gstreamer 1.0 with own mainloop & external buffers. based on example from gstreamer docs.
// public domain, 2015 by Florian Echtler <floe@butterbrot.org>. compile with:
// gcc --std=c99 -Wall $(pkg-config --cflags gstreamer-1.0) -o gst gst.c $(pkg-config --libs gstreamer-1.0) -lgstapp-1.0

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <zmq.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

typedef struct _Node Node;
struct _Node {
    gpointer data;
    size_t size;
    struct _Node * next;
};

typedef struct _Queue Queue;
struct _Queue {
	Node *head;
	Node *tail;
	int size;
};

int want = 1;

uint8_t b_white[160*120];
uint8_t b_black[160*120];
uint8_t frame[160*120];

pthread_t zmq_thr;

size_t peek(Queue * queue) 
{ 
    if(queue->head == NULL) 
    { 
        return -1; 
    } 
    return (&queue->head)->size; 
} 
 
Queue * init_queue(void) 
{ 
	Queue * queue = (Queue*)malloc(sizeof(Queue)); 
	queue->head = NULL;
	queue->tail = NULL;
	queue->size = 0;
	return queue;
} 
 
int push(Queue * queue, gpointer newdata, size_t size) 
{ 
    if(newdata == NULL) 
    { 
        printf("data can't be null!\n"); 
        return FALSE; 
    } 
 
    Node *temp = (Node*)malloc(sizeof(Node)); 
    temp->size = size; 
    temp->data = newdata;
	temp->next = NULL; 

    if(queue->head == NULL) 
    { 
        *queue->head	= *temp; 
		*queue->tail = *temp;
    } 
	else
	{
		*queue->tail->next = *temp;
		queue->tail = &temp;
	}
	
	printf("pushing newdata up of size %zu\n", temp->size);
    return TRUE; 
} 
 
Node * pop(Queue * queue)
{ 
    if(queue->head == NULL)  
    { 
        printf("Queue is Empty\n"); 
        return NULL; 
    } 
    Node *temp = &queue->head; 
 
    if(queue->head == queue->tail) 
	{
		queue->head = NULL; 
		queue->tail = NULL;
		printf("popped last Node\n");
	}
    else 
    { 
        queue->head = &queue->head->next; 
		printf("popped a node\n");
    } 
    return temp; 
}

void* zmq_thread(void *data)
{
    void *context, *subscriber;
    int rc;

	Queue *queue = (Queue *)data;

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
        rc = zmq_msg_recv(&msg, subscriber, ZMQ_NOBLOCK);

        if(rc == -1)
        {
			push(queue, b_white, 160*120);		
			printf("pushed white array\n");
		}
        else
        {
			size_t datasize = zmq_msg_size(&msg);

			uint8_t *rec_data = (uint8_t *)zmq_msg_data(&msg);

            push(queue, rec_data, datasize);
        }
        rc = zmq_msg_close(&msg);
		usleep(1/16);
    }

    printf("about to close\n");

    zmq_close (subscriber);
    zmq_ctx_destroy (context);
    return NULL;
}

void gst_controller_zmq_thr_creat(Queue * queue)
{
    int ret;

    pthread_attr_t attr; 
 
    pthread_attr_init(&attr); 
 
    ret = pthread_create(&zmq_thr, &attr, zmq_thread, queue); 
    if(ret != 0) 
    { 
        printf("could not create new thread!\n"); 
        return; 
    } 
 
    pthread_attr_destroy(&attr); 
    return; 
} 


static void prepare_buffer(GstAppSrc* appsrc, Queue * queue) {

  static gboolean white = FALSE;
  static GstClockTime timestamp = 0;
  GstBuffer *buffer;
  guint size;
  GstFlowReturn ret;

  if (!want) return;
  want = 0;

  size = 160 * 120;

	Node * pnode;
	uint8_t * data;
	
	size_t nnodesize = peek(queue);
		
	if(nnodesize != -1)
		printf("peek has data of size: %zu\n", nnodesize);
	else
	{
		printf("peek has no data :(\n");
		return;
	}
	pnode = pop(queue);
	data = pnode->data;
  buffer = gst_buffer_new_wrapped_full( 0, (gpointer)data, size, 0, size, NULL, NULL );
	free(pnode);
  white = !white;

  GST_BUFFER_PTS (buffer) = timestamp;
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 16);


  timestamp += GST_BUFFER_DURATION (buffer);

  ret = gst_app_src_push_buffer(appsrc, buffer);

  if (ret != GST_FLOW_OK) {
    /* something wrong, stop pushing */
    //g_main_loop_quit (loop);
  }
}

static void cb_need_data (GstElement *appsrc, guint unused_size, gpointer user_data) {
 prepare_buffer((GstAppSrc*)appsrc, (Queue *)user_data);
	want = 1;
}

gint main (gint argc, gchar *argv[]) {

  GstElement *pipeline, *appsrc, *conv, *videosink;
int i;
Queue *queue = init_queue();  
for (i = 0; i < 160*120; i++) {
	 if(i%2 == 0)
	 {
		 b_black[i] = 0;
		 b_white[i] = 0xFF;
	 }
	 else
	 {

		b_black[i] = 0xFF; b_white[i] = 0; 
	 }
  }

  gst_controller_zmq_thr_creat(queue);
  sleep(2);
  /* init GStreamer */
  gst_init (&argc, &argv);

  /* setup pipeline */
  pipeline = gst_pipeline_new ("pipeline");
  appsrc = gst_element_factory_make ("appsrc", "source");
  conv = gst_element_factory_make ("videoconvert", "conv");
  videosink = gst_element_factory_make ("xvimagesink", "videosink");

  /* setup */
  g_object_set (G_OBJECT (appsrc), "caps",
  		gst_caps_new_simple ("video/x-raw",
				     "format", G_TYPE_STRING, "GRAY8",
				     "width", G_TYPE_INT, 160,
				     "height", G_TYPE_INT, 120,
				     "framerate", GST_TYPE_FRACTION, 0, 1,
				     NULL), NULL);
  gst_bin_add_many (GST_BIN (pipeline), appsrc, conv, videosink, NULL);
  gst_element_link_many (appsrc, conv, videosink, NULL);

  /* setup appsrc */
  g_object_set (G_OBJECT (appsrc),
		"stream-type", 0, // GST_APP_STREAM_TYPE_STREAM
		"format", GST_FORMAT_TIME,
    "is-live", TRUE,
	"max-bytes", 19200,
    NULL);
  g_signal_connect (appsrc, "need-data", G_CALLBACK (cb_need_data), queue);

  /* play */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  
  while (1) {
    prepare_buffer((GstAppSrc*)appsrc, queue);
    g_main_context_iteration(g_main_context_default(),FALSE);
  }

  /* clean up */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));

  return 0;
}