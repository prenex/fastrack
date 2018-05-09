#include <cstdio>
#include "fastforwardlist.h"

int main() {
	printf("Testing fastforwardlist.h...\n");

	// Create a maximum 128 length list
	FastForwardList<int, 128> ffl;

	// (A) Simple test
	printf("SIMPLE TEST:\n");

	auto pos = ffl.head();
	for(int i = 0; i < 42; ++i) {
		ffl.push_front(i);
	}

	auto readHead = ffl.head();
	bool first = true;
	while(!readHead.isNil()) {
		if(!first) printf(", "); else first = false;
		auto val = ffl[readHead];
		printf("%d", val);
		readHead = ffl.next(readHead);
	}
	printf("\n");

	// (B) Middle-insert test
	printf("MIDDLE-INSERT TEST:\n");
	// Adding doubled values after every even number
	readHead = ffl.head();
	while(!readHead.isNil()) {
		auto val = ffl[readHead];
		if((val&1) == 0) {
			// We are here when we have an odd value

			// Rem.: This does not move the read head
			//       so the inserted value we will
			//       need to step over!!!
			ffl.insertAfter(val * 2, readHead);
			readHead = ffl.next(readHead);
		}
		readHead = ffl.next(readHead);
	}

	// Writeout after testing
	readHead = ffl.head();
	first = true;
	while(!readHead.isNil()) {
		if(!first) printf(", "); else first = false;
		auto val = ffl[readHead];
		printf("%d", val);
		readHead = ffl.next(readHead);
	}
	printf("\n");

	// (C) Overfilling and reset test
	printf("Overfilling TEST:\n");
	while(ffl.push_front(42)) {};
	// Writeout after testing
	readHead = ffl.head();
	first = true;
	while(!readHead.isNil()) {
		if(!first) printf(", "); else first = false;
		auto val = ffl[readHead];
		printf("%d", val);
		readHead = ffl.next(readHead);
	}
	printf("\n");
	printf("Reset TEST:\n");
	// reset
	ffl.reset();
	// Writeout after reset
	readHead = ffl.head();
	first = true;
	while(!readHead.isNil()) {
		if(!first) printf(", "); else first = false;
		auto val = ffl[readHead];
		printf("%d", val);
		readHead = ffl.next(readHead);
	}
	// add a value
	ffl.push_front(777);
	printf("- now added a value(should be 777):\n");
	// Writeout after reset + added value
	readHead = ffl.head();
	first = true;
	while(!readHead.isNil()) {
		if(!first) printf(", "); else first = false;
		auto val = ffl[readHead];
		printf("%d", val);
		readHead = ffl.next(readHead);
	}
	printf("\n");

	// TODO: Delete test (when there will be deletion)

	printf("...testing fastforwardlist.h ended!\n");
}

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
