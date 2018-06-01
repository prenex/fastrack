/// --------------------------------------------------------
/// Simple wrapper around video4linux2 to keep all low-level
/// code at a single place. Tries to use hard coded data for
/// fastest possible operations on my low-profile machines.
///
/// Useful links:
/// https://lwn.net/Articles/203924/
/// https://lightbits.github.io/v4l2_real_time/
/// https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/mmap.html
/// https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/buffer.html#c.v4l2_buffer
/// https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/buffer.html#c.v4l2_plane
/// http://jwhsmith.net/2014/12/capturing-a-webcam-stream-using-v4l2/
/// --------------------------------------------------------

#ifndef _V4L_WRAPPER_H
#define _V4L_WRAPPER_H

#include<chrono>
#include <cstdint> /*uint8_t */
#include <cmath>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/v4l2-common.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

// Define when you need debug logging data
#define V4L_WRAPPER_DEBUG_LOG 1

// define if you want exit(1) got called on errors - otherwise just the flag is set
#define EXIT_ON_ERROR 1

// when defined we try to log how much time some of the operations take
#define V4L_WRAPPER_DEBUG_TIME 1

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
		//imageFormat.fmt.pix.field = V4L2_FIELD_BOTTOM; // Only even lines? seems it does not work!
		// tell the device you are using this format
		if(ioctl(fd, VIDIOC_S_FMT, &imageFormat) < 0){
			perror("Device could not set format, VIDIOC_S_FMT");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}


		// 4. Request Buffers from the device
		requestBuffer.count = 4; // one request buffer
		//requestBuffer.count = 32; // TODO: need the code to handle it differently!
		requestBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // request a buffer wich we an use for capturing frames
		requestBuffer.memory = V4L2_MEMORY_MMAP;

		if(ioctl(fd, VIDIOC_REQBUFS, &requestBuffer) < 0){
			perror("Could not request buffer from device, VIDIOC_REQBUFS");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}

#ifdef V4L_WRAPPER_DEBUG_LOG
		printf("The number of request buffers is: %d\n", requestBuffer.count);
#endif // V4L_WRAPPER_DEBUG_LOG

		for(int i = 0; i < requestBuffer.count; ++i) {
			// 5. Query the buffer to get raw data ie. ask for the requested buffer
			// and allocate memory for it
			v4l2_buffer queryBuffer = {0};
			memset(&queryBuffer, 0, sizeof(queryBuffer));
			queryBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			queryBuffer.memory = V4L2_MEMORY_MMAP;
			queryBuffer.index = i;
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
			buffers[i] = (uint8_t*)mmap(NULL, queryBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
								fd, queryBuffer.m.offset);
#ifdef V4L_WRAPPER_DEBUG_LOG
printf("buffers[i]: %u", buffers[i]);
#endif
			memset(buffers[i], 0, queryBuffer.length);


			// 6. Prepare buffer to get a frame
			// Create a new buffer type so the device knows whichbuffer we are talking about
			memset(&bufferinfos[i], 0, sizeof(bufferinfos[i]));
			bufferinfos[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			bufferinfos[i].memory = V4L2_MEMORY_MMAP;
			bufferinfos[i].index = i;

			// Activate streaming - I have read that on some devices there must be a QUEUE buffer beforehand!!!
			if(ioctl(fd, VIDIOC_STREAMON, &bufferinfos[i].type) < 0){
				perror("Could not start streaming, VIDIOC_STREAMON");
#ifdef EXIT_ON_ERROR
				exit(1);
#endif
				errorFlag = true;
			}
		}

		memset(&bufferinfo, 0, sizeof(bufferinfo));
		bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		bufferinfo.memory = V4L2_MEMORY_MMAP;
	    //bufferinfo.index = 1; // UNUSED: only type, memory, reserved are used

		// Queue the buffer - this asks the HW to start filling the buffer
		for(int i = 0; i < requestBuffer.count; ++i) {
			if(ioctl(fd, VIDIOC_QBUF, &bufferinfos[i]) < 0){
				perror("Could not queue buffer, VIDIOC_QBUF");
#ifdef EXIT_ON_ERROR
				exit(1);
#endif
				errorFlag = true;
			}
		}
	}

	/** Closes resources - or at least tries to do so*/
	~V4LWrapper() {
		// end streaming
		for(int i = 0; i < requestBuffer.count; ++i) {
			if(ioctl(fd, VIDIOC_STREAMOFF, &bufferinfos[i].type) < 0) {
				perror("Could not end streaming, VIDIOC_STREAMOFF");
#ifdef EXIT_ON_ERROR
				exit(1);
#endif
				errorFlag = true;
			}
		}

		close(fd);
	}

	/** Should be ALWAYS called after a nextFrame after processing of memory area is done! */
	void finishFrame() {
		// Just ask the driver to refill the buffer we just processed
		if(ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0){
			perror("Could not queue buffer, VIDIOC_QBUF");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}
	}

	/**
	 * Asks the hardware to grab a frame and then wait for it to return raw data.
	 * Rem.: This method is completely synchronous and thus might lose valuable CPU time!
	 */
	uint8_t* nextFrame() {
#ifdef V4L_WRAPPER_DEBUG_TIME
		// start measuring time
		auto start = std::chrono::steady_clock::now();
#endif // V4L_WRAPPER_DEBUG_TIME
		// Dequeue the buffer - this waits here until hardware finishes
		if(ioctl(fd, VIDIOC_DQBUF, &bufferinfo) < 0){
			perror("Could not dequeue the buffer, VIDIOC_DQBUF");
#ifdef EXIT_ON_ERROR
			exit(1);
#endif
			errorFlag = true;
		}

#ifdef V4L_WRAPPER_DEBUG_TIME
		// start measuring time
		auto end = std::chrono::steady_clock::now();
#endif // V4L_WRAPPER_DEBUG_TIME
		// Frames get written after dequeuing the buffer

#ifdef V4L_WRAPPER_DEBUG_LOG
		printf("The buffer has %d KBytes of data\n", bufferinfo.bytesused / 1024);
#endif // V4L_WRAPPER_DEBUG_LOG

#ifdef V4L_WRAPPER_DEBUG_TIME
		// start measuring time
		auto diff = end - start;
		printf("Videoframe grab took %f ms\n", std::chrono::duration<double, std::milli>(diff).count());
#endif // V4L_WRAPPER_DEBUG_TIME

		return buffers[bufferinfo.index];
	}

	/** This tells the number of bytes filled into the buffer after nextFrame returns */
	unsigned int getBytesUsed() {
		return bufferinfo.bytesused;
	}
	
private:
	// A file descriptor to the video device
	int fd;
	v4l2_capability capability;
	v4l2_format imageFormat;
	v4l2_requestbuffers requestBuffer = {0};
	//uint8_t *buffer;
	uint8_t *buffers[32];
	v4l2_buffer bufferinfo; // buffer that we successfully grabbed
	v4l2_buffer bufferinfos[32];
	bool errorFlag = false;
};

#endif //_V4L_WRAPPER_H

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
