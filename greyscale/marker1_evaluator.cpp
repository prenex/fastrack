#include <cstdio>
#include "CImg.h"
#include "homer.h"
#include "hoparser.h"

using namespace cimg_library;

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

int main() {
	// TODO: use command line parameter(s) for image creation?
	//CImg<unsigned char> image("example_a4_small.jpg"), visu(620,900,1,3,0);
	CImg<unsigned char> image("real_test4_b.jpg");
	CImg<unsigned char> visu(image.width(),image.height(),1,3,0);
	CImg<unsigned char> lenAffImg(800,100,1,3,0);

	// Copy image
	CImg<unsigned char> origImage = image;
	//CImg<unsigned char> image("real_test3.jpg"), visu(620,900,1,3,0);
	//CImg<unsigned char> image("real_test2.jpg"), visu(620,900,1,3,0);
	//CImg<unsigned char> image("real_test1.jpg"), visu(620,900,1,3,0);
	const unsigned char red[] = { 255,0,0 }, green[] = { 0,255,0 }, blue[] = { 0,0,255 };
	image.blur(2.5);
	CImgDisplay lenAffDisp(lenAffImg, "The length-based value affection test window");
	CImgDisplay draw_disp(visu,"Intensity profile and marker data");
	CImgDisplay main_disp(image,"Select a scanline to run Hoparser!");

	// Rem.: The default template arg is good for us...
	//Homer<> h;
	Hoparser<> hp;
	LenAffectParams params;

	while (!main_disp.is_closed() && !draw_disp.is_closed() && !lenAffDisp.is_closed()) {
		main_disp.wait();
		if (main_disp.button() && main_disp.mouse_y()>=0) {
			const int y = main_disp.mouse_y();
			if(main_disp.button()&1) { // left-click
				visu.fill(0).draw_graph(image.get_crop(0,y,0,0,image.width()-1,y,0,0),red,1,1,0,255,0);
				visu.draw_graph(image.get_crop(0,y,0,1,image.width()-1,y,0,1),green,1,1,0,255,0);
				visu.draw_graph(image.get_crop(0,y,0,2,image.width()-1,y,0,2),blue,1,1,0,255,0).display(draw_disp);

				// Indicate a line start because we evaluate the clicked scanline
				//h.reset();
				hp.newLine();

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

				// Feed data into homer from the selected scanline
				int i = 0;
				for(i = 0; i < image.width(); ++i) {
					// Rem.: last value means the 'red' channel
					unsigned char redCol = image(i, y, 0, 0);
					auto res = hp.next(redCol);

					// TODO: maybe do this only for debugging?
					if(res.isToken) {
						drawBoxAround(image, i, y, (unsigned char*)&blue).display(main_disp);
					}

					// CHECK FOR FINAL RESULTS!
					if(res.foundMarker) {
						// Log and show this marker centerX
						int centerX = hp.getMarkerX();
						printf("*** Found marker at %d and centerX: %d ***\n", i, centerX);
						drawBoxAround(image, centerX, y, (unsigned char*)&green).display(main_disp);
					}
				}
			} else if(main_disp.button()&2) { // right-click
				// Log mouse click position
				printf("--- (X, Y) position of the mouse on right click: (%d, %d)\n", main_disp.mouse_x(), y);
				// Reset image
				image.assign(origImage);
			}
		}
	}
	return 0;
}

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
