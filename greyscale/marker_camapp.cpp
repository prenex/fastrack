// Sample application that runs 2D marker tracking on /dev/video0 camera
//
// Compile with: g++ marker_camapp.cpp -lGL -lX11 -o marker_camapp
// When having very slow (5FPS) camera speed turn off auto_exposure:
//
// $ v4l2-ctl -d /dev/video0 -L
//                      brightness (int)    : min=-127 max=127 step=1 default=0 value=0
//                        contrast (int)    : min=0 max=127 step=1 default=64 value=64
//                      saturation (int)    : min=0 max=255 step=1 default=64 value=64
//                             hue (int)    : min=-16000 max=16000 step=1 default=0 value=0
//  white_balance_temperature_auto (bool)   : default=1 value=1
//                           gamma (int)    : min=16 max=500 step=1 default=100 value=100
//            power_line_frequency (menu)   : min=0 max=2 default=1 value=1
// 				0: Disabled
// 				1: 50 Hz
// 				2: 60 Hz
//       white_balance_temperature (int)    : min=2800 max=6500 step=1 default=5200 value=5200 flags=inactive
//                       sharpness (int)    : min=1 max=29 step=1 default=8 value=8
//                   exposure_auto (menu)   : min=0 max=3 default=3 value=1
// 				1: Manual Mode
// 				3: Aperture Priority Mode
//               exposure_absolute (int)    : min=1 max=2500 step=1 default=156 value=156
//          exposure_auto_priority (bool)   : default=0 value=1
// $ v4l2-ctl -d /dev/video0 "--set-ctrl=exposure_auto=1"
// #This helps too - at least for me:
// $ v4l2-ctl -d /dev/video0 "--set-ctrl=white_balance_temperature_auto=0"
// $ v4l2-ctl -d /dev/video0 "--set-ctrl=exposure_absolute=512"
//
// So basically you can turn off most "auto" things freely to suckless on lower end machines!

// ======== //
// SETTINGS //
// ======== //

#define WIN_XPOS 256
#define WIN_YPOS 64
#define WIN_XRES 640
#define WIN_YRES 480
/*#define WIN_XRES 320
#define WIN_YRES 240*/
#define NUM_SAMPLES 1

// Must be the same as WIN_*RES as of now!
#define CAM_XRES 640
#define CAM_YRES 480
/*##define CAM_XRES 320
#define CAM_YRES 240*/

// MUST BE HERE FOR TECHNICAL REASONS to have uint8_t for below!
#include <cstdint> // (*)

// Pixel-buffer to render as greyscale to the screen
// Rem.: Must have same size as camera buf as of now!
static uint8_t pixBuf[WIN_XRES * WIN_YRES];

// ===== //
// DEBUG //
// ===== //

// Define this if you want your last frame image to be saved on errors
#define SAVE_LAST_FRAME_ON_FFL_ASSERT 1

// Rem.: This must be before mcparser include as that includes fastforwardlist.h
#ifdef SAVE_LAST_FRAME_ON_FFL_ASSERT
// because we look for a cryptic error and want to save the frame
// we redefine what happens when assertions fail
#define FFL_ASSERT myassertfun
// We only include CImg (for image saving) when we need to!
#include "CImg.h"
#define LAST_FRAME_FILE "lastErrorFrame.png"
// The assert function
void myassertfun(bool pred) {
	// check if assertion failed
	if(!pred) {
		// Create CImg from the last camera frame
		cimg_library::CImg<unsigned char> lastFrameImg(CAM_XRES,CAM_YRES,1,3,0);
		cimg_forXY(lastFrameImg,x,y) {
			// Rem.: red channel is used in marker1_mceval
			lastFrameImg(x,y,0,0) = pixBuf[x + y*CAM_XRES];
		}
		// Save the last camera frame - if we can
		lastFrameImg.save(LAST_FRAME_FILE);

		// Print to the user that it has been saved
		fprintf(stderr, "SOME ASSERT FAILED! Saved the erronous frame as: " LAST_FRAME_FILE "\n");

		// Quit application immediately!
		exit(1);
	}
}
#endif

// ======== //
// Includes //
// ======== //

#include <cstdio>
#include <cstdlib>
#include <cstdint> // Have it already at (*), but better show up here too...
#include <cctype>
#include <cstring>
#include <sys/time.h>
#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>

#include <vector> /* vector */

// Use this for wrapping video4linux
#include "v4lwrapper.h"

// MarkerCenter frame parser
#include "mcparser.h" 

// ==== //
// CODE //
// ==== //

MCParser<> mcp;

struct MyWin {
	Display  *display;
	Window win;
	int displayed;
	int width;
	int height;
};

struct MyWin Win;

double elapsedMsec(const struct timeval *start, const struct timeval *stop) {
	return ((stop->tv_sec  - start->tv_sec ) * 1000.0 +
			(stop->tv_usec - start->tv_usec) / 1000.0);
}

/** This is where we need to show our frames */
void draw() {

	/** Created at first call of draw() and stays the same through the app run */
	static V4LWrapper<CAM_XRES, CAM_YRES> cameraWrapper;

	glClear(GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	/*
	glBegin(GL_TRIANGLES);
	glColor3f(1.0f, 0.0f, 0.0f);
	glVertex3f( 0.0f, 1.0f, 0.0f);
	glVertex3f(-1.0f, -1.0f, 0.0f);
	glVertex3f( 1.0f, -1.0f, 0.0f);
	glEnd();
	*/

	uint8_t *rawData = cameraWrapper.nextFrame();

	// the position in the buffer and the amoun to copy from the buffer; The latter is 0 here because of hackz for first loop!
	int bufPos = 0, memBlockSize = 0; 
	// the remaining buffer size, is decremented by memBlockSize on each loop so we do not overwrite the buffer
	int remainingBufferSize = cameraWrapper.getBytesUsed();
	int lineOffset = 0;
	int destOffset = 0;
	// For each line:
    while(remainingBufferSize > 0) {
		bufPos += memBlockSize;
		// We read the whole line as a buffered reading
		memBlockSize = CAM_XRES * (4/2); // 4byte = 2pixel in YUYV so reading 2 bytes get us a greyscale pixel!
        //memcpy(pixBuf, rawData+bufPos, memBlockSize);

		for(int i = 0; i < memBlockSize; i += 2) {
			// YUYV - so the two relevant grey values are in the Ys (every second byte)
			uint8_t mag = (rawData+bufPos)[i];

			// TODO: run marker detection code here with MCParser::next(mag);
			auto res = mcp.next(mag);
#ifdef DEBUG_POINTS // TODO
			// do this only for debugging?
			// These should not be on the image actually
			if(res.isToken) {
				// Cant do this now as it overwrites the next scanline:
				// drawBoxAround(image, i, j, (unsigned char*)&blue);
				// Much better debug indicator:
				//image.draw_point(i, j, (unsigned char*)&blue);
			}
			// Check for 1D marker result
			if(res.foundMarker) {
				// Log and show this marker centerX
				int centerX = mcp.tokenizer.getMarkerX();
				auto order = mcp.tokenizer.getOrder();
				if(order >= 2) {
					printf("*** Found marker at %d and centerX: %d and order: %d***\n", i, centerX, order);
					//drawBoxAround(image, centerX, j, (unsigned char*)&green);
				} else {
					printf("*** Found marker at %d and centerX: %d and order: %d***\n", i, centerX, order);
				}
			}
#endif // DEBUG_POINTS 

			//Write our output buffer for showing the results
			pixBuf[destOffset++] = mag;
		}

		// Check for end of frame special cases...
        if(memBlockSize > remainingBufferSize)
            memBlockSize = remainingBufferSize;

        // Subtract the amount of data we have to copy
        // from the remaining buffer size
        remainingBufferSize -= memBlockSize;

		// Increment line offset
		lineOffset += memBlockSize;

		mcp.endLine();
	}
	// Ends the frame: both for my parser and v4l2
	// ! NEEDED !
	cameraWrapper.finishFrame(); // TODO: might be optimised further by different loops for my processing
	auto results = mcp.endImageFrame();

	// Show the results
	printf("Found %d 2D markers on the photo!\n", (int)results.markers.size());
	for(int i = 0; i < results.markers.size(); ++i) {
		auto mx = results.markers[i].x;
		auto my = results.markers[i].y;
		auto mc = results.markers[i].confidence;
		auto mo = results.markers[i].order;
		printf(" - (%d, %d)*%d @ %d confidence!\n", mx, my, mo, mc);
		// TODO: draw out markers
		pixBuf[mx + my*CAM_XRES] = 255;
	}

	glDrawPixels(WIN_XRES, WIN_YRES, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixBuf);

	glFlush();
	glXSwapBuffers(Win.display, Win.win);
}

void keyboardCB(KeySym sym, unsigned char key, int x, int y,
		int *setting_change) {
	switch (tolower(key)) {
		case 27:
			exit(EXIT_SUCCESS);
			break;
		case 'k':
			printf("You hit the 'k' key\n");
			break;
		case 0:
			switch (sym) {
				case XK_Left  :
					printf("You hit the Left Arrow key\n");
					break;
				case XK_Right :
					printf("You hit the Right Arrow key\n");
					break;
			}
			break;
	}
}

void reshapeCB(int width, int height) {
	Win.width = width;
	Win.height = height;
	glViewport(0, 0, width, height);
	//glMatrixMode(GL_PROJECTION);
	//glLoadIdentity();
	//glFrustum(-1.0, 1.0, -1.0, 1.0, 1.5, 20.0);
	//glMatrixMode(GL_MODELVIEW);
}

/* Try to find a framebuffer config that matches
 * the specified pixel requirements.
 */
GLXFBConfig chooseFBConfig(Display *display, int screen) {
	static const int Visual_attribs[] = {
		GLX_X_RENDERABLE  , True,
		GLX_DRAWABLE_TYPE , GLX_WINDOW_BIT,
		GLX_RENDER_TYPE   , GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE , GLX_TRUE_COLOR,
		GLX_RED_SIZE	  , 8,
		GLX_GREEN_SIZE	  , 8,
		GLX_BLUE_SIZE	  , 8,
		GLX_ALPHA_SIZE	  , 8,
		GLX_DEPTH_SIZE	  , 16,
		GLX_STENCIL_SIZE  , 8,
		GLX_DOUBLEBUFFER  , True,
		GLX_SAMPLE_BUFFERS, 1,
		GLX_SAMPLES		  , 1,
		None
	};
	int attribs [ 100 ] ;
	memcpy(attribs, Visual_attribs, sizeof(Visual_attribs));
	GLXFBConfig ret = 0;
	int fbcount;
	GLXFBConfig *fbc = glXChooseFBConfig(display, screen, attribs, &fbcount);
	if (fbc) {
		if (fbcount >= 1)
			ret = fbc[0];
		XFree(fbc);
	}
	return ret;
}

GLXContext createContext(Display *display, int screen,
		GLXFBConfig fbconfig, XVisualInfo *visinfo, Window window) {
#define GLX_CONTEXT_MAJOR_VERSION_ARB		0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB		0x2092
	typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*,
			GLXFBConfig, GLXContext, int, const int*);
	/* Verify GL driver supports glXCreateContextAttribsARB() */
	/*	 Create an old-style GLX context first, to get the correct function ptr. */
	glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
	GLXContext ctx_old = glXCreateContext(display, visinfo, 0, True);
	if (!ctx_old) {
		printf("Could not even allocate an old-style GL context!\n");
		exit(EXIT_FAILURE);
	}
	glXMakeCurrent (display, window, ctx_old) ;
	/* Verify that GLX implementation supports the new context create call */
	if (strstr(glXQueryExtensionsString(display, screen),
				"GLX_ARB_create_context") != 0)
		glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
			glXGetProcAddress((const GLubyte *) "glXCreateContextAttribsARB");
	if (!glXCreateContextAttribsARB) {
		printf("Can't create new-style GL context\n");
		exit(EXIT_FAILURE);
	}
	/* Got the pointer.  Nuke old context. */
	glXMakeCurrent(display, None, 0);
	glXDestroyContext(display, ctx_old);

	/* Try to allocate a GL 2.1 COMPATIBILITY context */
	static int Context_attribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
		GLX_CONTEXT_MINOR_VERSION_ARB, 1,
		GLX_CONTEXT_PROFILE_MASK_ARB , GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
		/*GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, */
		/*GLX_CONTEXT_FLAGS_ARB		  , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, */
		/*GLX_CONTEXT_FLAGS_ARB		  , GLX_CONTEXT_DEBUG_BIT_ARB, */
		None
	};
	GLXContext context = glXCreateContextAttribsARB(display, fbconfig, 0,
			True, Context_attribs);
	/* Forcably wait on any resulting X errors */
	XSync(display, False);
	if (!context) {
		printf("Failed to allocate a GL 2.1 context\n");
		exit(EXIT_FAILURE);
	}
	printf("Created GL 2.1 context\n");
	return context;
}

void createWindow() {
	/* Init X and GLX */
	Win.displayed = 0;
	Display *display = Win.display = XOpenDisplay(":0.0");
	if (!display)
		printf("Cannot open X display\n");
	int    screen	= DefaultScreen(display);
	Window root_win = RootWindow(display, screen);
	if (!glXQueryExtension(display, 0, 0))
		printf("X Server doesn't support GLX extension\n");
	/* Pick an FBconfig and visual */
	GLXFBConfig fbconfig = chooseFBConfig(display, screen);
	if (!fbconfig) {
		printf("Failed to get GLXFBConfig\n");
		exit(EXIT_FAILURE);
	}
	XVisualInfo *visinfo = glXGetVisualFromFBConfig(display, fbconfig);
	if (!visinfo) {
		printf("Failed to get XVisualInfo\n");
		exit(EXIT_FAILURE);
	}
	printf("X Visual ID = 0x%.2x\n", (int)visinfo->visualid);
	/* Create the X window */
	XSetWindowAttributes winAttr ;
	winAttr.event_mask = StructureNotifyMask | KeyPressMask ;
	winAttr.background_pixmap = None ;
	winAttr.background_pixel  = 0	 ;
	winAttr.border_pixel	  = 0	 ;
	winAttr.colormap = XCreateColormap(display, root_win,
			visinfo->visual, AllocNone);
	unsigned int mask = CWBackPixmap | CWBorderPixel | CWColormap | CWEventMask;
	Window win = Win.win = XCreateWindow (display, root_win,
			WIN_XPOS, WIN_YPOS,
			WIN_XRES, WIN_YRES, 0,
			visinfo->depth, InputOutput,
			visinfo->visual, mask, &winAttr) ;
	XStoreName(Win.display, win, "My GLX Window");
	/* Create an OpenGL context and attach it to our X window */
	GLXContext context = createContext(display, screen, fbconfig, visinfo, win);
	if (! glXMakeCurrent(display, win, context))
		printf("glXMakeCurrent failed.\n");
	if (! glXIsDirect (display, glXGetCurrentContext()))
		printf("Indirect GLX rendering context obtained\n");
	/* Display the window */
	XMapWindow(display, win);
	if (! glXMakeCurrent(display, win, context))
		printf("glXMakeCurrent failed.\n");
	printf("Window Size    = %d x %d\n", WIN_XRES, WIN_YRES);
	printf("Window Samples = %d\n", NUM_SAMPLES);
}

void processXEvents(Atom wm_protocols, Atom wm_delete_window) {
	int setting_change = 0;
	while (XEventsQueued(Win.display, QueuedAfterFlush)) {
		XEvent event;
		XNextEvent(Win.display, &event);
		if(event.xany.window != Win.win)
			continue;
		switch (event.type) {
			case MapNotify:
				{
					Win.displayed = 1;
					break;
				}
			case ConfigureNotify:
				{
					XConfigureEvent cevent = event.xconfigure;
					reshapeCB(cevent.width, cevent.height);
					break;
				}
			case KeyPress:
				{
					char chr;
					KeySym symbol;
					XComposeStatus status;
					XLookupString(&event.xkey, &chr, 1, &symbol, &status);
					keyboardCB(symbol, chr, event.xkey.x, event.xkey.y,
							&setting_change);
					break;
				}
			case ClientMessage:
				{
					if (event.xclient.message_type == wm_protocols &&
							(Atom)event.xclient.data.l[0] == wm_delete_window) {
						exit(EXIT_SUCCESS);
					}
					break;
				}
		}
	}
}

void mainLoop() {
	/* Register to receive window close events (the "X" window manager button) */
	Atom wm_protocols	  = XInternAtom(Win.display, "WM_PROTOCOLS"    , False);
	Atom wm_delete_window = XInternAtom(Win.display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(Win.display, Win.win, &wm_delete_window, True);

	while(1) {
		/* Redraw window (after it's mapped) */
		if (Win.displayed)
			draw();

		/* Update frame rate */
		struct timeval last_xcheck = {0, 0};
		struct timeval now;
		gettimeofday(&now, 0);

		/* Check X events every 250ms second - as we should not do this too much! */
		if (elapsedMsec(&last_xcheck, &now) > 250) {
			processXEvents(wm_protocols, wm_delete_window);
			last_xcheck = now;
		}
	}
}

int main(int argc, char *argv[]) {
	Win.width = WIN_XRES;
	Win.height = WIN_YRES;
	createWindow();

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_FLAT);

	printf("Valid keys: Left, Right, k, ESC\n");
	printf("Press ESC to quit\n");
	mainLoop();
	return EXIT_SUCCESS;
}

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
