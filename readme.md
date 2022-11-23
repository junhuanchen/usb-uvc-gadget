> it is uvcgadget not libuvc.

# UVC gadget test application

uvcgadget is a pure C library that implements handling of UVC gadget functions.

use board https://wiki.sipeed.com/m3axpi

> h264 need update kernel.

## usage onboard(m3axpi)

### config

- `./uvc-gadget.sh start` >>> create /dev/video0

```
	create_frame $FUNCTION 640 360 uncompressed u
	create_frame $FUNCTION 1280 720 uncompressed u
	create_frame $FUNCTION 1280 720 mjpeg m
	create_frame $FUNCTION 1920 1080 mjpeg m
```

### shell

`gcc main.c -lpthread -o uvc-gadget && ./uvc-gadget`

and change buffer && buflen, use `./uvc-gadget -d /dev/video0` >>> pc will find this usb webcam.

```c

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

```

on board

```bash
open succeeded, file descriptor = 3
device is usb-ss-gadget on bus gadget
uvc_fill_streaming_control: bFormatIndex 1,bFrameIndex 1
uvc_fill_streaming_control: bFormatIndex 1,bFrameIndex 1
bRequestType a1 bRequest 86 wValue 0200 wIndex 0100 wLength 0001
control request (req 86 cs 02)
bRequestType a1 bRequest 86 wValue 0200 wIndex 0200 wLength 0001
control request (req 86 cs 02)
bRequestType a1 bRequest 87 wValue 0100 wIndex 0001 wLength 001a
streaming request (req 87 cs 01)
uvc_fill_streaming_control: bFormatIndex 1,bFrameIndex 1
bRequestType 21 bRequest 01 wValue 0100 wIndex 0001 wLength 001a
streaming request (req 01 cs 01)
setting probe control, length = 26

ctrl->bFormatIndex = 1 ,ctrl->bFrameIndex =1,iformat 1,iframe 1,*interval 333333
DEBUG: V4L2_PIX_FMT_YUYV
bRequestType a1 bRequest 81 wValue 0100 wIndex 0001 wLength 001a
streaming request (req 81 cs 01)
bRequestType a1 bRequest 87 wValue 0200 wIndex 0200 wLength 0002
control request (req 87 cs 02)
bRequestType a1 bRequest 82 wValue 0200 wIndex 0200 wLength 0002
control request (req 82 cs 02)
bRequestType a1 bRequest 83 wValue 0200 wIndex 0200 wLength 0002
control request (req 83 cs 02)
bRequestType a1 bRequest 84 wValue 0200 wIndex 0200 wLength 0002
control request (req 84 cs 02)
bRequestType a1 bRequest 87 wValue 0200 wIndex 0100 wLength 0001
control request (req 87 cs 02)
bRequestType a1 bRequest 84 wValue 0200 wIndex 0100 wLength 0001
control request (req 84 cs 02)
```

### onyourpc(ffmpeg or potplayer)

> hotplug UVC may cause chip reboots, which should be individual hardware bugs or code bug ?

```
juwan@juwan-n85-dls:~$ ls /dev/video
video0  video1
juwan@juwan-n85-dls:~$ ls /dev/video
video0  video1  video2  video3
juwan@juwan-n85-dls:~$ ls /dev/video
video0  video1  video2  video3
```

- `ffmpeg -hide_banner -f v4l2 -list_formats all -i /dev/video2`

```
juwan@juwan-n85-dls:~$ ffmpeg -hide_banner -f v4l2 -list_formats all -i /dev/video2
[video4linux2,v4l2 @ 0x55e30290f6c0] Raw       :     yuyv422 :           YUYV 4:2:2 : 640x360 1280x720
[video4linux2,v4l2 @ 0x55e30290f6c0] Compressed:       mjpeg :          Motion-JPEG : 1280x720 1920x1080
/dev/video2: Immediate exit requested
```

- `ffplay -pixel_format yuyv422 -video_size 640x360 -i /dev/video2`

https://user-images.githubusercontent.com/32978053/202128673-1a5e747f-296e-430f-a007-7df63ef6233f.mp4

- `ffplay -pixel_format mjpeg -video_size 1280x720 -i /dev/video2`

https://user-images.githubusercontent.com/32978053/202128660-26b006ab-f088-4d7f-ae46-71accb861bb3.mp4
