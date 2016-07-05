#include "zmq.h"
#include <stdio.h>
#include <unistd.h>

#define BITS_PER_PIXEL 8
#define WIDTH 160
#define HEIGHT 120

int main(int argc, char *argv[])
{
	//ff: frames file
	FILE *ff;
	//creating a point that will point to each segment of memory per frame
	void *fdata = NULL;
	//sizes of data
	size_t bpp, size;
	bpp	= BITS_PER_PIXEL;
	size = WIDTH*HEIGHT;
	//zsock_t *publisher = zsock_new_pub("@tcp://localhost:5556");
	//  Ensure subscriber connection has time to complete
	void *context = zmq_ctx_new();
	void *publisher = zmq_socket(context, ZMQ_PUB);
	int rc = zmq_bind(publisher, "tcp://*:5556");
	if(rc != 0)
	{	
		printf("could not bind to tcp://*:5556");
		return -1;
	}
	ff=fopen(argv[1], "r");

	int x = 0;
	
	//zchunk_t *chunk;
	//zframe_t *msg;
	while(x < 1000)
	{
		printf("sleeping 62.5 miliseconds \n");
		sleep(0.0625);
		printf("reading data from file\n");
		size_t dataSize = fread(fdata, bpp, size, ff);
		if(dataSize < size)
		{
			printf("finished reading file, going to the beginning \n");
			fseek(ff, SEEK_SET, 0);
		}
		printf("read file, sending data.");	
		
		zmq_send(publisher, fdata, dataSize, 0);

		//chunk = zchunk_new(fdata, sizeof(fdata));

		//msg = zchunk_pack(chunk);
	
		//zsock_send(publisher, "i124488zsbcfUhp", msg);	

		printf("sent frame #%i\n", x);
		x+=1;
	}	
	printf("sent 1000 frames. That's enough.");
	//zsock_destroy(&publisher);
	//zchunk_destroy(&chunk);
	//zframe_destroy(&msg);
	return 0;
}
