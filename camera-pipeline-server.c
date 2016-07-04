#include "zmq.h"
#include "czmq.h"
#include <stdio.h>

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
	size_t bpp, size, size_bytes;
	bpp	= BITS_PER_PIXEL;
	size = WIDTH*HEIGHT;
	size_bytes = size*bpp;
	void *publisher = zsock_new_pub("tcp://*:5556");

	//  Ensure subscriber connection has time to complete
	sleep (1);

	ff=fopen(argv[1], "r");

	int x = 0;
	
	while(true)
	{
		sleep(62.5);
		
		if(fread(fdata, bpp, size, ff) < size)
		{
			printf("finished reading file, going to the beginning \n");
			fseek(ff, SEEK_SET, 0);
		}
	
		zchunk_t *chunk = zchunk_new(fdata, sizeof(fdata));

		zframe_t *msg = zchunk_pack(chunk);
		
		zframe_send(&msg, publisher, 0);

		printf("sent frame #%i\n", x);
	
		if(x == 1000)
		{
			printf("sent 1000 frames. That's enough.");
			zsock_destroy(&publisher);
			zchunk_destroy(&chunk);
			zframe_destroy(&msg);
			return 0;
		}
	}
}
