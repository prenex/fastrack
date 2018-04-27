#include <cstdio>
#include "CImg.h"
#include "homer.h"
#include "hoparser.h"

using namespace cimg_library;
int main() {
	//CImg<unsigned char> image("example_a4_small.jpg"), visu(620,900,1,3,0);
	CImg<unsigned char> image("real_test4_b.jpg"), visu(1436,900,1,3,0);
	//CImg<unsigned char> image("real_test3.jpg"), visu(620,900,1,3,0);
	//CImg<unsigned char> image("real_test2.jpg"), visu(620,900,1,3,0);
	//CImg<unsigned char> image("real_test1.jpg"), visu(620,900,1,3,0);
	const unsigned char red[] = { 255,0,0 }, green[] = { 0,255,0 }, blue[] = { 0,0,255 };
	image.blur(2.5);
	CImgDisplay main_disp(image,"Select a scanline to run Hoparser!"), draw_disp(visu,"Intensity profile and marker data");

	// Rem.: The default template arg is good for us...
	//Homer<> h;
	Hoparser<> hp;

	while (!main_disp.is_closed() && !draw_disp.is_closed()) {
		main_disp.wait();
		if (main_disp.button() && main_disp.mouse_y()>=0) {
			const int y = main_disp.mouse_y();
			visu.fill(0).draw_graph(image.get_crop(0,y,0,0,image.width()-1,y,0,0),red,1,1,0,255,0);
			visu.draw_graph(image.get_crop(0,y,0,1,image.width()-1,y,0,1),green,1,1,0,255,0);
			visu.draw_graph(image.get_crop(0,y,0,2,image.width()-1,y,0,2),blue,1,1,0,255,0).display(draw_disp);

			// Indicate a line start because we evaluate the clicked scanline
			//h.reset();
			hp.newLine();

			// Feed data into homer from the selected scanline
			int i = 0;
			for(i = 0; i < image.width(); ++i) {
				// Rem.: last value means the 'red' channel
				unsigned char redCol = image(i, y, 0, 0);
				auto foundMarker = hp.next(redCol);
				if(foundMarker) {
					// TODO: Implement something to show this marker
				}
			}
		}
	}
	return 0;
}

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
