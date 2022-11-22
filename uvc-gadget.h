/*
 * UVC gadget test application
 *
 * Copyright (C) 2010 Ideas on board SPRL <laurent.pinchart@ideasonboard.com>
 * Copyright (C) 2022 junhuanchen on board m3axpi <junhuanchen@qq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 */

#ifndef _DLS_UVC_GADGET_H_
#define _DLS_UVC_GADGET_H_

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <linux/usb/ch9.h>
#include "uvc.h"
#include "video.h"
#include <linux/videodev2.h>
#include <sys/time.h>
#include <signal.h>

#define clamp(val, min, max) ({                 \
        typeof(val) __val = (val);              \
        typeof(min) __min = (min);              \
        typeof(max) __max = (max);              \
        (void) (&__val == &__min);              \
        (void) (&__val == &__max);              \
        __val = __val < __min ? __min: __val;   \
        __val > __max ? __max: __val; })

#define ARRAY_SIZE(a) ((sizeof(a) / sizeof(a[0])))

#define BUF_CNT 4
#define MAX_PACKET 1024

struct uvc_device
{
	int fd;

	struct uvc_streaming_control probe;
	struct uvc_streaming_control commit;

	int control;

	unsigned int fcc;
	unsigned int width;
	unsigned int height;

	void **mem;
	unsigned int jpg_max, nbufs, bufsize;

	fd_set fds;

	uint8_t bulk_mode, uvc_on, work_mode, color;
};

struct uvc_frame_info
{
	unsigned int width;
	unsigned int height;
	unsigned int intervals[8];
};

struct uvc_format_info
{
	unsigned int fcc;
	const struct uvc_frame_info *frames;
};

static const struct uvc_frame_info uvc_frames_yuyv[] = {
	{
		640,
		360,
		{333333, 0, 0, 0},
	},
	{
		1280,
		720,
		{333333, 0},
	},
	{
		1920,
		1080,
		{
			0,
		},
	},
};

static const struct uvc_frame_info uvc_frames_mjpeg[] = {
	{
		640,
		360,
		{333333, 10000000, 50000000, 0},
	},
	{
		1280,
		720,
		{333333, 0},
	},
	{
		1920,
		1080,
		{
			0,
		},
	},
};

static const struct uvc_format_info uvc_formats[] = {
	{V4L2_PIX_FMT_YUYV, uvc_frames_yuyv},
	{V4L2_PIX_FMT_MJPEG, uvc_frames_mjpeg},
};

static struct uvc_device *uvc_open(const char *devname)
{
	struct uvc_device *dev;
	struct v4l2_capability cap;
	int ret;
	int fd;

	fd = open(devname, O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		printf("v4l2 open failed: %s (%d)\n", strerror(errno), errno);
		return NULL;
	}

	printf("open succeeded, file descriptor = %d\n", fd);

	ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0)
	{
		printf("unable to query device: %s (%d)\n", strerror(errno),
			   errno);
		close(fd);
		return NULL;
	}

	printf("device is %s on bus %s\n", cap.card, cap.bus_info);

	dev = malloc(sizeof *dev);
	if (dev == NULL)
	{
		close(fd);
		return NULL;
	}

	memset(dev, 0, sizeof *dev);
	dev->fd = fd;

	return dev;
}

static void uvc_close(struct uvc_device *dev)
{
	close(dev->fd);
	free(dev->mem);
	free(dev);
}

/* ---------------------------------------------------------------------------
 * Video streaming
 */
void dump_mem_s(void *buf, int len)
{
	int i;
	char *val = buf;

	for (i = 0; i < len; i++)
	{

		if (i % 16 == 0)
			printf("\n");

		printf("  %02x", *val++);
	}
	printf("\n");
}

int tim_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{

	int nsec;

	if (x->tv_sec > y->tv_sec)
		return -1;

	if ((x->tv_sec == y->tv_sec) && (x->tv_usec > y->tv_usec))
		return -1;

	result->tv_sec = (y->tv_sec - x->tv_sec);
	result->tv_usec = (y->tv_usec - x->tv_usec);
	if (result->tv_usec < 0)
	{
		result->tv_sec--;
		result->tv_usec += 1000000;
	}

	return 0;
}

/*
static void uvc_video_fill_buffer(struct uvc_device *dev, struct v4l2_buffer *buf)
{
	switch (dev->fcc)
	{
	case V4L2_PIX_FMT_YUYV:
	{
		unsigned int bpl = dev->width * 2;
		for (unsigned int i = 0; i < dev->height; ++i)
		{
			memset(dev->mem[buf->index] + i * bpl, dev->color++, bpl);
		}
		buf->bytesused = bpl * dev->height;
		break;
	}
	case V4L2_PIX_FMT_MJPEG:
	{
		const char *jpgs[] = {"./images/1.jpg", "./images/2.jpg", "./images/1.jpg", "./images/3.jpg"};
		int max = 0;
		int fd = open(jpgs[buf->index], O_RDONLY);
		if (fd <= 0)
		{
			printf("Unable to open mjpeg_mode image '%s'\n", jpgs[buf->index]);
			break;
		}
		else
		{
			buf->bytesused = lseek(fd, 0, SEEK_END);
			lseek(fd, 0, SEEK_SET);

			if (buf->bytesused > dev->jpg_max)
				dev->jpg_max = buf->bytesused;

			read(fd, dev->mem[buf->index], buf->bytesused);
			close(fd);
		}

		break;
	}
	}
	// dump_mem_s(dev->mem[buf->index]);
}
*/
static void uvc_video_fill_buffer(struct uvc_device *dev, struct v4l2_buffer *buf);

static int uvc_video_process(struct uvc_device *dev)
{
	struct v4l2_buffer buf;
	int ret;

	if (dev->uvc_on == 0)
		return 0;

	memset(&buf, 0, sizeof buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;

	if ((ret = ioctl(dev->fd, VIDIOC_DQBUF, &buf)) < 0)
	{
		printf("Unable to dequeue buffer: %s (%d).\n", strerror(errno),
			   errno);
		return ret;
	}

	uvc_video_fill_buffer(dev, &buf);

	if ((ret = ioctl(dev->fd, VIDIOC_QBUF, &buf)) < 0)
	{
		printf("Unable to requeue buffer: %s (%d).\n", strerror(errno),
			   errno);
		return ret;
	}

	return 0;
}

static int uvc_video_reqbufs(struct uvc_device *dev, int nbufs)
{
	struct v4l2_requestbuffers rb;
	struct v4l2_buffer buf;
	unsigned int i;
	int ret;

	for (i = 0; i < dev->nbufs; ++i)
		munmap(dev->mem[i], dev->bufsize);

	free(dev->mem);
	dev->mem = 0;
	dev->nbufs = 0;

	memset(&rb, 0, sizeof rb);
	rb.count = nbufs;
	rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	rb.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(dev->fd, VIDIOC_REQBUFS, &rb);
	if (ret < 0)
	{
		printf("Unable to allocate buffers: %s (%d).\n",
			   strerror(errno), errno);
		return ret;
	}

	printf("%u buffers allocated.\n", rb.count);

	/* Map the buffers. */
	dev->mem = malloc(rb.count * sizeof dev->mem[0]);

	for (i = 0; i < rb.count; ++i)
	{
		memset(&buf, 0, sizeof buf);
		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(dev->fd, VIDIOC_QUERYBUF, &buf);
		if (ret < 0)
		{
			printf("Unable to query buffer %u: %s (%d).\n", i,
				   strerror(errno), errno);
			return -1;
		}
		printf("length: %u offset: %u\n", buf.length, buf.m.offset);

		dev->mem[i] = mmap(0, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, buf.m.offset);
		if (dev->mem[i] == MAP_FAILED)
		{
			printf("Unable to map buffer %u: %s (%d)\n", i,
				   strerror(errno), errno);
			return -1;
		}
		printf("Buffer %u mapped at address %p.\n", i, dev->mem[i]);
	}

	dev->bufsize = buf.length;
	dev->nbufs = rb.count;

	return 0;
}

static int uvc_video_stream(struct uvc_device *dev, int enable)
{
	struct v4l2_buffer buf;
	unsigned int i;
	int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	int ret;

	if (!enable)
	{
		printf("Stopping video stream.\n");
		ioctl(dev->fd, VIDIOC_STREAMOFF, &type);
		return 0;
	}

	printf("Starting video stream. dev->nbufs %d \n", dev->nbufs);

	for (i = 0; i < dev->nbufs; ++i)
	{
		memset(&buf, 0, sizeof buf);

		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;

		uvc_video_fill_buffer(dev, &buf);

		printf("Queueing buffer %u.\n", i);
		if ((ret = ioctl(dev->fd, VIDIOC_QBUF, &buf)) < 0)
		{
			printf("Unable to queue buffer: %s (%d).\n",
				   strerror(errno), errno);
			break;
		}
	}

	ret = ioctl(dev->fd, VIDIOC_STREAMON, &type);

	if (ret < 0)
		printf("VIDIOC_STREAMON ioctrl err %d\n", ret);
	return ret;
}

static int uvc_video_set_format(struct uvc_device *dev)
{
	struct v4l2_format fmt;
	int ret;

	printf("Setting format to 0x%08x %ux%u\n",
		   dev->fcc, dev->width, dev->height);

	memset(&fmt, 0, sizeof fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width = dev->width;
	fmt.fmt.pix.height = dev->height;
	fmt.fmt.pix.pixelformat = dev->fcc;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	if (dev->fcc == V4L2_PIX_FMT_MJPEG)
	{
		fmt.fmt.pix.sizeimage = dev->jpg_max * 1.5;
		printf(" === mjpeg_mode mode\n");
	}
	else
		printf("=== YUYV mode\n");

	if ((ret = ioctl(dev->fd, VIDIOC_S_FMT, &fmt)) < 0)
		printf("Unable to set format: %s (%d).\n",
			   strerror(errno), errno);

	return ret;
}

static int uvc_video_init(struct uvc_device *dev __attribute__((__unused__)))
{
	return 0;
}

/* ---------------------------------------------------------------------------
 * Request processing
 */

static void uvc_fill_streaming_control(struct uvc_device *dev,
									   struct uvc_streaming_control *ctrl,
									   int iframe, int iformat)
{
	const struct uvc_format_info *format;
	const struct uvc_frame_info *frame;
	unsigned int nframes;

	if (iformat < 0)
		iformat = ARRAY_SIZE(uvc_formats) + iformat;
	if (iformat < 0 || iformat >= (int)ARRAY_SIZE(uvc_formats))
		return;
	format = &uvc_formats[iformat];

	nframes = 0;
	while (format->frames[nframes].width != 0)
		++nframes;

	if (iframe < 0)
		iframe = nframes + iframe;
	if (iframe < 0 || iframe >= (int)nframes)
		return;
	frame = &format->frames[iframe];

	memset(ctrl, 0, sizeof *ctrl);

	ctrl->bmHint = 1;

	ctrl->bFormatIndex = iformat + 1;
	ctrl->bFrameIndex = iframe + 1;

	printf("uvc_fill_streaming_control: bFormatIndex %d,bFrameIndex %d \n", ctrl->bFormatIndex, ctrl->bFrameIndex);

	ctrl->dwFrameInterval = frame->intervals[0];
	switch (format->fcc)
	{
	case V4L2_PIX_FMT_YUYV:
		ctrl->dwMaxVideoFrameSize = frame->width * frame->height * 2;
		break;
	case V4L2_PIX_FMT_MJPEG:
		ctrl->dwMaxVideoFrameSize = dev->jpg_max;
		break;
	}
	ctrl->dwMaxPayloadTransferSize = MAX_PACKET; /* TODO this should be filled by the driver. */
	ctrl->bmFramingInfo = 3;
	ctrl->bPreferedVersion = 1;
	ctrl->bMaxVersion = 1;
}

static void uvc_events_process_standard(struct uvc_device *dev, struct usb_ctrlrequest *ctrl,
										struct uvc_request_data *resp)
{
	printf("standard request\n");
	(void)dev;
	(void)ctrl;
	(void)resp;
}

static void uvc_events_process_control(struct uvc_device *dev, uint8_t req, uint8_t cs,
									   struct uvc_request_data *resp)
{
	printf("control request (req %02x cs %02x)\n", req, cs);
	(void)dev;
	(void)resp;

	struct uvc_streaming_control *ctrl;

	if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
		return;

	ctrl = (struct uvc_streaming_control *)&resp->data;
	resp->length = sizeof *ctrl;

	switch (req)
	{
	case UVC_SET_CUR:
		dev->control = cs;
		resp->length = 34;
		break;

	case UVC_GET_CUR:
		if (cs == UVC_VS_PROBE_CONTROL)
			memcpy(ctrl, &dev->probe, sizeof *ctrl);
		else
			memcpy(ctrl, &dev->commit, sizeof *ctrl);
		break;

	// case UVC_GET_MIN:
	// case UVC_GET_MAX:
	// case UVC_GET_DEF:
	// 	uvc_fill_streaming_control(dev, ctrl, req == UVC_GET_MAX ? -1 : 0,
	// 							   req == UVC_GET_MAX ? -1 : 0);
	// 	break;

	case UVC_GET_RES:
		memset(ctrl, 0, sizeof *ctrl);
		break;

	case UVC_GET_LEN:
		resp->data[0] = 0x00;
		resp->data[1] = 0x22;
		resp->length = 2; // 2;
		break;

	case UVC_GET_INFO:
		resp->data[0] = 0x03;
		resp->length = 1;
		break;
	}
}

static void uvc_events_process_streaming(struct uvc_device *dev, uint8_t req, uint8_t cs,
										 struct uvc_request_data *resp)
{
	struct uvc_streaming_control *ctrl;

	printf("streaming request (req %02x cs %02x)\n", req, cs);

	if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
		return;

	ctrl = (struct uvc_streaming_control *)&resp->data;
	resp->length = sizeof *ctrl;

	switch (req)
	{
	case UVC_SET_CUR:
		dev->control = cs;
		resp->length = 34;
		break;

	case UVC_GET_CUR:
		if (cs == UVC_VS_PROBE_CONTROL)
			memcpy(ctrl, &dev->probe, sizeof *ctrl);
		else
			memcpy(ctrl, &dev->commit, sizeof *ctrl);
		break;

	case UVC_GET_MIN:
	case UVC_GET_MAX:
	case UVC_GET_DEF:
		uvc_fill_streaming_control(dev, ctrl, req == UVC_GET_MAX ? -1 : 0,
								   req == UVC_GET_MAX ? -1 : 0);
		break;

	case UVC_GET_RES:
		memset(ctrl, 0, sizeof *ctrl);
		break;

	case UVC_GET_LEN:
		resp->data[0] = 0x00;
		resp->data[1] = 0x22;
		resp->length = 2; // 2;
		break;

	case UVC_GET_INFO:
		resp->data[0] = 0x03;
		resp->length = 1;
		break;
	}
}

static void uvc_events_process_class(struct uvc_device *dev, struct usb_ctrlrequest *ctrl,
									 struct uvc_request_data *resp)
{
	if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
		return;

	switch (ctrl->wIndex & 0xff)
	{
	case UVC_INTF_CONTROL:
		uvc_events_process_control(dev, ctrl->bRequest, ctrl->wValue >> 8, resp);
		break;

	case UVC_INTF_STREAMING:
		uvc_events_process_streaming(dev, ctrl->bRequest, ctrl->wValue >> 8, resp);
		break;

	default:
		break;
	}
}

static void uvc_events_process_setup(struct uvc_device *dev, struct usb_ctrlrequest *ctrl,
									 struct uvc_request_data *resp)
{
	dev->control = 0;

	printf("bRequestType %02x bRequest %02x wValue %04x wIndex %04x "
		   "wLength %04x\n",
		   ctrl->bRequestType, ctrl->bRequest,
		   ctrl->wValue, ctrl->wIndex, ctrl->wLength);

	switch (ctrl->bRequestType & USB_TYPE_MASK)
	{
	case USB_TYPE_STANDARD:
		uvc_events_process_standard(dev, ctrl, resp);
		break;

	case USB_TYPE_CLASS:
		uvc_events_process_class(dev, ctrl, resp);
		break;

	default:
		break;
	}
}

static void uvc_events_process_data(struct uvc_device *dev, struct uvc_request_data *data)
{
	struct uvc_streaming_control *target;
	struct uvc_streaming_control *ctrl;
	const struct uvc_format_info *format;
	const struct uvc_frame_info *frame;
	const unsigned int *interval;
	unsigned int iformat, iframe;
	unsigned int nframes;

	switch (dev->control)
	{
	case UVC_VS_PROBE_CONTROL:
		printf("setting probe control, length = %d\n", data->length);
		target = &dev->probe;
		break;

	case UVC_VS_COMMIT_CONTROL:
		printf("setting commit control, length = %d\n", data->length);
		target = &dev->commit;
		break;

	default:
		printf("setting unknown control, length = %d\n", data->length);
		return;
	}

	ctrl = (struct uvc_streaming_control *)&data->data;
	iformat = clamp((unsigned int)ctrl->bFormatIndex, 1U, (unsigned int)ARRAY_SIZE(uvc_formats));

	// if (dev->mjpg_mode == 1)
	// 	iformat = 2; // mjpg
	// else
	// 	iformat = 1; // yuv

	format = &uvc_formats[iformat - 1];

	nframes = 0;
	while (format->frames[nframes].width != 0)
		++nframes;

	iframe = clamp((unsigned int)ctrl->bFrameIndex, 1U, nframes);
	frame = &format->frames[iframe - 1];
	interval = frame->intervals;

	while (interval[0] < ctrl->dwFrameInterval && interval[1])
		++interval;

	target->bFormatIndex = iformat;
	target->bFrameIndex = iframe;

	printf("\nctrl->bFormatIndex = %d ,ctrl->bFrameIndex =%d,iformat %d,iframe %d,*interval %d\n", ctrl->bFormatIndex, ctrl->bFrameIndex,
		   iformat, iframe, *interval);

	switch (format->fcc)
	{
	case V4L2_PIX_FMT_YUYV:
		printf("DEBUG: V4L2_PIX_FMT_YUYV\n");
		target->dwMaxVideoFrameSize = frame->width * frame->height * 2;
		break;
	case V4L2_PIX_FMT_MJPEG:
		if (dev->jpg_max == 0)
		{
			printf("WARNING: mjpg requested and no image loaded.\n");
			// dev->jpg_max = frame->width * frame->height * 2;
			dev->jpg_max = 1024*1024; // set jpg max 1m if 2688 * 1520 jpg 95 ≈ 600KB
		}
		target->dwMaxVideoFrameSize = dev->jpg_max;
		break;
	}
	target->dwFrameInterval = *interval;

	if (dev->control == UVC_VS_COMMIT_CONTROL)
	{
		dev->fcc = format->fcc;
		dev->width = frame->width;
		dev->height = frame->height;

		uvc_video_set_format(dev);
		if (dev->bulk_mode)
			uvc_video_stream(dev, 1);
	}
}

static void uvc_events_process(struct uvc_device *dev)
{
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
	struct uvc_request_data resp;
	int ret;

	ret = ioctl(dev->fd, VIDIOC_DQEVENT, &v4l2_event);
	if (ret < 0)
	{
		printf("VIDIOC_DQEVENT failed: %s (%d)\n", strerror(errno),
			   errno);
		return;
	}

	memset(&resp, 0, sizeof resp);
	resp.length = -EL2HLT;

	switch (v4l2_event.type)
	{
	case UVC_EVENT_CONNECT:
	case UVC_EVENT_DISCONNECT:
		return;

	case UVC_EVENT_SETUP:
		uvc_events_process_setup(dev, &uvc_event->req, &resp);
		break;

	case UVC_EVENT_DATA:
		uvc_events_process_data(dev, &uvc_event->data);
		return;

	case UVC_EVENT_STREAMON:
		uvc_video_reqbufs(dev, BUF_CNT);
		uvc_video_stream(dev, 1);

		dev->uvc_on = 1;
		return;

	case UVC_EVENT_STREAMOFF:
		uvc_video_stream(dev, 0);
		uvc_video_reqbufs(dev, 0);

		dev->uvc_on = 0;
		return;
	}

	// printf("[uvc_events_process] %X %X %X %X %X %X\r\n", \
		UVCIOC_SEND_RESPONSE, v4l2_event.type, uvc_event->req.bRequest, resp.length, resp.data[0], resp.data[1]);

	// printf("[uvc_events_process] %X %d\r\n", uvc_event->req.bRequest, resp.length);

	ioctl(dev->fd, UVCIOC_SEND_RESPONSE, &resp);
	if (ret < 0)
	{
		printf("UVCIOC_S_EVENT failed: %s (%d)\n", strerror(errno),
			   errno);
		return;
	}
}

static void uvc_events_init(struct uvc_device *dev)
{
	struct v4l2_event_subscription sub;

	uvc_fill_streaming_control(dev, &dev->probe, 0, 0);
	uvc_fill_streaming_control(dev, &dev->commit, 0, 0);

	if (dev->bulk_mode)
	{
		/* FIXME Crude hack, must be negotiated with the driver. */
		dev->probe.dwMaxPayloadTransferSize = 16 * 1024;
		dev->commit.dwMaxPayloadTransferSize = 16 * 1024;
	}

	memset(&sub, 0, sizeof sub);
	sub.type = UVC_EVENT_SETUP;
	ioctl(dev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_DATA;
	ioctl(dev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_STREAMON;
	ioctl(dev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
	sub.type = UVC_EVENT_STREAMOFF;
	ioctl(dev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
}

int uvc_loop(struct uvc_device **self)
{
	struct uvc_device *dev = *self;
	fd_set efds = dev->fds;
	fd_set wfds = dev->fds;

	int ret = select(dev->fd + 1, NULL, &wfds, &efds, NULL);
	if (FD_ISSET(dev->fd, &efds))
		uvc_events_process(dev);
	if (FD_ISSET(dev->fd, &wfds))
		uvc_video_process(dev);
	usleep(1000);
	return ret;
}

int uvc_exit(struct uvc_device **self)
{
	struct uvc_device *dev = *self;
	dev->work_mode = 0;
	uvc_close(dev);
	*self = NULL;
	return 0;
}

int uvc_init(struct uvc_device **self)
{
	struct uvc_device *dev = NULL;
	*self = NULL;
	dev = uvc_open("/dev/video0");
	if (dev == NULL)
		return 1;
	// dev->mjpg_mode = 1; // for m3axpi 1 mjpg 0 yuyv
	dev->bulk_mode = 0; // for m3axpi only 0
	dev->work_mode = 1;
	uvc_events_init(dev);
	uvc_video_init(dev);
	FD_ZERO(&dev->fds);
	FD_SET(dev->fd, &dev->fds);
	*self = dev;
	return 0;
}

int uvc_unit_test()
{
	struct uvc_device *dev;
	uvc_init(&dev);
	while (dev->work_mode)
	{
		uvc_loop(&dev);
	}
	uvc_exit(&dev);
	return 0;
}

// gcc ./main.c -lpthread

struct uvc_thread
{
    pthread_t task;
	struct uvc_device *dev;
};

int _uvc_thread(void *arg)
{
	struct uvc_thread *uvc = arg;
	uvc_init(&uvc->dev);
	while (uvc->dev->work_mode)
	{
		uvc_loop(&uvc->dev);
	}
	uvc_exit(&uvc->dev);
	return 0;
}

int uvc_start(struct uvc_thread *uvc)
{
	if ((pthread_create(&uvc->task, NULL, _uvc_thread, (void *)uvc)) == -1) {
		printf(" create error!\n");
		return -1;
	}
	return 0;
}

int uvc_stop(struct uvc_thread *uvc)
{
	uvc->dev->work_mode = 0;
	if (pthread_join(uvc->task, NULL)) {
		printf("thread is not exit...\n");
		return -1;
	}
	return 0;
}

#endif /* _DLS_UVC_GADGET_H_ */

