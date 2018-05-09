#ifndef FASTTRACK_MC_PARSER_H
#define FASTTRACK_MC_PARSER_H

#include "hoparser.h"
#include "fastforwardlist.h"

#define MAX_MARKER_PER_SCANLINE 2048 // Could be smaller I guess

/**
 * A 2D marker with its confidence, order and position.
 * The confidence is useful for knowing how stable was the marker
 * The order is an integer number encoded in the marker (not ensured to be unique)
 */
struct 2DMarker {
	unsigned int x;
	unsigned int y;
	unsigned int confidence;
	unsigned int order;
};

/**
 * Defines a marker center completely. Basically a 2D marker and some further temporal data.
 */
struct MarkerCenter {
public:
	2DMarker marker;
	unsigned int minX;
	unsigned int minY;
	unsigned int maxX;
	unsigned int maxY;
	unsigned int signalCount;
private:
	// Rem.: marker.confidence is only updated with this when maxY is also updated!
	// This is needed to have the real confidence value there, but we need
	// a different variable for ending the marker center then. This is it.
	unsigned int confidenceTemp;
};

/**
 * A whole-image parser as described below.
 * Basically finds every marker1 marker on an image!
 * Parses "vertical MarkerCenters" by using an 
 * underlying Hoparser results.
 * Use next(<pixel>) call for providing magnitude data
 * Use endLine() call to indicate the end of line
 * Use endImageFrame() to get results and reset the parser
 *     for the next possible frame that might come later.
 * Rem.: Template parameters are those of Hoparser!
 */
struct ImageFrameResult{
	/** The found and properly closed MarkerCenters */
	std::vector<MarkerCenter> markerCenters;
};

template<typename MT = uint8_t, typename CT = int>
class MCParser {
public:
	/** Returns the same data as HoParser - mostly debug-only! */
	inline NexRes next(MT mag) noexcept {
		// TODO
	}

	/**
	 * Indicates that the line has ended and "next" pixels are on a following line.
	 * Rem.: Lines should be normally of the same size otherwise the algorithm can fail!
	 *       This is not a strict requirement, but no resizing will happen along the changes!
	 */
	inline void endLine() noexcept {
		// TODO
	}

	/**
	 * Ends the current image frame and returns all found 2D marker locations on the image
	 */
	inline ImageFrameResult endImageFrame() noexcept {
		// TODO
	}
private:
};

#endif // _FT_MC_PARSER_H

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
