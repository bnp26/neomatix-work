#include <zmq.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
	void *context, *subscriber;
	int rc;

	context = zmq_ctx_new ();
	subscriber = zmq_socket(context, ZMQ_SUB);

	rc = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
	
	if(rc != 0)
	{
		printf("could not set subscriber options\n");
		return -1;
	}

	do
	{	

		rc = zmq_connect (subscriber, "tcp://127.0.0.1:5556");
		printf("could not connect, trying again\n");
		sleep(1);

	}while(rc != 0);


	while(1)
	{
		zmq_msg_t msg;
		rc = zmq_msg_init(&msg);
		if(rc != 0)
		{
			printf("could not init msg\n");
			return -1;
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
	return 0;
}
