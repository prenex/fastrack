#ifndef _GV_PNP_CALCULATOR_H
#define _GV_PNP_CALCULATOR_H
// X11 library conflicts with Eigen3 if this is not done
// This is supported by msvc, gcc and most llvm compilers so it is fine...
#pragma push_macro("Success")
#undef Success

// Rem.: This defines Success as macro to its own value in eigen3...
#include <opengv/absolute_pose/methods.hpp>
#include <opengv/absolute_pose/CentralAbsoluteAdapter.hpp>
#include <opengv/math/cayley.hpp>

class GvPnPCalculator {
	/** Returns the 3D pose estimation of the camera using n data point correspondances between normalized screen coordinates and absolute world-space coordinates. */
	NOINLINE PoseRes3D calculate(unsigned int n, double *screen_xy, double *world_xyz) {
		// TODO: IMPLEMENT PROPERLY!

		// Just zero out the result completely first. This ensures (0,0,0) position.
		PoseRes3D result;
		for(int i = 0; i < FT_TRANSFORM_MATRIX_SIZE; ++i) {
			result.transform[i] = 0;
		}
		// Fill the diagonal with ones: this gives an identity matrix for rotation.
		result.transform[0] = 1;
		result.transform[5] = 1;
		result.transform[10] = 1;

		// Return the identity matrix with no translation
		return result;
	}
};

// X11 library conflicts with Eigen3 if this is not done
// This is supported by msvc, gcc and most llvm compilers so it is fine...
#pragma pop_macro("Success")
#endif // _GV_PNP_CALCULATOR_H

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
