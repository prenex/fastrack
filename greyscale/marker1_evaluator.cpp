#include <cstdio>
#include "CImg.h"
#include "homer.h"

using namespace cimg_library;
int main() {
	//CImg<unsigned char> image("example_a4_small.jpg"), visu(620,900,1,3,0);
	CImg<unsigned char> image("real_test4_b.jpg"), visu(1436,900,1,3,0);
	//CImg<unsigned char> image("real_test3.jpg"), visu(620,900,1,3,0);
	//CImg<unsigned char> image("real_test2.jpg"), visu(620,900,1,3,0);
	//CImg<unsigned char> image("real_test1.jpg"), visu(620,900,1,3,0);
	const unsigned char red[] = { 255,0,0 }, green[] = { 0,255,0 }, blue[] = { 0,0,255 };
	image.blur(2.5);
	CImgDisplay main_disp(image,"Click a point"), draw_disp(visu,"Intensity profile");

	// Rem.: The default template arg is good for us...
	Homer<> h;

	while (!main_disp.is_closed() && !draw_disp.is_closed()) {
		main_disp.wait();
		if (main_disp.button() && main_disp.mouse_y()>=0) {
			const int y = main_disp.mouse_y();
			visu.fill(0).draw_graph(image.get_crop(0,y,0,0,image.width()-1,y,0,0),red,1,1,0,255,0);
			visu.draw_graph(image.get_crop(0,y,0,1,image.width()-1,y,0,1),green,1,1,0,255,0);
			visu.draw_graph(image.get_crop(0,y,0,2,image.width()-1,y,0,2),blue,1,1,0,255,0).display(draw_disp);

			// Reset the "homer"
			h.reset();

			// Feed data into homer from the selected scanline
			int i = 0;
			for(i = 0; i < image.width(); ++i) {
				// Rem.: last value means the 'red' channel
				unsigned char redCol = image(i, y, 0, 0);
				bool ho = h.isHo();
				auto len = h.getLen();
				auto avg = h.magAvg();
				h.next(redCol);

				if(!h.isHo() && ho) {
					// Here when ended a "ho"8gg
					printf("AVG: %d at LEN: %d @ i = %d\n", avg, len, i);
				}
			}

			if(h.isHo()) {
				// Here when ended a "ho" in the end of the scanline
				printf("=== AVG: %d at LEN: %d @ i = %d\n", h.magAvg(), h.getLen(), i);
			} else {
				printf("===\n");
			}
		}
	}
	return 0;
}

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
