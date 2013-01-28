/*
 * generic helper functions for handling video4linux capture buffers
 *
 * (c) 2007 Mauro Carvalho Chehab, <mchehab@infradead.org>
 *
 * Highly based on video-buf written originally by:
 * (c) 2001,02 Gerd Knorr <kraxel@bytesex.org>
 * (c) 2006 Mauro Carvalho Chehab, <mchehab@infradead.org>
 * (c) 2006 Ted Walther and John Sokol
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2
 */

#ifndef _VIDEOBUF_CORE_H
#define _VIDEOBUF_CORE_H

#include <linux/poll.h>
#ifdef CONFIG_VIDEO_V4L1_COMPAT
#define __MIN_V4L1
#include <linux/videodev.h>
#endif
#include <linux/videodev2.h>

#define UNSET (-1U)


struct videobuf_buffer;
struct videobuf_queue;

/* --------------------------------------------------------------------- */

/*
 * A small set of helper functions to manage video4linux buffers.
 *
 * struct videobuf_buffer holds the data structures used by the helper
 * functions, additionally some commonly used fields for v4l buffers
 * (width, height, lists, waitqueue) are in there.  That struct should
 * be used as first element in the drivers buffer struct.
 *
 * about the mmap helpers (videobuf_mmap_*):
 *
 * The mmaper function allows to map any subset of contingous buffers.
 * This includes one mmap() call for all buffers (which the original
 * video4linux API uses) as well as one mmap() for every single buffer
 * (which v4l2 uses).
 *
 * If there is a valid mapping for a buffer, buffer->baddr/bsize holds
 * userspace address + size which can be feeded into the
 * videobuf_dma_init_user function listed above.
 *
 */

struct videobuf_mapping {
	unsigned int count;
	unsigned long start;
	unsigned long end;
	struct videobuf_queue *q;
};

enum videobuf_state {
	VIDEOBUF_NEEDS_INIT = 0,
	VIDEOBUF_PREPARED   = 1,
	VIDEOBUF_QUEUED     = 2,
	VIDEOBUF_ACTIVE     = 3,
	VIDEOBUF_DONE       = 4,
#ifdef CONFIG_VIDEO_EMXX
	VIDEOBUF_CANCELED   = 5,
	VIDEOBUF_DQBUF_PERMIT = 6,
	VIDEOBUF_ERROR      = 7,
	VIDEOBUF_IDLE       = 8,
#else
	VIDEOBUF_ERROR      = 5,
	VIDEOBUF_IDLE       = 6,
#endif
};

#ifdef CONFIG_VIDEO_EMXX
/* state_queued flag */ /*** executing process is set. ***/
enum videobuf_state_queued{
	STATE_QUEUED_IDLE,
	STATE_QUEUED_ROT_PREPARED,
	STATE_QUEUED_ROT_QUEUED,
	STATE_QUEUED_TMR_PREPARED,
	STATE_QUEUED_TMR_QUEUED,
	STATE_QUEUED_LCD_PREPARED,
	STATE_QUEUED_LCD_QUEUED,
	STATE_QUEUED_DONE,
};

/* state_proccess flag */ /*** executed process is set. ***/
#define STATE_PROCCESS_IDLE         0x00
#define STATE_PROCCESS_ROT_QUEUED   0x01
#define STATE_PROCCESS_ROT_COMPLETE 0x02
#define STATE_PROCCESS_TMR_QUEUED   0x10
#define STATE_PROCCESS_TMR_COMPLETE 0x20
#define STATE_PROCCESS_LCD_QUEUED   0x40
#define STATE_PROCCESS_LCD_COMPLETE 0x80

/* for state_frame */ /*** executed process is set. ***/
#define STATE_FRAME_DONE       0
#define STATE_FRAME_IMMEDIATE -1
#define STATE_FRAME_CANCELED  -2
#define STATE_FRAME_SKIPPED   -3
#endif

struct videobuf_buffer {
	unsigned int            i;
	u32                     magic;

	/* info about the buffer */
	unsigned int            width;
	unsigned int            height;
	unsigned int            bytesperline; /* use only if != 0 */
	unsigned long           size;
	unsigned int            input;
	enum v4l2_field         field;
	enum videobuf_state     state;
	struct list_head        stream;  /* QBUF/DQBUF list */

	/* touched by irq handler */
	struct list_head        queue;
	wait_queue_head_t       done;
	unsigned int            field_count;
	struct timeval          ts;

	/* Memory type */
	enum v4l2_memory        memory;

	/* buffer size */
	size_t                  bsize;

	/* buffer offset (mmap + overlay) */
	size_t                  boff;

	/* buffer addr (userland ptr!) */
	unsigned long           baddr;

	/* for mmap'ed buffers */
	struct videobuf_mapping *map;

	/* Private pointer to allow specific methods to store their data */
	int			privsize;
	void                    *priv;
#ifdef CONFIG_VIDEO_EMXX
	enum videobuf_state_queued state_queued; /* executed process is set. */
	unsigned long		state_proccess;  /* executed process is set.  */
	long			state_frame;     /* executed process is set. */

	struct temp_buffer	*rot_buf;        /* pointer to ROT buffer */

#ifdef CONFIG_VIDEO_EMXX_FILTER
	struct v4l2_filter_option	filter;	/* resize filter */
#endif
	struct v4l2_phys_add	base_addr;
	struct v4l2_effect	save_effect;	/* the latest set value*/
#ifdef CONFIG_VIDEO_EMXX_IMAGESIZE
	__u32	         	image_width;	/* original image width */
	__u32	         	image_height;	/* original image height */
#endif
	__u32	         	pixelformat;

	__u32	         	output;
	__u32			sequence;
	wait_queue_head_t       clear_done;
#endif
};

struct videobuf_queue_ops {
	int (*buf_setup)(struct videobuf_queue *q,
			 unsigned int *count, unsigned int *size);
	int (*buf_prepare)(struct videobuf_queue *q,
			   struct videobuf_buffer *vb,
			   enum v4l2_field field);
	void (*buf_queue)(struct videobuf_queue *q,
			  struct videobuf_buffer *vb);
	void (*buf_release)(struct videobuf_queue *q,
			    struct videobuf_buffer *vb);
#ifdef CONFIG_VIDEO_EMXX
	int (*buf_cancel)(struct videobuf_queue *q,
			    struct videobuf_buffer *vb);
	void (*buf_dqueue)(struct videobuf_queue *q,
			   struct videobuf_buffer *vb);
#endif
};

#define MAGIC_QTYPE_OPS	0x12261003

/* Helper operations - device type dependent */
struct videobuf_qtype_ops {
	u32                     magic;

	struct videobuf_buffer *(*alloc)(size_t size);
	void *(*vaddr)		(struct videobuf_buffer *buf);
	int (*iolock)		(struct videobuf_queue *q,
				 struct videobuf_buffer *vb,
				 struct v4l2_framebuffer *fbuf);
	int (*sync)		(struct videobuf_queue *q,
				 struct videobuf_buffer *buf);
	int (*mmap_mapper)	(struct videobuf_queue *q,
				 struct videobuf_buffer *buf,
				 struct vm_area_struct *vma);
};

struct videobuf_queue {
	struct mutex               vb_lock;
	spinlock_t                 *irqlock;
	struct device		   *dev;

	wait_queue_head_t	   wait; /* wait if queue is empty */

	enum v4l2_buf_type         type;
	unsigned int               inputs; /* for V4L2_BUF_FLAG_INPUT */
	unsigned int               msize;
	enum v4l2_field            field;
	enum v4l2_field            last;   /* for field=V4L2_FIELD_ALTERNATE */
	struct videobuf_buffer     *bufs[VIDEO_MAX_FRAME];
	const struct videobuf_queue_ops  *ops;
	struct videobuf_qtype_ops  *int_ops;

	unsigned int               streaming:1;
	unsigned int               reading:1;

	/* capture via mmap() + ioctl(QBUF/DQBUF) */
	struct list_head           stream;

	/* capture via read() */
	unsigned int               read_off;
	struct videobuf_buffer     *read_buf;

	/* driver private data */
	void                       *priv_data;
#ifdef CONFIG_VIDEO_EMXX
	__u32                      index_max;
	__u32                      sequence;
#endif
};

int videobuf_waiton(struct videobuf_buffer *vb, int non_blocking, int intr);
int videobuf_iolock(struct videobuf_queue *q, struct videobuf_buffer *vb,
		struct v4l2_framebuffer *fbuf);

struct videobuf_buffer *videobuf_alloc(struct videobuf_queue *q);

/* Used on videobuf-dvb */
void *videobuf_queue_to_vaddr(struct videobuf_queue *q,
			      struct videobuf_buffer *buf);

void videobuf_queue_core_init(struct videobuf_queue *q,
			 const struct videobuf_queue_ops *ops,
			 struct device *dev,
			 spinlock_t *irqlock,
			 enum v4l2_buf_type type,
			 enum v4l2_field field,
			 unsigned int msize,
			 void *priv,
			 struct videobuf_qtype_ops *int_ops);
int  videobuf_queue_is_busy(struct videobuf_queue *q);
void videobuf_queue_cancel(struct videobuf_queue *q);

enum v4l2_field videobuf_next_field(struct videobuf_queue *q);
int videobuf_reqbufs(struct videobuf_queue *q,
		     struct v4l2_requestbuffers *req);
int videobuf_querybuf(struct videobuf_queue *q, struct v4l2_buffer *b);
int videobuf_qbuf(struct videobuf_queue *q,
		  struct v4l2_buffer *b);
int videobuf_dqbuf(struct videobuf_queue *q,
		   struct v4l2_buffer *b, int nonblocking);
#ifdef CONFIG_VIDEO_V4L1_COMPAT
int videobuf_cgmbuf(struct videobuf_queue *q,
		    struct video_mbuf *mbuf, int count);
#endif
int videobuf_streamon(struct videobuf_queue *q);
int videobuf_streamoff(struct videobuf_queue *q);

void videobuf_stop(struct videobuf_queue *q);

int videobuf_read_start(struct videobuf_queue *q);
void videobuf_read_stop(struct videobuf_queue *q);
ssize_t videobuf_read_stream(struct videobuf_queue *q,
			     char __user *data, size_t count, loff_t *ppos,
			     int vbihack, int nonblocking);
ssize_t videobuf_read_one(struct videobuf_queue *q,
			  char __user *data, size_t count, loff_t *ppos,
			  int nonblocking);
unsigned int videobuf_poll_stream(struct file *file,
				  struct videobuf_queue *q,
				  poll_table *wait);

int videobuf_mmap_setup(struct videobuf_queue *q,
			unsigned int bcount, unsigned int bsize,
			enum v4l2_memory memory);
int __videobuf_mmap_setup(struct videobuf_queue *q,
			unsigned int bcount, unsigned int bsize,
			enum v4l2_memory memory);
int videobuf_mmap_free(struct videobuf_queue *q);
int videobuf_mmap_mapper(struct videobuf_queue *q,
			 struct vm_area_struct *vma);

#endif