#include <vector> /* vector */
#include <cstdint> /*uint8_t */

void asdf(std::vector<uint8_t> &testVec, uint8_t r) {
	testVec.resize(5);
	testVec[0] = r;
}

int main() {
	std::vector<uint8_t> testVec;
	asdf(testVec, 0);

	return testVec[0];
}
