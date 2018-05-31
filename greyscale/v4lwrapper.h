#ifndef _V4L_WRAPPER_H
#define _V4L_WRAPPER_H

// Define when you need debug logging data
#define V4L_WRAPPER_DEBUG_LOG 1

// define if you want exit(1) got called on errors - otherwise just the flag is set
#define EXIT_ON_ERROR 1


template<int WIDTH = 640, int HEIGHT = 480>
class V4LWrapper {
public:
	/** Opens a video device and prepares streaming */
	V4LWrapper() {
		// 1.  Open the device
		fd = open("/dev/video0",O_RDWR);
		if(fd < 0){
			perror("Failed to open device, OPEN");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}


		// 2. Ask the device if it can capture frames
		if(ioctl(fd, VIDIOC_QUERYCAP, &capability) < 0){
			// something went wrong... exit
			perror("Failed to get device capabilities, VIDIOC_QUERYCAP");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}

		// 2.5.) Check for video capture and streaming capabilities
		if(!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
			fprintf(stderr, "The device does not handle single-planar video capture.\n");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}

		if(!(capability.capabilities & V4L2_CAP_STREAMING)){
			fprintf(stderr, "The device does not handle video capture streaming.\n");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}


		// 3. Set Image format
		imageFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		imageFormat.fmt.pix.width = WIDTH;
		imageFormat.fmt.pix.height = HEIGHT;
		imageFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		imageFormat.fmt.pix.field = V4L2_FIELD_NONE; // we could choose interlacing here - not interlaced
		// tell the device you are using this format
		if(ioctl(fd, VIDIOC_S_FMT, &imageFormat) < 0){
			perror("Device could not set format, VIDIOC_S_FMT");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}


		// 4. Request Buffers from the device
		requestBuffer.count = 1; // one request buffer
		requestBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // request a buffer wich we an use for capturing frames
		requestBuffer.memory = V4L2_MEMORY_MMAP;

		if(ioctl(fd, VIDIOC_REQBUFS, &requestBuffer) < 0){
			perror("Could not request buffer from device, VIDIOC_REQBUFS");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}

		// 5. Query the buffer to get raw data ie. ask for the requested buffer
		// and allocate memory for it
		memset(&queryBuffer, 0, sizeof(queryBuffer));
		queryBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		queryBuffer.memory = V4L2_MEMORY_MMAP;
		queryBuffer.index = 0;
		if(ioctl(fd, VIDIOC_QUERYBUF, &queryBuffer) < 0){
			perror("Device did not return the buffer information, VIDIOC_QUERYBUF");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}
		// use a pointer to point to the newly created buffer
		// mmap() will map the memory address of the device to
		// an address in memory
		buffer = (uint8_t*)mmap(NULL, queryBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
							fd, queryBuffer.m.offset);
		memset(buffer, 0, queryBuffer.length);


		// 6. Prepare buffer to get a frame
		// Create a new buffer type so the device knows whichbuffer we are talking about
		memset(&bufferinfo, 0, sizeof(bufferinfo));
		bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		bufferinfo.memory = V4L2_MEMORY_MMAP;
		bufferinfo.index = 0;

		// Activate streaming
		type = bufferinfo.type;
		if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
			perror("Could not start streaming, VIDIOC_STREAMON");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}
	}

	/** Closes resources - or at least tries to do so*/
	~V4LWrapper() {
		// end streaming
		if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
			perror("Could not end streaming, VIDIOC_STREAMOFF");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}

		close(fd);
	}

	uint8_t* nextFrame() {
		// Queue the buffer
		if(ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0){
			perror("Could not queue buffer, VIDIOC_QBUF");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}

		// Dequeue the buffer
		if(ioctl(fd, VIDIOC_DQBUF, &bufferinfo) < 0){
			perror("Could not dequeue the buffer, VIDIOC_DQBUF");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}
		// Frames get written after dequeuing the buffer

#ifdef V4L_WRAPPER_DEBUG_LOG
		printf("The buffer has %d KBytes of data\n", bufferinfo.bytesused / 1024);
#endif // V4L_WRAPPER_DEBUG_LOG
		return buffer;
	}

	unsigned int getBytesUsed() {
		return bufferinfo.bytesused;
	}
	
private:
	// A file descriptor to the video device
	int fd;
	v4l2_capability capability;
	v4l2_format imageFormat;
	v4l2_requestbuffers requestBuffer = {0};
	v4l2_buffer queryBuffer = {0};
	uint8_t *buffer;
	v4l2_buffer bufferinfo;
	int type;
	bool errorFlag = false;
};

#endif //_V4L_WRAPPER_H

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
