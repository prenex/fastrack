#ifndef FASTTRACK_MC_PARSER_H
#define FASTTRACK_MC_PARSER_H

#include "hoparser.h"

/**
 * A 2D marker with its confidence and position.
 */
struct 2DMarker {
	unsigned int x;
	unsigned int y;
	unsigned int confidence;
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
struct ImageFrameRes{
	// TODO: Maybe replace with our custom - cache-friendly and array based - forward list???
	//       Or keep it like this because of easier built-in std::move and stuff???
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
	inline ImageFrameRes endImageFrame() noexcept {
		// TODO
	}
private:
};

#endif // _FT_MC_PARSER_H
