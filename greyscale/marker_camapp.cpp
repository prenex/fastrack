// Compile with: g++ marker_camapp.cpp -lglut -lGLESv2 --std=c++14 -o marker_camapp


#define MCA_VERSION "0.1"

// There is one file where we should do these
#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

//#define _GNU_SOURCE

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <Glut/glut.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#endif
#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>
#include <string>

#include "mathelper.h"
#include "loghelper.h"

#define DEBUG 1
/* #define DEBUG_EXTRA */

/* Randomly chosen default width/heigth */
int win_width = 640;
int win_heigth = 480;

/** The location of the shader uniforms */
static GLuint ModelViewProjectionMatrix_location,
	      NormalMatrix_location,
	      LightSourcePosition_location,
	      MaterialColor_location,
	      TextureSampler_location;
/** The projection matrix */
static GLfloat ProjectionMatrix[16];
/** The direction of the directional light for the scene */
static const GLfloat LightSourcePosition[4] = { 5.0, 5.0, 10.0, 1.0};

static void printGlError(std::string where) {
	GLenum err = glGetError();
	if(err != GL_NO_ERROR) {
		LOGE((where + " - glError: 0x%x").c_str(), err);
	}
}

/**
 * Main drawing routine
 */
static void draw() {
	const static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
	const static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
	const static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };
	GLfloat transform[16];
	identity(transform);

	// Cornflowerblue for having a retro feeling from my XNA years :-)
	glClearColor(0.3921, 0.5843, 0.9294, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* Translate and rotate the view */
	translate(transform, 0, 0, -40);
	//rotate(transform, 2 * M_PI * view_rot[0] / 360.0, 1, 0, 0);
	//rotate(transform, 2 * M_PI * view_rot[1] / 360.0, 0, 1, 0);
	//rotate(transform, 2 * M_PI * view_rot[2] / 360.0, 0, 0, 1);

	// TODO: Render the scene

	// Swap the buffers and render everything
	glutSwapBuffers();

#ifdef LONGTEST
	glutPostRedisplay(); // check for issues with not throttling calls
#endif
}

/**
 * Handles a new window size or exposure.
 *
 * @param width the window width
 * @param height the window height
 */
static void handleViewportReshape(int width, int height) {
	/* Update the projection matrix */
	perspective(ProjectionMatrix, 60.0, width / (float)height, 1.0, 64.0);

	/* Set the viewport */
	glViewport(0, 0, (GLint) width, (GLint) height);
}

/**
 * Handles special glut events.
 *
 * @param special the event to handle.
 * @param x the x coordinate of the mouse when the event happened
 * @param y the y coordinate of the mouse when the event happened
 */
static void handleSpecialGlutEvents(int special, int x, int y) {
	// TODO: do whatever
	switch (special) {
		case GLUT_KEY_LEFT:
			break;
		case GLUT_KEY_RIGHT:
			break;
		case GLUT_KEY_UP:
			break;
		case GLUT_KEY_DOWN:
			break;
		case GLUT_KEY_F11:
			glutFullScreen();
			break;
	}
}

/**
 * Handles mouse-related glut events.
 *
 * @param button GLUT_LEFT_BUTTON, GLUT_MIDDLE_BUTTON, or GLUT_RIGHT_BUTTON
 * @param state GLUT_UP or GLUT_DOWN indicating whether the callback was due to a release or press
 * @param x the x coordinate of the mouse when the event happened
 * @param y the y coordinate of the mouse when the event happened
 */
static void handleMouseGlutEvents(int button, int state, int x, int y) {
	LOGI("mouse event(%d,%d,x:%d,y:%d)", button, state, x, y);
}

static void idle(void) {
	static int frames = 0;
	static double tRot0 = -1.0, tRate0 = -1.0;
	double dt, t = glutGet(GLUT_ELAPSED_TIME) / 1000.0;

	if (tRot0 < 0.0)
		tRot0 = t;
	dt = t - tRot0;
	tRot0 = t;

#ifdef TEST_MEMORYPROFILER_ALLOCATIONS_MAP
	// This file is used to test --memoryprofiler linker flag, in which case
	// add some allocation noise.
	static void *allocatedPtr = 0;
	free(allocatedPtr);
	allocatedPtr = malloc(rand() % 10485760);
#endif

	glutPostRedisplay();
	frames++;

	if (tRate0 < 0.0)
		tRate0 = t;
	if (t - tRate0 >= 5.0) {
		GLfloat seconds = t - tRate0;
		GLfloat fps = frames / seconds;
		printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds,
				fps);
		tRate0 = t;
		frames = 0;
#ifdef LONGTEST
		static runs = 0;
		runs++;
		if (runs == 4) {
			int result = fps;
#ifdef TEST_MEMORYPROFILER_ALLOCATIONS_MAP
			result = 0;
#endif
			REPORT_RESULT();
		}
#endif
	}
}

// TODO: Change accordingly
static const char vertex_shader[] =
"attribute vec3 position;\n"
"attribute vec3 normal;\n"
"attribute vec2 texcoord;\n"
"\n"
"uniform mat4 ModelViewProjectionMatrix;\n"
"uniform mat4 NormalMatrix;\n"
"uniform vec4 LightSourcePosition;\n"
"uniform vec4 MaterialColor;\n"
"\n"
"varying vec4 Color;\n"
"varying vec2 fragTex;\n"
"\n"
"void main(void)\n"
"{\n"
"    // Transform the normal to eye coordinates\n"
"    vec3 N = normalize(vec3(NormalMatrix * vec4(normal, 1.0)));\n"
"\n"
"    // The LightSourcePosition is actually its direction for directional light\n"
"    vec3 L = normalize(LightSourcePosition.xyz);\n"
"\n"
"    // Multiply the diffuse value by the vertex color (which is fixed in this case)\n"
"    // to get the actual color that we will use to draw this vertex with\n"
"    float diffuse = max(dot(N, L), 0.0);\n"
"    Color = diffuse * MaterialColor;\n"
"\n"
"    // Transform the position to clip coordinates\n"
"    gl_Position = ModelViewProjectionMatrix * vec4(position, 1.0);\n"
"    // Fill the varying textcoord for the fragment shader\n"
"    fragTex = texcoord;\n"
"}";

static const char fragment_shader[] =
"#ifdef GL_ES\n"
"precision mediump float;\n"
"#endif\n"
"varying vec4 Color;\n"
"varying vec2 fragTex;\n"
"uniform sampler2D texSampler2D;\n"
"\n"
"void main(void)\n"
"{\n"
"    vec4 texel = texture2D(texSampler2D, fragTex);\n"
//"    gl_FragColor = vec4(Color.rgb * texel.rgb, texel.a);\n"
//"    gl_FragColor = Color;\n"
"    gl_FragColor = texel;\n"
// For testing texture indices
//"    gl_FragColor = (Color + texel) + vec4(fragTex, 0.5, 1.0);\n"
"}";

static void init(char* modelFileNameAndPath) {
	GLuint v, f, program;
	const char *p;
	char msg[512];

	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	/* Compile the vertex shader */
	p = vertex_shader;
	v = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(v, 1, &p, NULL);
	glCompileShader(v);
	glGetShaderInfoLog(v, sizeof msg, NULL, msg);
	printf("vertex shader info: %s\n", msg);

	/* Compile the fragment shader */
	p = fragment_shader;
	f = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(f, 1, &p, NULL);
	glCompileShader(f);
	glGetShaderInfoLog(f, sizeof msg, NULL, msg);
	printf("fragment shader info: %s\n", msg);

	/* Create and link the shader program */
	program = glCreateProgram();
	glAttachShader(program, v);
	glAttachShader(program, f);
	// Attribute location handling is simple enough for this app
	// We just use manual values for the shader variables...
	glBindAttribLocation(program, 0, "position");
	//glBindAttribLocation(program, 1, "normal");
	glBindAttribLocation(program, 2, "texcoord");

	glLinkProgram(program);
	glGetProgramInfoLog(program, sizeof msg, NULL, msg);
	printf("info: %s\n", msg);

	/* Enable the shaders */
	glUseProgram(program);

	/* Get the locations of the uniforms so we can access them */
	ModelViewProjectionMatrix_location = glGetUniformLocation(program, "ModelViewProjectionMatrix");
	NormalMatrix_location = glGetUniformLocation(program, "NormalMatrix");
	LightSourcePosition_location = glGetUniformLocation(program, "LightSourcePosition");
	MaterialColor_location = glGetUniformLocation(program, "MaterialColor");
	TextureSampler_location = glGetUniformLocation(program, "texSampler2D");
	/* Set the LightSourcePosition uniform which is constant throught the program */
	glUniform4fv(LightSourcePosition_location, 1, LightSourcePosition);

	// Activate texture unit0 for basic texturing
	glActiveTexture(GL_TEXTURE0);
}

int main(int argc, char *argv[]) {
#ifdef TEST_MEMORYPROFILER_ALLOCATIONS_MAP
	printf("You should see an interactive CPU profiler graph below, and below that an allocation map of the Emscripten main HEAP, with a long blue block of allocated memory.\n");
#endif
	printf("argc:%d\n", argc);
	/* Initialize the window */
	glutInit(&argc, argv);
	glutInitWindowSize(win_width, win_heigth);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

	glutCreateWindow("The fasttrack marker realtime tester " MCA_VERSION);

	/* Set up glut callback functions */
	// This let us use "real" keyboard movements
	// without this there is key repeat and the
	// signals are being sent continously after
	// the first signal + waiting and continous..
	// Like in the terminal - which is bad as
	// we can't record keydown/keyup pairs!
	glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
	glutIdleFunc(idle);
	glutReshapeFunc(handleViewportReshape);
	glutDisplayFunc(draw);
	glutSpecialFunc(handleSpecialGlutEvents);
	glutMouseFunc(handleMouseGlutEvents);

	/* Do our initialization */
	if(argc == 2) {
		// User provided the model to load
		init(argv[1]);
	} else {
		init(nullptr);
	}

	glutMainLoop();

	return 0;
}


// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
