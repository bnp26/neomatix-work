#include "zmq.h"
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>

#define BITS_PER_PIXEL 8
#define WIDTH 160
#define HEIGHT 120

int main(int argc, char *argv[])
{
	//ff: frames file
	FILE *ff;

	size_t bpp, size;
	zmq_msg_t msg;
	
	if(argc != 2)
	{
		printf("INCORRECT FORMAT!\n");
		printf("FORMAT:\t./camera-pipeline-server <file>\n");
	}

	bpp	= 8;
	size = 160*120;
	
	int64_t affinity;
	int rc;
	//  Ensure subscriber connection has time to complete
	
	void *context = zmq_ctx_new();
	void *publisher = zmq_socket(context, ZMQ_PUB);

	//setting the affinity and the thread configurations for the server.

	affinity = 1;
	rc = zmq_setsockopt (publisher, ZMQ_AFFINITY, &affinity, sizeof affinity);
	assert(rc == 0);	

	rc = zmq_bind(publisher, "tcp://*:5556");
	assert(rc == 0);
	printf("server socket bound to tcp://localhost:5556");

	ff=fopen(argv[1], "r");
	sleep(1);	
	time_t endtime;
	time_t runtime_len = 20;
	time_t curtime = time(NULL);
	
	endtime = curtime + runtime_len;
	
	int x = 0;
	//zchunk_t *chunk;
	//zframe_t *msg;
	uint8_t fdata [size];
	do
	{
		printf("reading data from file\n");
		usleep(1000);
		size_t dataSize = fread(fdata, 1, size, ff);
		
		printf("real data size = %zu\n", dataSize);

		rc = zmq_msg_init_size(&msg, dataSize);
		assert(rc == 0);
		
		memcpy(zmq_msg_data(&msg), fdata, dataSize);

		if(dataSize < size)
		{
			printf("finished reading file, going to the beginning \n");
			rewind(ff);
		}
		printf("read file, sending data.\n");	
		
		rc = zmq_msg_send (&msg, publisher, ZMQ_DONTWAIT);
		assert(rc == dataSize);

		zmq_msg_close(&msg);
		
		printf("sent frame #%i\n", x);
		time(&curtime);
		x+=1;
	}while(x < 20000);	
	printf("sent data for 120 seconds. That's enough... right???\n");
	return 0;
}
