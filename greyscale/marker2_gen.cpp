#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <cerrno>
#include <climits>
#include "CImg.h"
using namespace cimg_library;

#define DEF_SIZE_X 512
#define DEF_SIZE_Y 512

#define DEF_CIRCLE_SIZE 200
#define DEF_CIRCLE_STEP 6

const char* outfile = "marker2.bmp";

struct Rgb {
	unsigned char r;
	unsigned char g;
	unsigned char b;

public:
	Rgb() {
		r = 0; g = 0; b = 0;
	}

	Rgb(unsigned char rr, unsigned char gg, unsigned char bb) {
		r = rr;
		g = gg;
		b = bb;
	}
};

const unsigned char black[] = { 0,0,0 };

enum STR2INT_ERROR { SUCCES, OVERFL, UNDERFL, INCONVERTIBLE };

// base-10 is the default
STR2INT_ERROR str2int(int &i, char const *s, int base = 0)
{
    char *end;
    long  l;
    errno = 0;
    l = strtol(s, &end, base);
    if ((errno == ERANGE && l == LONG_MAX) || l > INT_MAX) {
        return OVERFL;
    }
    if ((errno == ERANGE && l == LONG_MIN) || l < INT_MIN) {
        return UNDERFL;
    }
    if (*s == '\0' || *end != '\0') {
        return INCONVERTIBLE;
    }
    i = l;
    return SUCCES;
}

void printUsage() {
	printf("USAGE:\n");
	printf("------\n");
	printf("\n");
	printf("marker2_gen                        - generate default marker\n");
	printf("marker2_gen --help                 - show this message\n");
	printf("marker2_gen <csize>                - use provided circle size\n");
	printf("marker2_gen <csize> <cstep>        - use circle size and circle step size (size of concentric slices)\n");
	printf("marker2_gen <csize> <cstep> <size> - use circle size, circle step size and marker width\n\n");
}

// IN: now, whole_size
// OUT: current color
Rgb calcSliceColor(float i, float circleStepSize) {
	// Ratio is in [0,1]
	double ratio = i / circleStepSize;
	// Square ratio to make the black->white function less steep
	ratio = (ratio * ratio);
	double cf = ratio * 255;
	int ci = (cf > 255) ? 255 : (int) cf;
	// TODO: maybe use some real function to do greyscale->RGB?
	return Rgb(ci, ci, ci);
}

// Draw enough concentric circles to construct a slice. Because we do this with circles, we need 
void drawConcentricSlice(CImg<unsigned char> &marker, int midx, int midy, int size, int circleStepSize) {
	for(int i = 0; i < size; ++i) {
		Rgb col = calcSliceColor(i, circleStepSize);
		marker.draw_circle(midx, midy, size-i, (unsigned char*)&col);
	}
}

/** Main entry point */
int main(int argc, const char** argv) {

	int circleStep = DEF_CIRCLE_STEP;
	int circleSize = DEF_CIRCLE_SIZE;
	int sizeX = DEF_SIZE_X;
	int sizeY = DEF_SIZE_Y;

	// Parse parameters
	// <csize> and --help
	if(argc > 1) {
		std::string firstParam(argv[1]);
		if(firstParam == "--help") {
			printUsage();
			return 0;
		} else {
			int cs;
			auto res = str2int(cs, argv[1]);
			if(res != SUCCES) {
				printUsage();
				return 0;
			} else {
				// Set circle size as provided
				circleSize = cs;
			}

			// <cstep>
			if(argc > 2) {
				int cst;
				auto res = str2int(cst, argv[2]);
				if(res != SUCCES) {
					printUsage();
					return 0;
				} else {
					// Set circle step
					circleStep = cst;
				}
			}

			// <size>
			if(argc > 3) {
				int ms;
				auto res = str2int(ms, argv[3]);
				if(res != SUCCES) {
					printUsage();
					return 0;
				} else {
					// Set marker size
					sizeX = ms;
					sizeY = ms;
				}
			}
		}
	}

	// Do stuff
	int midx = sizeX / 2;
	int midy = sizeY / 2;
	CImg<unsigned char> marker(sizeX,sizeY,1,3,0);
	marker.fill(255);

	// Draw the circles
	for(int i = circleStep - 1; i > 0; --i) {
		int size = (i * circleSize) / circleStep;
		int circleStepSize = (circleSize / circleStep);
		if(i == 1){
			// Last - inner - circle!
			// This must be completely black
			marker.draw_circle(midx, midy, size-i, black);
		} else {
			// All other circles
			drawConcentricSlice(marker, midx, midy, size, circleStepSize);
		}
	}

	Rgb centerColor;
	centerColor.g = 255;
	marker.draw_point(midx, midy, (unsigned char*)&centerColor);

	std::string title = std::string("Generated marker - click to save as ") + outfile;
	CImgDisplay draw_disp(marker, title.c_str());
	bool isClickedOn = false;
	while (!draw_disp.is_closed() && ! isClickedOn) {
		draw_disp.wait();
		if (draw_disp.button() && draw_disp.mouse_y()>=0) {
			isClickedOn = true;
		}
	}

	// Save the marker if the user clicked on it (and not just closed the window)
	if(isClickedOn) {
		marker.save(outfile);
	}

	return 0;
}

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
