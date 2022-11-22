// gcc ./main.c -lpthread

#include "uvc-gadget.h"

int uvc_start(struct uvc_thread *uvc);
int uvc_stop(struct uvc_thread *uvc);

static void uvc_video_fill_buffer(struct uvc_device *dev, struct v4l2_buffer *buf)
{
    int bufpos = buf->index;
    char *buffer = dev->mem[bufpos];
    int *buflen = &buf->bytesused;
	// printf("bufpos %d \r\n", bufpos);

	switch (dev->fcc)
	{
	case V4L2_PIX_FMT_YUYV:
	{
		/* Fill the buffer with video data. */
		unsigned int bpl = dev->width * 2;
		for (unsigned int i = 0; i < dev->height; ++i)
		{
			memset(buffer + i * bpl, dev->color++, bpl);
		}
		*buflen = bpl * dev->height;
		break;
	}
	case V4L2_PIX_FMT_MJPEG:
	{
		const char *jpgs[] = {"./images/1.jpg", "./images/2.jpg", "./images/1.jpg", "./images/3.jpg"};
		int fd = open(jpgs[bufpos], O_RDONLY);
		if (fd > 0)
		{
			*buflen = lseek(fd, 0, SEEK_END);
			lseek(fd, 0, SEEK_SET);
			read(fd, buffer, *buflen);
			close(fd);
		}
        if (*buflen > dev->jpg_max) {
            dev->jpg_max = *buflen;
        }
		break;
	}
	}
	// dump_mem_s(dev->mem[buf->index]);
}

// gcc ./main.c -lpthread
int main()
{
    struct uvc_thread uvc;
    uvc_start(&uvc);
    for (int i = 0; i < 120; i++)
    {
        sleep(1);
        printf("time:%ld\r\n", time(NULL));
    }
    uvc_stop(&uvc);
    return 0;
}