/* g++ webcam_capture.cpp -o webcam_capture.out --std==c++14 */

#include <iostream>
#include <stdio.h>
#include <cstdlib>
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
#include <string.h>
#include <fstream>
#include <string>
#include <vector> /* vector */
#include <cstdint> /*uint8_t */

using namespace std;

// Magic constants for YUYV -> RGB8 transform
const int K1 = int(1.402f * (1 << 16));
const int K2 = int(0.714f * (1 << 16));
const int K3 = int(0.334f * (1 << 16));
const int K4 = int(1.772f * (1 << 16));

// RGB: maximum squared distance between the marker color(s) and the found color to classify as a marker color
const float RGB_MAX_MARKER_DIFF_SQ_DIST = 0.0314f;

// In some POCs we paint using this color when we are too far from the marker color (showing only markercolors on the whole pic for clarity)
const uint8_t NO_MARK_COLOR_R = 0x00f;
const uint8_t NO_MARK_COLOR_G = 0x00f;
const uint8_t NO_MARK_COLOR_B = 0x00f;

// RGB: Marker colors to look for
const float RGB_MARK1_R = 0xff / (float)(0xff+0x55+0x38);
const float RGB_MARK1_G = 0x55 / (float)(0xff+0x55+0x38);
const float RGB_MARK1_B = 0x38 / (float)(0xff+0x55+0x38);
// TODO: other 3 marker colors are needed here!

// YUYV: Marker colors and max dist is currently TODO!

// Constrains between min and max with saturation
static inline void saturate(int& value, int min_val, int max_val) {
	if (value < min_val) value = min_val;
	if (value > max_val) value = max_val;
}

// Returns true if the given rgb triplet is a marker color
bool isMarkerColor(int R, int G, int B) {
	float r = (float)R / (float)(R+G+B);
	float g = (float)G / (float)(R+G+B);
	float b = (float)B / (float)(R+G+B);
	float rDiff = fabs(r - RGB_MARK1_R);
	float gDiff = fabs(g - RGB_MARK1_G);
	float bDiff = fabs(b - RGB_MARK1_B);

	auto sqDist = rDiff*rDiff + gDiff*gDiff + bDiff*bDiff;
	//printf("sqDist = %f\n", sqDist);

	return (sqDist < RGB_MAX_MARKER_DIFF_SQ_DIST);
}

// Converts YUV 4:2:2 to an RGB_888 vector - possibly growing the target vector if needed.
// The target vector will contain only the corresponding rgb888 result and nothing else
std::vector<uint8_t> convertToRgb888FromYuv422(uint8_t *yuv422s, int yuvlen) {
    	std::vector<uint8_t> rgb888Block;	// vector of the rgb888 data for the current block
	// 4byte = 2 pixels in yuv422!
	for(int i = 0; i < yuvlen / 4; ++i) {
		uint8_t y1  = (uint8_t)yuv422s[i*4];
		uint8_t u = (uint8_t)yuv422s[i*4 + 1];
		uint8_t y2  = (uint8_t)yuv422s[i*4 + 2];
		uint8_t v = (uint8_t)yuv422s[i*4 + 3];

		int8_t uf = u - 128;
		int8_t vf = v - 128;

		// Pixel1
		// v0 (pastebin):
		int r = y1 + (K1*vf >> 16);
		int g = y1 - (K2*vf >> 16) - (K3*uf >> 16);
		int b = y1 + (K4*uf >> 16);
 
		saturate(r, 0, 255);
		saturate(g, 0, 255);
		saturate(b, 0, 255);
		// v1 (interesting):
//		float r = (uint8_t)(y1 + 1.4065 * (v - 128));
//		float g = (uint8_t)(y1 - 0.3455 * (u - 128) - (0.7169 * (v - 128)));
//		float b = (uint8_t)(y1 + 1.7790 * (u - 128));
		// This prevents colour distortions in your rgb image
//		if (r < 0) r = 0;
//		else if (r > 255) r = 255;
//		if (g < 0) g = 0;
//		else if (g > 255) g = 255;
//		if (b < 0) b = 0;
//		else if (b > 255) b = 255;
		// v2:
//		uint8_t r = uint8_t(y1 + 1.140*v);
//		uint8_t g = uint8_t(y1 - 0.395*u - 0.581*v);
//		uint8_t b = uint8_t(y1 + 2.032*u);
		// Grayscale (simple):
		//uint8_t r = y1;
		//uint8_t g = y1;
		//uint8_t b = y1;

		// By only looking for the marker color for pixel0s
		// we can still show a picture whilst highlight the areas where we find stuff!
		if(isMarkerColor(r, g, b)) {
			rgb888Block.push_back((uint8_t)r);
			rgb888Block.push_back((uint8_t)g);
			rgb888Block.push_back((uint8_t)b);
		} else {
			rgb888Block.push_back(NO_MARK_COLOR_R);
			rgb888Block.push_back(NO_MARK_COLOR_G);
			rgb888Block.push_back(NO_MARK_COLOR_B);
		}

		// Pixel2
		// v0 (pastebin):
		r = y2 + (K1*vf >> 16);
		g = y2 - (K2*vf >> 16) - (K3*uf >> 16);
		b = y2 + (K4*uf >> 16);
 
		saturate(r, 0, 255);
		saturate(g, 0, 255);
		saturate(b, 0, 255);
		// v1 (interesting):
//		r = (uint8_t)(y2 + 1.4075 * (v - 128));
//		g = (uint8_t)(y2 - 0.3455 * (u - 128) - (0.7169 * (v - 128)));
//		b = (uint8_t)(y2 + 1.7790 * (u - 128));
		// This prevents colour distortions in your rgb image
//		if (r < 0) r = 0;
//		else if (r > 255) r = 255;
//		if (g < 0) g = 0;
//		else if (g > 255) g = 255;
//		if (b < 0) b = 0;
//		else if (b > 255) b = 255;
		// v2:
//		r = uint8_t(y2 + 1.140*v);
//		g = uint8_t(y2 - 0.395*u - 0.581*v);
//		b = uint8_t(y2 + 2.032*u);
		// Grayscale (simple):
		//r = y2;
		//g = y2;
		//b = y2;
		rgb888Block.push_back((uint8_t)r);
		rgb888Block.push_back((uint8_t)g);
		rgb888Block.push_back((uint8_t)b);
	}

	return rgb888Block;
}

int main() {
    // 1.  Open the device
    int fd; // A file descriptor to the video device
    fd = open("/dev/video0",O_RDWR);
    if(fd < 0){
        perror("Failed to open device, OPEN");
        return 1;
    }


    // 2. Ask the device if it can capture frames
    v4l2_capability capability;
    if(ioctl(fd, VIDIOC_QUERYCAP, &capability) < 0){
        // something went wrong... exit
        perror("Failed to get device capabilities, VIDIOC_QUERYCAP");
        return 1;
    }


    // 3. Set Image format
    v4l2_format imageFormat;
    imageFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    imageFormat.fmt.pix.width = 640;
    imageFormat.fmt.pix.height = 480;
    imageFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    imageFormat.fmt.pix.field = V4L2_FIELD_NONE;
    // tell the device you are using this format
    if(ioctl(fd, VIDIOC_S_FMT, &imageFormat) < 0){
        perror("Device could not set format, VIDIOC_S_FMT");
        return 1;
    }


    // 4. Request Buffers from the device
    v4l2_requestbuffers requestBuffer = {0};
    requestBuffer.count = 1; // one request buffer
    requestBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // request a buffer wich we an use for capturing frames
    requestBuffer.memory = V4L2_MEMORY_MMAP;

    if(ioctl(fd, VIDIOC_REQBUFS, &requestBuffer) < 0){
        perror("Could not request buffer from device, VIDIOC_REQBUFS");
        return 1;
    }


    // 5. Quety the buffer to get raw data ie. ask for the you requested buffer
    // and allocate memory for it
    v4l2_buffer queryBuffer = {0};
    queryBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    queryBuffer.memory = V4L2_MEMORY_MMAP;
    queryBuffer.index = 0;
    if(ioctl(fd, VIDIOC_QUERYBUF, &queryBuffer) < 0){
        perror("Device did not return the buffer information, VIDIOC_QUERYBUF");
        return 1;
    }
    // use a pointer to point to the newly created buffer
    // mmap() will map the memory address of the device to
    // an address in memory
    char* buffer = (char*)mmap(NULL, queryBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                        fd, queryBuffer.m.offset);
    memset(buffer, 0, queryBuffer.length);


    // 6. Get a frame
    // Create a new buffer type so the device knows whichbuffer we are talking about
    v4l2_buffer bufferinfo;
    memset(&bufferinfo, 0, sizeof(bufferinfo));
    bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufferinfo.memory = V4L2_MEMORY_MMAP;
    bufferinfo.index = 0;

    // Activate streaming
    int type = bufferinfo.type;
    if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
        perror("Could not start streaming, VIDIOC_STREAMON");
        return 1;
    }

/***************************** Begin looping here *********************/
    // Queue the buffer
    if(ioctl(fd, VIDIOC_QBUF, &bufferinfo) < 0){
        perror("Could not queue buffer, VIDIOC_QBUF");
        return 1;
    }

    // Dequeue the buffer
    if(ioctl(fd, VIDIOC_DQBUF, &bufferinfo) < 0){
        perror("Could not dequeue the buffer, VIDIOC_DQBUF");
        return 1;
    }
    // Frames get written after dequeuing the buffer

    cout << "Buffer has: " << (double)bufferinfo.bytesused / 1024
            << " KBytes of data" << endl;

    // Write the data out to file
    ofstream outFileYuv422;
    ofstream outFileRgb888;
    outFileYuv422.open("webcam_output.yuv422.data", ios::binary| ios::app);
    outFileRgb888.open("webcam_output.rgb888.data", ios::binary| ios::app);

    int bufPos = 0, outFileMemBlockSize = 0;  // the position in the buffer and the amoun to copy from
                                        // the buffer
    int remainingBufferSize = bufferinfo.bytesused; // the remaining buffer size, is decremented by
                                                    // memBlockSize amount on each loop so we do not overwrite the buffer
    std::vector<uint8_t> rgb888Block;	// vector of the rgb888 data for the current block
    char* outFileMemBlock = NULL;  // a pointer to a new memory block
    int itr = 0; // counts thenumber of iterations
    while(remainingBufferSize > 0) {
        bufPos += outFileMemBlockSize;  // increment the buffer pointer on each loop
                                        // initialise bufPos before outFileMemBlockSize so we can start
                                        // at the begining of the buffer

        outFileMemBlockSize = 1024;    // set the output block size to a preferable size. 1024 :)
        outFileMemBlock = new char[sizeof(char) * outFileMemBlockSize];

        // copy 1024 bytes of data starting from buffer+bufPos
        memcpy(outFileMemBlock, buffer+bufPos, outFileMemBlockSize);
        outFileYuv422.write(outFileMemBlock, outFileMemBlockSize);

	// Convert into RGB
	rgb888Block = convertToRgb888FromYuv422((uint8_t*)outFileMemBlock, outFileMemBlockSize);
	// Write out bytes into the rgb file too
	//printf("Writing %d rgb byte!", rgb888Block.size());
	outFileRgb888.write((const char*)(&rgb888Block[0]), rgb888Block.size());

        // calculate the amount of memory left to read
        // if the memory block size is greater than the remaining
        // amount of data we have to copy
        if(outFileMemBlockSize > remainingBufferSize)
            outFileMemBlockSize = remainingBufferSize;

        // subtract the amount of data we have to copy
        // from the remaining buffer size
        remainingBufferSize -= outFileMemBlockSize;

        // display the remaining buffer size
        cout << itr++ << " Remaining bytes: "<< remainingBufferSize << endl;
	// TODO: delete[] the outFileMemBlock!!!
    }

    // Close the file
    outFileYuv422.close();
    outFileRgb888.close();


/******************************** end looping here **********************/

    // end streaming
    if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
        perror("Could not end streaming, VIDIOC_STREAMOFF");
        return 1;
    }

    close(fd);
    return 0;
}

