#include <stdio.h>
#include <czmq.h>
#include <cv.h>

#define BITS_PER_PIXEL 8
#define WIDTH 160
#define HEIGHT 120
int main(int argc, char *argv[])
{
	//ff: frames file
	FILE *ff;
	//initializing IplImage
	IplImage *frame;
	//creating a point that will point to each segment of memory per frame
	void *fdata;
	//sizes of data
	size_t *bpp, *size;
	bbp	= BITS_PER_PIXEL;
	size = WIDTH*HEIGHT;

	zctx_t *context = zctx_new ();
	
	void *publisher = zsocket_new (context, ZMQ_PUB);
	
	if (argc == 2)
		zsocket_bind (publisher, argv [1]);
	else
		zsocket_bind (publisher, "tcp://*:5556");

	//  Ensure subscriber connection has time to complete
	sleep (1);

	ff=fopen(argv[1], "r");

	//creates image header but does not allocate memory for image
	frame = cvCreateImageHeader(size, IPL_DEPTH_8U, 1);

	while(true)
	{
		sleep(62.5);
		if(fread(fdata, bbp, size, ff) < size)
		{
			printf("finished reading file, going to the beginning \n");
			fseek(ff, SEEK_SET, 0);
		}

		zchunk_t *chunk = zchunk_new(fdata, sizeof(fdata));

		zframe_t *msg = zchunk_pack(chunk);
		
		zframe_send(&msg, publisher, 0);
	}
}
