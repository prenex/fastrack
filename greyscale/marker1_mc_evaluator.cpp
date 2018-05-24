//#define NO_ATTRITION 1 // fastest operation (2x speedup - but harder to use proper parameters)
#define SIMPLE_ATTRITION 1 // second fastest operation mode (nearly as good as the NO_ATTRITION)
// Enable this to draw some of the debug points and log some more info as in the marker1_eval application
/*
#define DEBUG_POINTS 1
#define MC_DEBUG_LOG 1
*/

#include <cstdio>
#include <string>
#include <iostream>
#include <chrono>
#include "CImg.h"
#include "homer.h"
#include "hoparser.h"
#include "mcparser.h"

using namespace cimg_library;

#define TEST_FILE_DEFAULT "real_test4_b.jpg"

/** Draws a measurable-sized dot as a "box" around a pixel */
CImg<unsigned char>& drawBoxAround(CImg<unsigned char> &img, int x, int y, unsigned char* color) {
	return img.draw_point(x, y, color)
		.draw_point(x-1, y-1, color)
		.draw_point(x, y-1, color)
		.draw_point(x+1, y-1, color)
		.draw_point(x-1, y, color)
		.draw_point(x+1, y, color)
		.draw_point(x-1, y+1, color)
		.draw_point(x, y+1, color)
		.draw_point(x+1, y+1, color);
}

void printUsageAndQuit() {
	printf("USAGE:\n");
	printf("------\n\n");

	printf("marker1_mc_eval                  - test with " TEST_FILE_DEFAULT "\n");
	printf("marker1_mc_eval my_img.png       - test with my_img.png\n");
	printf("marker1_mc_eval --help           - show this message\n");

	// Quit immediately!
	exit(0);
}

int main(int argc, char** argv) {
	std::string testFile(TEST_FILE_DEFAULT);
	std::string help("--help");

	// Handle possible --help or image parameter
	if(argc == 2) {
		// Print help if they ask
		if(argv[1] == help) {
			printUsageAndQuit();
		}

		// Use provided test file if they provide
		testFile = std::string(argv[1]);
	} else if(argc > 2) {
		// Print help if they need
		printUsageAndQuit();
	}

	CImg<unsigned char> image(testFile.c_str());
	//image.blur(2.5);
	CImg<unsigned char> visu(image.width(),image.height(),1,3,0);
	CImg<unsigned char> lenAffImg(800,100,1,3,0);

	// Create a 'bitmap' so that we do not need to measure CImg effectivity
	// in our test throughout the application!
	std::vector<unsigned char> origPixels(image.width() * image.height(), 0);
	for(int y = 0; y < image.height(); ++y) {
		for(int x = 0; x < image.width(); ++x) {
			// TODO: should be actually grey magnitude, but red channel is good-enough!
			unsigned char redCol = image(x, y, 0, 0);
			origPixels[x + y*image.width()] = redCol;
		}
	}
	

	printf("Processing image (%d, %d) with pixelcount: %d\n", image.width(), image.height(), (int)origPixels.size());

	//CImg<unsigned char> image("real_test3.jpg"), visu(620,900,1,3,0);
	//CImg<unsigned char> image("real_test2.jpg"), visu(620,900,1,3,0);
	//CImg<unsigned char> image("real_test1.jpg"), visu(620,900,1,3,0);
	const unsigned char red[] = { 255,0,0 }, green[] = { 0,255,0 }, blue[] = { 0,0,255 };
	CImgDisplay lenAffDisp(lenAffImg, "The length-based value affection test window");
	CImgDisplay draw_disp(visu,"Intensity profile and marker data");
	CImgDisplay main_disp(image,"Select a scanline to run Hoparser!");

	// Rem.: The default template arg is good for us...
	//Hoparser<> hp;
	MCParser<> mcp;
	LenAffectParams params;

	unsigned long long nextCallNo = 0; // counting total next(..) pixel calls
	while (!main_disp.is_closed() && !draw_disp.is_closed() && !lenAffDisp.is_closed()) {
		main_disp.wait();
		if (main_disp.button() && main_disp.mouse_y()>=0) {
			const int y = main_disp.mouse_y();
			if(main_disp.button()&1) { // left-click

#ifdef DEBUG_POINTS 
				// Draw length affectedness testing values
				unsigned char lenAffCol[] = { 255,0,0 };
				for(int j = 0; j < lenAffImg.width(); ++j) {
					auto affected = lenAffect(10, j, params);
					lenAffCol[0] = affected;
					lenAffCol[1] = affected;
					lenAffCol[2] = affected;
					lenAffImg.draw_line(j, 0, j, lenAffImg.height(), (unsigned char*)&lenAffCol);
				}
				lenAffImg.display(lenAffDisp);
#endif // DEBUG_POINTS 

				// start measuring time
				auto start = std::chrono::steady_clock::now();

				// Parse all the scanlines properly
				int fullSize = image.height()*image.width();
				for(int j = 0; j < fullSize; j+=image.width()) {
					for(int i = 0; i < image.width(); ++i) {
						// Rem.: The last value means the 'red' channel
						//       and we can use that to approximate the greyscale :-)
						//unsigned char redCol = image(i, j, 0, 0);
						unsigned char redCol2 = origPixels[i + j];
						//if(redCol != redCol2) printf("%d != %d\n", redCol, redCol2);
						auto res = mcp.next(redCol2);

#ifdef DEBUG_POINTS 
						++nextCallNo;
						// do this only for debugging?
						// These should not be on the image actually
						if(res.isToken) {
							// Cant do this now as it overwrites the next scanline:
							// drawBoxAround(image, i, j, (unsigned char*)&blue);
							// Muh better debug indicator:
							image.draw_point(i, j, (unsigned char*)&blue);
						}
						// Check for 1D marker result
						if(res.foundMarker) {
							// Log and show this marker centerX
							int centerX = mcp.tokenizer.getMarkerX();
							auto order = mcp.tokenizer.getOrder();
							if(order > 2) {
								printf("*** Found marker at %d and centerX: %d and order: %d***\n", i, centerX, order);
								drawBoxAround(image, centerX, j, (unsigned char*)&green);
							} else {
								printf("*** Found marker at %d and centerX: %d and order: %d***\n", i, centerX, order);
							}
						}
#endif // DEBUG_POINTS 
					}

					// Notify MCParser about the end of the line
					mcp.endLine();
				}

				// notify MCParserv about the end of the image frame and get the results
				auto results = mcp.endImageFrame();

				// Calculate time of run
				auto endCalc = std::chrono::steady_clock::now();
				auto diff = endCalc - start;
				std::cout << "calculation took " << std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;

				// TODO: show the results
				printf("Found %d 2D markers on the photo!\n", (int)results.markers.size());
				for(int i = 0; i < results.markers.size(); ++i) {
					auto mx = results.markers[i].x;
					auto my = results.markers[i].y;
					auto mc = results.markers[i].confidence;
					auto mo = results.markers[i].order;
					printf(" - (%d, %d)*%d @ %d confidence!\n", mx, my, mo, mc);
					// Draw them all :) 
					drawBoxAround(image, mx, my, (unsigned char*)&red);
				}

				// Update image
				image.display(main_disp);
			} else if(main_disp.button()&2) { // right-click
				// Log mouse click position
				printf("--- (X, Y) position of the mouse on right click: (%d, %d)\n", main_disp.mouse_x(), y);

				// Show graphs
				visu.fill(0).draw_graph(image.get_crop(0,y,0,0,image.width()-1,y,0,0),red,1,1,0,255,0);
				visu.draw_graph(image.get_crop(0,y,0,1,image.width()-1,y,0,1),green,1,1,0,255,0);
				visu.draw_graph(image.get_crop(0,y,0,2,image.width()-1,y,0,2),blue,1,1,0,255,0).display(draw_disp);
			}
		}
	}
#ifdef DEBUG_POINTS 
	printf("Number of total next(<pixel>) calls: %llu\n", nextCallNo);
#endif // DEBUG_POINTS

	return 0;
}

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
