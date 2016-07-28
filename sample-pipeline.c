// example appsrc for gstreamer 1.0 with own mainloop & external buffers. based on example from gstreamer docs.
// public domain, 2015 by Florian Echtler <floe@butterbrot.org>. compile with:
// gcc --std=c99 -Wall $(pkg-config --cflags gstreamer-1.0) -o gst gst.c $(pkg-config --libs gstreamer-1.0) -lgstapp-1.0

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <zmq.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>


typedef struct _Node Node;
struct _Node {
	gpointer data;
	int frame;
	guint64 duration;
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

uint8_t b_black[160*120];
uint8_t frame[160*120];

pthread_t zmq_thr;
sem_t mutex;

size_t peek(Queue * queue) 
{ 
    if(queue->head == NULL) 
    { 
        return -1; 
    } 
    return queue->head->size; 
} 
 
Queue * init_queue(void) 
{ 
	Queue * queue = (Queue*)malloc(sizeof(Queue)); 
	queue->head = NULL;
	queue->tail = NULL;
	queue->size = 0;
	return queue;
} 

Node * pop(Queue * queue)
{ 
    if(queue->head == NULL)  
    { 
        printf("Queue is Empty\n"); 
        return NULL; 
    } 
    
	Node *temp = (Node*)malloc(sizeof(Node)); 
	temp->data = queue->head->data;
	temp->size = queue->head->size;
	temp->frame = queue->head->frame;
	temp->duration = queue->head->duration;	
	temp->next = NULL;	
	
	if(queue->head->next == NULL)
	{
		queue->head = NULL;
		queue->tail = NULL;
	}
    else 
    { 
		queue->head = queue->head->next; 
    } 
	queue->size -= 1;
    return temp; 
}

Node * has_data(Queue * queue, gpointer newdata)
{
	if(queue->head == NULL)
	{
		return NULL;
	}
	Node *temp = queue->head;

	do{
		if(temp->data == newdata)
			return temp;
		else
			temp = temp->next;

	}while(temp->next != NULL);
	return NULL;
}


int push(Queue * queue, gpointer newdata, size_t size) 
{
	Node *temp, *tempcopy;
    if(newdata == NULL) 
    { 
        printf("data can't be null!\n"); 
        return FALSE; 
    } 
	
	temp = has_data(queue, newdata);	
	guint64 duration = gst_util_uint64_scale_int (1, GST_SECOND, 16);
	guint64 timestamp = duration;
	if(temp == NULL)
	{
		temp = (Node*)malloc(sizeof(Node)); 
		temp->data = newdata;
		temp->size = size;
		temp->duration = duration; 
		temp->frame = queue->tail->frame + 1;
		temp->next = NULL; 
	}
	else if(temp->data == b_black)
	{
		temp->frame = ;
		queue->time += temp->duration;
	}
	else
	{
		tempcopy = (Node*)malloc(sizeof(Node)); 
		tempcopy->data = temp->data;
		temp->size = temp->size;
		temp->duration = temp-duration; 
		temp->timestamp = queue->tail->timestamp + temp->duration;
		temp->next = NULL; 

	}
//--to limit max queue size--
/*
	else if(queue->size >= 32)
	{
		pop(queue);
		
	}
*/

	if(queue->head == NULL) 
    { 
        queue->head = temp; 
		queue->tail = temp;
    }
	else
	{
		queue->tail->next = temp;
		queue->tail = temp;
	}
	queue->size += 1;	
    return TRUE; 
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
	int black = TRUE;
	uint8_t * lastimage;
	size_t lastimagesize;
    while (1) {
        zmq_msg_t msg;
        rc = zmq_msg_init (&msg);
        if(rc == -1)
        {
            printf("could not init msg!\n");
            return NULL;
        }
        rc = zmq_msg_recv(&msg, subscriber, ZMQ_NOBLOCK);
		
		sem_wait(&mutex);
        if(rc == -1 && !black)
        {
			push(queue, lastimage, lastimagesize);
		}
		else if(rc == -1)
		{
			push(queue, b_black, 160*120);		
		}
        else
        {
			size_t datasize = zmq_msg_size(&msg);

			uint8_t *rec_data = (uint8_t *)zmq_msg_data(&msg);
			lastimage = rec_data;
			lastimagesize = datasize;
            push(queue, rec_data, datasize);
			black = FALSE;
        }
		sem_post(&mutex);
        rc = zmq_msg_close(&msg);
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

  GstBuffer *buffer;
  guint size;
  GstFlowReturn ret;

  if (!want) return;
  want = 0;

  size = 160 * 120;

	Node * pnode;
	uint8_t * data;
	guint64 timestamp;
	guint64 duration;
	size_t nnodesize;

	sem_wait(&mutex);
	pnode = pop(queue);
	sem_post(&mutex);
		
	if(pnode != NULL)
	{
		nnodesize = pnode->size;
		data = pnode->data;
		timestamp = pnode->timestamp;
		duration = pnode->duration;
		printf("peek has data of size: %zu\n", nnodesize);
	}
	else
	{
		printf("peek has no data :(\n");
	}
  buffer = gst_buffer_new_wrapped_full( 0, (gpointer)data, size, 0, size, NULL, NULL );
	free(pnode);

  GST_BUFFER_PTS (buffer) = timestamp;
  GST_BUFFER_DURATION (buffer) = duration;

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
	 b_black[i] = 0;
}
	sem_init(&mutex, 0, 1);
  gst_controller_zmq_thr_creat(queue);
  sleep(1);
  /* init GStreamer */
  gst_init (&argc, &argv);

  /* setup pipeline */
  pipeline = gst_pipeline_new ("pipeline");
  appsrc = gst_element_factory_make ("appsrc", "source");
  conv = gst_element_factory_make ("videoconvert", "conv");
//  enc = gst_element_factory_make ("jpegenc", "jpgenc");
  videosink = gst_element_factory_make ("xvimagesink", "videosink");

  /* setup */
  g_object_set (G_OBJECT (appsrc), "caps",
  		gst_caps_new_simple ("video/x-raw",
				     "format", G_TYPE_STRING, "GRAY8",
				     "width", G_TYPE_INT, 160,
				     "height", G_TYPE_INT, 120,
				     "framerate", GST_TYPE_FRACTION, 16, 1,
				     NULL), NULL);
  gst_bin_add_many (GST_BIN (pipeline), appsrc, conv, videosink, NULL);
  gst_element_link_many (appsrc, conv, videosink, NULL);

  /* setup appsrc */
  g_object_set (G_OBJECT (appsrc),
		"stream-type", 0, // GST_APP_STREAM_TYPE_STREAM
		"format", GST_FORMAT_TIME,
    "is-live", TRUE,
	"max-bytes", 19200,
	"block", TRUE,
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
	sem_destroy(&mutex);
  return 0;
}
