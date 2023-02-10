#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>



int file_load(const char *filename, void **p_data, long *p_size)
{
	FILE *fp;

	if (!filename || !filename[0]) {
		printf("filename can not be empty\n");
		return -1;
	}
	
	if (!p_data || !p_size) {
		printf("invalid arguments\n");
		return -1;
	}

	fp = fopen(filename, "rb");
	if (NULL == fp) {
		printf("can not open file \"%s\"\n", filename);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	*p_size = ftell(fp);
	rewind(fp);

	*p_data = malloc(*p_size);
	if (!(*p_data)) {
		printf("out of memory\n");
		fclose(fp);
		return -1;
	}
	
	fread(*p_data, 1, *p_size, fp);
	
	fclose(fp);

	return 0;
}

int file_save(const char *filename, const void *data, long data_size)
{
	FILE *fp;

	if (!filename || !filename[0]) {
		printf("invalid file name\n");
		return -1;
	}

	if (!data || data_size <= 0) {
		printf("invalid file data\n");
		return -1;
	}

	fp = fopen(filename, "wb");
	if (NULL == fp) {
		printf("create file %s failed\n", filename);
		return -1;
	}

	fwrite(data, 1, data_size, fp);

	fclose(fp);

	return 0;
}


struct v4l2_buffer_map {
	void *start;
	int length;
};

int nx_vpu_encode(const void *input_data, long input_data_size, 
				  unsigned int input_data_format, int width, int height, 
				  void **output, long *output_size)
{
#define NX_ENC_DEV "/dev/video14"
//#define NX_DEC_DEV "/dev/video15"
/*
 * V4L2_MEMORY_MMAP   : driver io mode not support;
 * V4L2_MEMORY_USERPTR: ???
 * V4L2_MEMORY_DMABUF : driver missed struct v4l2_ioctl_ops::vidioc_expbuf op, not support;
 */
#define V4L2_MEM_TYPE V4L2_MEMORY_MMAP//V4L2_MEMORY_USERPTR
#define V4L2_PLANE_N 3 // maxium plane number

	int fd = -1, plane_num = 1, i;
	struct timespec t0, t;

	if (!input_data || input_data_size <= 0) {
		printf("invalid input data\n");
		return -1;
	}

	/*if (yuv_format != ...) {
		return -1;
	}*/

	if (width <= 0 || height <= 0) {
		printf("invalid YUV dimension: %dx%d\n", width, height);
		return -1;
	}

	if (!output || !output_size) {
		printf("invalid JPEG arguments\n");
		return -1;
	}

	//clock_gettime(CLOCK_MONOTONIC, &t0);

	fd = open(NX_ENC_DEV, O_RDWR);
	if (fd < 0) {
		printf("open %s failed: %s\n", strerror(errno));
		return -1;
	}

	//--------------------------------------------------------------------
	struct v4l2_format fmt;

	/* set encoder input format??? */
	printf("set input format...\n");
	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = width;
	fmt.fmt.pix_mp.height = height;
	fmt.fmt.pix_mp.pixelformat = input_data_format/*V4L2_PIX_FMT_YUV420*/;
	if (input_data_format == V4L2_PIX_FMT_YUV420) {
		plane_num = 3;
		fmt.fmt.pix_mp.num_planes = plane_num; // 1, 3???
		if (plane_num == 3) {
			fmt.fmt.pix_mp.plane_fmt[0].sizeimage = width * height;
			fmt.fmt.pix_mp.plane_fmt[0].bytesperline = width;
			fmt.fmt.pix_mp.plane_fmt[1].sizeimage = (width * height) / 2;
    		fmt.fmt.pix_mp.plane_fmt[1].bytesperline = width / 2;
		}/* else if (V4L2_PLANE_N == 1) {
			fmt.fmt.pix_mp.plane_fmt[0].sizeimage = yuv_size + (width * height) / 2;
			fmt.fmt.pix_mp.plane_fmt[0].bytesperline = width + width / 2;
		}*/
	} else if (input_data_format == V4L2_PIX_FMT_GREY) {
		plane_num = 1;
		fmt.fmt.pix_mp.num_planes = plane_num; 
		fmt.fmt.pix_mp.plane_fmt[0].sizeimage = input_data_size;
		fmt.fmt.pix_mp.plane_fmt[0].bytesperline = width;
	} else {
		printf("unsupported encode input format: %d\n", input_data_format);
		close(fd);
		return -1;
	}
	//fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		printf("set encoder input format failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	/* set encoder output format??? */
	printf("set output format...\n");
	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	//fmt.fmt.pix_mp.width = width;
	//fmt.fmt.pix_mp.height = height;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = width * height;
	//fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		printf("set encoder output format failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	//--------------------------------------------------------------------

	struct v4l2_requestbuffers rb;
	struct v4l2_buffer buf;
	struct v4l2_plane inb_planes[V4L2_PLANE_N], outb_planes[1];
	struct v4l2_buffer_map inb_map[V4L2_PLANE_N], outb_map;

	/* 
	 * request input buffers 
	 */
	//--------------------------------------------------------------------
	printf("request input buffer(s)...\n");
	memset(&rb, 0, sizeof(struct v4l2_requestbuffers));
	rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	rb.memory = V4L2_MEM_TYPE; 
	rb.count = 1;
	if (ioctl(fd, VIDIOC_REQBUFS, &rb) < 0) {
		printf("request input buffers failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	//--------------------------------------------------------------------
	
	//--------------------------------------------------------------------
	printf("query input buffer(s)...\n");
	memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.index = 0;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEM_TYPE;
	buf.length = plane_num;
#if 0
	for (i = 0; i < V4L2_PLANE_N; i++) { //TODO:
		inb_planes[i].length = yuv_size;
		inb_planes[i].m.userptr = (unsigned long)yuv;
		inb_planes[i].data_offset = 0;
	}
#endif
	buf.m.planes = inb_planes; 
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
		printf("query input buffers failed: %s\n", strerror(errno));
		close(fd);
		return -1;
    }
	//--------------------------------------------------------------------

#if 1
	printf("input buffer planes info(memory=%u,offset=%u,length=%u):\n", 
			buf.memory, buf.m.offset, buf.length);
	for (i = 0; i < plane_num; i++) {
		printf("-> plane%d: bytesused=%u, length=%u, mem_offset=%u\n", 
				i, inb_planes[i].bytesused, 
				inb_planes[i].length, inb_planes[i].m.mem_offset);
	}
#endif

	/*if (V4L2_MEM_TYPE == V4L2_MEMORY_MMAP) {
		for (i = 0; i < V4L2_PLANE_N; i++) {
			inb_map[i].start = mmap(0, buf.m.planes[i].length, 
									PROT_READ|PROT_WRITE, MAP_SHARED, 
									fd, buf.m.planes[i].m.mem_offset);
    		if (inb_map[i].start == MAP_FAILED) {
				printf("mmap input buffer failed: %s\n", strerror(errno));
				close(fd);
				return -1;
    		}
			inb_map[i].length = buf.m.planes[i].length;
		}
		
		memcpy(inb_map[0].start, yuv, yuv_size);
	}*/

	/* 
	 * request output buffers 
	 */
	//--------------------------------------------------------------------
	printf("request output buffer(s)...\n");
	memset(&rb, 0, sizeof(struct v4l2_requestbuffers));
	rb.count = 1;
	rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	rb.memory = V4L2_MEM_TYPE;
	if (ioctl(fd, VIDIOC_REQBUFS, &rb) < 0) {
		printf("request output buffers failed: %s\n", strerror(errno));
		
		//munmap(inb_map.start, inb_map.length);
		close(fd);
		
		return -1;
	}
	//--------------------------------------------------------------------

	if (V4L2_MEM_TYPE == V4L2_MEMORY_USERPTR) {
		//*output = malloc(bufsize);
		*output = valloc(width * height); // page aligned
	}
	
	//--------------------------------------------------------------------
	printf("query output buffer(s)...\n");
	memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.index = 0;
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory = V4L2_MEM_TYPE;
	buf.length = 1;
	//for (i = 0; i < 1; i++) { //TODO:
	//	outb_planes[i].length = width * height;
	//	outb_planes[i].m.userptr = (unsigned long)(*p_jpeg);
	//	outb_planes[i].data_offset = 0;
	//}
	buf.m.planes = outb_planes; 
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
		printf("unable to query output buffer: %s\n", strerror(errno));
		
		//munmap(inb_map.start, inb_map.length);
		close(fd);
		
		return -1;
    }
	//--------------------------------------------------------------------
	
#if 1
	printf("output buffer planes:\n");
	for (i = 0; i < 1; i++) {
		printf("-> plane%d: bytesused=%u, length=%u, mem_offset=%u\n", 
				i, outb_planes[i].bytesused, 
				outb_planes[i].length, outb_planes[i].m.mem_offset);
	}
#endif

	//--------------------------------------------------------------------
	if (V4L2_MEM_TYPE == V4L2_MEMORY_MMAP) {
		printf("mmap input buffer(s)...\n");
        for (i = 0; i < plane_num; i++) {
            inb_map[i].start = mmap(0, inb_planes[i].length,
                                    PROT_READ|PROT_WRITE, MAP_SHARED,
                                    fd, inb_planes[i].m.mem_offset);
            if (inb_map[i].start == MAP_FAILED) {
                printf("mmap input buffer failed: %s\n", strerror(errno));
                close(fd);
                return -1;
            }
            inb_map[i].length = inb_planes[i].length;
        }

        memcpy(inb_map[0].start, input_data, input_data_size);
    }
	//--------------------------------------------------------------------
	
	//--------------------------------------------------------------------
	if (V4L2_MEM_TYPE == V4L2_MEMORY_MMAP) {
		printf("mmap output buffer(s)...\n");
		outb_map.start = mmap(0, outb_planes[0].length, 
							  PROT_READ|PROT_WRITE, MAP_SHARED, 
							  fd, outb_planes[0].m.mem_offset);
    	if (outb_map.start == MAP_FAILED) {
			printf("unable to map output buffer: %s\n", strerror(errno));
			//munmap(inb_map.start, inb_map.length);
			close(fd);
			return -1;
    	}
		outb_map.length = outb_planes[0].length;
	}
	//--------------------------------------------------------------------
    
	clock_gettime(CLOCK_MONOTONIC, &t0);

	//--------------------------------------------------------------------
	printf("queue output buffer(s)...\n");
	memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.index = 0;
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory = V4L2_MEM_TYPE;
	buf.length = 1;
	buf.m.planes = outb_planes;
	outb_planes[0].bytesused = outb_planes[0].length;
	//buf.bytesused = outb_planes[0].length;
#if 0
	for (i = 0; i < 1; i++) { //TODO:
		outb_planes[i].length = width * height;
		outb_planes[i].bytesused = outb_planes[i].length; 
		outb_planes[i].m.userptr = (unsigned long)(*p_jpeg);
		outb_planes[i].data_offset = 0;
	}
#endif
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        printf("queue output buffers failed: %s\n", strerror(errno));

        //munmap(inb_map.start, inb_map.length);
        //munmap(outb_map.start, outb_map.length);

        close(fd);

        return -1;
    }
	//--------------------------------------------------------------------

	/* 
	 * input & ouput stream on. 
	 */
	int type;

	//--------------------------------------------------------------------
#if 1
	printf("output stream on...\n");
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMON, &type)) {
		printf("start decoder output failed: %s\n", strerror(errno));
		
		//munmap(inb_map.start, inb_map.length);
		//munmap(outb_map.start, outb_map.length);
		
		close(fd);
		
		return -1;
	}
#endif
	//--------------------------------------------------------------------

#if 0
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMON, &type)) {
		printf("start decoder input failed: %s\n", strerror(errno));

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		ioctl(fd, VIDIOC_STREAMOFF, &type);
		
		//munmap(inb_map.start, inb_map.length);
		//munmap(outb_map.start, outb_map.length);
		
		close(fd);
		
		return -1;
	}
#endif

	/*
     * queue input & output buffers.
     */
	printf("queue input buffer(s)...\n");
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.index = 0;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEM_TYPE;
	buf.m.planes = inb_planes;
	buf.length = plane_num;
	for (i = 0; i < plane_num; i++)
		inb_planes[i].bytesused = inb_planes[i].length;
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        printf("queue input buffers failed: %s\n", strerror(errno));

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		ioctl(fd, VIDIOC_STREAMOFF, &type);
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		ioctl(fd, VIDIOC_STREAMOFF, &type);
        
		//munmap(inb_map.start, inb_map.length);
        //munmap(outb_map.start, outb_map.length);
        
		close(fd);

        return -1;
    }

#if 1
	printf("input stream on...\n");
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMON, &type)) {
		printf("start decoder input failed: %s\n", strerror(errno));
		
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		ioctl(fd, VIDIOC_STREAMOFF, &type);

		//munmap(inb_map.start, inb_map.length);
		//munmap(outb_map.start, outb_map.length);
		
		close(fd);
		
		return -1;
	}
	
	#if 0
	printf("output stream on...\n");
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMON, &type)) {
		printf("start decoder output failed: %s\n", strerror(errno));
		
		//munmap(inb_map.start, inb_map.length);
		//munmap(outb_map.start, outb_map.length);
		
		close(fd);
		
		return -1;
	}
	#endif
#endif

	/* get encoded data frame from output buffer */
	printf("dequeue an output buffer...\n");
	memset(&buf, 0, sizeof(struct v4l2_buffer));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory = V4L2_MEM_TYPE;
	buf.length = 1;
	buf.m.planes = outb_planes;
	if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
		printf("dequeue output buffer failed: %s\n", strerror(errno));

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		ioctl(fd, VIDIOC_STREAMOFF, &type);
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		ioctl(fd, VIDIOC_STREAMOFF, &type);
		
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEM_TYPE;
		(void)ioctl(fd, VIDIOC_DQBUF, &buf);
		
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		buf.memory = V4L2_MEM_TYPE;
		(void)ioctl(fd, VIDIOC_DQBUF, &buf);
		
		//munmap(inb_map.start, inb_map.length);
		//munmap(outb_map.start, outb_map.length);
		
		close(fd);
		
		return -1;
	}

	/* copy to encoded buffer */
	printf("encoded frame size size: %u bytes\n", buf.m.planes[0].bytesused);
	*output_size = outb_planes[0].bytesused;
	if (V4L2_MEM_TYPE == V4L2_MEMORY_MMAP) {
		*output = malloc(*output_size);
		memcpy(*output, outb_map.start, *output_size);
	}
	//memcpy(*p_jpeg, outb_map.start, buf.bytesused);

	printf("input stream off...\n");
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ioctl(fd, VIDIOC_STREAMOFF, &type);
	
	printf("output stream off...\n");
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ioctl(fd, VIDIOC_STREAMOFF, &type);
	
#if 0
	memset(&buf, 0, sizeof(struct v4l2_buffer));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEM_TYPE;
	(void)ioctl(fd, VIDIOC_DQBUF, &buf);
	
	memset(&buf, 0, sizeof(struct v4l2_buffer));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory = V4L2_MEM_TYPE;
	(void)ioctl(fd, VIDIOC_DQBUF, &buf);
#endif
	
	//munmap(inb_map.start, inb_map.length);
	//munmap(outb_map.start, outb_map.length);

	printf("close device\n");	
	close(fd);

	clock_gettime(CLOCK_MONOTONIC, &t);
	printf("encode spent %lldus\n", 
			((long long)( ((long long)(t.tv_sec - t0.tv_sec)) * 1000000000LL + t.tv_nsec - t0.tv_nsec )) / 1000LL);

	return 0;
}

int main(int argc, char *argv[])
{
	int width, height;
	void *input_data;
	long input_data_size;
	unsigned int input_data_format;
	void *output;
	long output_size;

	if (argc != 4) {
		printf("usage: %s <yuv-file-name> <width> <height>\n", argv[0]);
		return -1;
	}
	
	if (file_load(argv[1], &input_data, &input_data_size) < 0) {
		printf("load file %s failed\n", argv[1]);
		return -1;
	}
	
	width = atoi(argv[2]);
	height = atoi(argv[3]);
	if (width <= 0 || height <= 0) {
		printf("invalid yuv file dimension: %dx%d\n", width, height);
		free(input_data);
		return -1;
	}

	/* determin YUV format: V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_YUV444 */
	if (input_data_size == width * height + width * height / 2)
		input_data_format = V4L2_PIX_FMT_YUV420;
	else if (input_data_size == width * height * 3)
		input_data_format = V4L2_PIX_FMT_YUV444;
	else if (input_data_size == width * height)
		input_data_format = V4L2_PIX_FMT_GREY;
	else {
		printf("yuv file format is unknown\n");
		free(input_data);
		return -1;
	}

	printf("%s: %dx%d, %ldbytes, %s\n", 
			argv[1], width, height, input_data_size, 
			input_data_format == V4L2_PIX_FMT_YUV420 ? "YUV420" : 
				(input_data_format == V4L2_PIX_FMT_YUV444 ? "YUV444" : "GREY"));

	if (nx_vpu_encode(input_data, input_data_size, 
						input_data_format, width, height, 
						&output, &output_size) < 0) {
		printf("convert %s to JPEG failed\n", argv[1]);
		free(input_data);
		return -1;
	}
	
	printf("save to file...\n");
	char ofilename[256];
	sprintf(ofilename, "%s.jpg", argv[1]);
	file_save(ofilename, output, output_size);

	free(input_data);
	free(output);

	return 0;
}



