#include <cstdio>

// You need to define this if you want to test the range checks (and with them)
//#define FFL_INSERT_RANGE_CHECK 1

#include "fastforwardlist.h"

int main() {
	printf("Testing fastforwardlist.h...\n");

	// Create a maximum 127 length list
	// Rem.: (2^x)-1 is the most optimal and the smaller the better!
	FastForwardList<int, 127> ffl;

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

	// (C) Range-check and reset test
#ifdef FFL_INSERT_RANGE_CHECK
	printf("Range-check TEST:\n");
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
#endif
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

	// Delete test (when there will be deletion)
	printf("Delete TEST:\n");

	// Remove head
	ffl.unlinkHead();

	// Writeout after delete test No.1
	readHead = ffl.head();
	first = true;
	while(!readHead.isNil()) {
		if(!first) printf(", "); else first = false;
		auto val = ffl[readHead];
		printf("%d", val);
		readHead = ffl.next(readHead);
	}
	printf("\n");

#ifdef FFL_INSERT_RANGE_CHECK
	printf("Unlink range check...\n");
	// Nothing should happen when range checking is on
	for(int i = 0; i < 1024; ++i) {
		ffl.unlinkHead();
	}
	printf("...Unlink range check OK!\n");
#endif

	// 10 9 8 7 6 5 4 3 2 1 0
	FFLPosition zeroPos;
	bool firstIteration = true;
	for(int i = 0; i < 11; ++i) {
		ffl.push_front(i);
		// Save the position of head when zero is the head!
		if(firstIteration) {
			zeroPos = ffl.head();
			firstIteration = false;
		}
	}

	// Reorder it as incremented:
	// - insert value AFTER the earlier list
	// - unlink the values - except until reaching zeroPos
	// Should result in: 10 0 1 2 3 4 5 6 7 8 9
	// (only 0 stayed in its original memory slot however)
	FFLPosition writeHead = zeroPos;
	FFLPosition unlinkHead = ffl.head(); // keep 10 (head elem) but unlink after that!
	readHead = ffl.head();
	readHead = ffl.next(readHead);
	// turn around numbers in an other list
	while(readHead != zeroPos) {
		ffl.insertAfter(ffl[readHead], writeHead);
		writeHead = ffl.next(writeHead);
		readHead = ffl.unlinkAfter(unlinkHead);
	}

	// Writeout after delete test No.2: complex big testing!
	readHead = ffl.head();
	first = true;
	while(!readHead.isNil()) {
		if(!first) printf(", "); else first = false;
		auto val = ffl[readHead];
		printf("%d", val);
		readHead = ffl.next(readHead);
	}
	printf("\n");


	printf("...testing fastforwardlist.h ended!\n");
}

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
