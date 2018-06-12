#ifndef FT_3D_QR_POSER
#define FT_3D_QR_POSER

// X11 library conflicts with Eigen3 if this is not done
// This is supported by msvc, gcc and most llvm compilers so it is fine...
#pragma push_macro("Success")
#undef Success

// Rem.: This defines Success as macro to its own value in eigen3...
#include <opengv/absolute_pose/methods.hpp>
#include <opengv/absolute_pose/CentralAbsoluteAdapter.hpp>
#include <opengv/math/cayley.hpp>

/** Uses fasttrack MCParser to get 3D pose estimates from circle-patterned QR codes */
class Qr3DPoser {

}

// X11 library conflicts with Eigen3 if this is not done
// This is supported by msvc, gcc and most llvm compilers so it is fine...
#pragma pop_macro("Success")
#endif // FT_3D_QR_POSER

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
