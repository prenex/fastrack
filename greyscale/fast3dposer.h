#ifndef FT_3D_QR_POSER
#define FT_3D_QR_POSER

#include "hoparser.h" // class NexRes
#include "mcparser.h" // McParser return values
#include "microshackz.h" // NOINLINE and stuff for micro-optimizations

#define FT_TRANSFORM_MATRIX_SIZE 12

/**
 * Result of the 3D poser: the id read out from the QR-code (if any) and the transform.
 */
struct PoseRes3D {
	/** A 3x4 transformation matrix */
	double transform[FT_TRANSFORM_MATRIX_SIZE];

	/** Read transform position into the given 3 variables for the x,y,z coordinates of the camera */
	void readPosInto(double &x, double &y, double &z) {
		// TODO: Ensure this is right in OpenGV or the matrix ordering is different maybe!
		// The last column of the 4x3 matrix is the translation vector. The other parts are a 3x3 rotation matrix.
		x = transform[3];
		y = transform[3+4];
		z = transform[3+8];
	}
};

/**
 * Example class for perspective-n-point calculations. This acts to be the default bogus implementation showing what methods are needed if _USE_OPENGV is not specified!
 */
class NopPnPCalculator {
	/** Returns the 3D pose estimation of the camera using n data point correspondances between normalized screen coordinates and absolute world-space coordinates. */
	NOINLINE PoseRes3D calculate(unsigned int n, double *screen_xy, double *world_xyz) {
		// Just zero out the result completely first. This ensures (0,0,0) position.
		PoseRes3D result;
		for(int i = 0; i < FT_TRANSFORM_MATRIX_SIZE; ++i) {
			result.transform[i] = 0;
		}
		// TODO: Ensure this is right in OpenGV or the matrix ordering is different maybe!
		// Fill the diagonal with ones: this gives an identity matrix for rotation.
		result.transform[0] = 1;
		result.transform[5] = 1;
		result.transform[10] = 1;

		// Return the identity matrix with no translation
		return result;
	}
};

// We use OpenGV if asked to. That is a small library and contains a lot of PnP implementations
#ifdef _USE_OPENGV
#include "gv_pnpcalculator.h"
/** Uses the given fasttrack MCParser to get 3D pose estimates from circle-patterned QR codes and by default uses OpenGV perspective-4-point algorithms */
template<typename PnPCalculator = GvPnPCalculator, typename MT = uint8_t, typename CT = int, typename MCP = MCParser<MT, CT>>
#else
/** Uses the given fasttrack MCParser to get 3D pose estimates from circle-patterned QR codes and the given perspective-4-point algorithm provider */
template<typename PnPCalculator = NopPnPCalculator, typename MT = uint8_t, typename CT = int, typename MCP = MCParser<MT, CT>>
#endif // _USE_OPENGV
class Fast3DPoser {
private:
	// The MCParser as given by the user
	MCP mcp;
	PnPCalculator pnp;
public:
	/** FEED OF THE NEXT MAGNITUDE: Returns the "isToken" data if available - mostly debug-only return value! */
	inline NexRes next(MT mag) noexcept {
		return mcp.next(mag);
	}

	/**
	 * Indicates that the line has ended and "next" pixels are on a following line.
	 * Rem.: Lines should be normally of the same size otherwise the algorithm can fail!
	 *       This is not a strict requirement, but no resizing will happen along the changes!
	 */
	inline void endLine() noexcept {
		mcp.endLine();
	}

	/**
	 * Ends the current image frame and returns all found 2D marker locations on the image.
	 * Rem.: The returned reference is only valid until the next() function is called once again.
	 * Rem.: This algorithm is not completely online - the 2D->3D calculations mostly happen here!
	 */
	inline const PoseRes3D endImageFrame() noexcept {
		// Get 2D marker results
		ImageFrameResult mcres = mcp.endImageFrame();

		// TODO: Calculate 3D camera pose estimate
		PoseRes3D res;

		// Return the 3D camera pose estimate
		return res;
	}
};

#endif // FT_3D_QR_POSER

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
