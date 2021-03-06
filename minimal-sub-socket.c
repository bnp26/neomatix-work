#include <zmq.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

pthread_t zmq_thr;

void gst_controller_zmq_thr_creat(void);
void *zmq_thread(void *);

int main (int argc, char *argv[])
{
	gst_controller_zmq_thr_creat();

	
	void *retval;
	pthread_join(zmq_thr, &retval);
	pthread_exit(retval);	

	return 0;
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

}

void* zmq_thread(void *data)
{
	void *context, *subscriber;
	int rc;

	context = zmq_ctx_new ();
	subscriber = zmq_socket(context, ZMQ_SUB);


	do
	{	

		rc = zmq_connect (subscriber, "tcp://127.0.0.1:5556");
		printf("could not connect, trying again\n");
		sleep(1);

	}while(rc != 0);

	rc = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
	
	if(rc != 0)
	{
		printf("could not set subscriber options\n");
		return NULL;
	}

	while(1)
	{
		zmq_msg_t msg;
		rc = zmq_msg_init(&msg);
		if(rc != 0)
		{
			printf("could not init msg\n");
			return NULL;
		}

		rc = zmq_msg_recv(&msg, subscriber, 0);

		if(rc == -1)
		{
			printf("didn't recieve a message\n");
		}
		else
		{
			printf("supposedly recieved data.\n");
			uint8_t *data = zmq_msg_data(&msg);
			printf("printing first 3 pieces of data: %02x:%02x:%02x\n", data[0], data[1], data[2]);
		}

		zmq_msg_close(&msg);
	}

	zmq_close(&subscriber);
	zmq_term(&context);
	return data;
}
