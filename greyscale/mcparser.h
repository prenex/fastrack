#ifndef FASTTRACK_MC_PARSER_H
#define FASTTRACK_MC_PARSER_H

#include <cstdint>
#include <cstdlib>
#include "hoparser.h"
#include "fastforwardlist.h"

#ifndef MAX_MARKER_PER_SCANLINE // Let the users define this
#define MAX_MARKER_PER_SCANLINE 1024 // Could be smaller I guess
#endif

// Best to keep these values as-is
// Most of them are hand-picked to be good both for speed and usage!
#ifndef MAX_ORDER
#define MAX_ORDER 5
#endif
#ifndef MIN_ORDER
#define MIN_ORDER 2
#endif
#define START_CONFIDENCE 50

/**
 * A 2D marker with its confidence, order and position.
 * The confidence is useful for knowing how stable was the marker - the bigger is better.
 * The order is an integer number encoded in the marker (not ensured to be unique)
 */
struct 2DMarker {
	unsigned int x;
	unsigned int y;
	unsigned int confidence;
	unsigned int order;
};

struct MCParserConfig {
	/**
	 * Ignore every suspected marker-pos in the scanlines
	 * which has smaller order than this value
	 */
	unsigned int ignoreOrderSmallerThan = 2;

	/**
	 * Maximum difference between the marker center position 
	 * in different scanlines to consider it to be a continuation
	 * and "extension" of the last scanlines marker-center position.
	 * When bigger is the value, we "SKIP" extending at this scanline.
	 */
	unsigned int deltaDiffMax = 10;

	/**
	 * Maximum difference between the minimum and maximum X coordinates
	 * in a MarkerCenter when consecutive scanlines would grow this
	 * difference too much. When bigger, we "SKIP" extending.
	 */
	unsigned int widthDiffMax = 50;

	/** Marker is closed if there was no pixel for it in the last 50 rows */
	unsigned int closeDiffY = 50;
}

/**
 * Defines a marker center we are currently suspecting
 */
struct MarkerCenter {
public:
	/** Result of a tryExtend(..) operation */
	enum class ExtendResult {
		SKIPPED,
		EXTENDED,
		CLOSED,
	}

	// Always the X coordinate of the last-line center from hoparser
	// !! SHOULD BE FIRST!!!
	unsigned int lastX;
	// Mininal X
	unsigned int minX;
	// Maximal X
	unsigned int maxX;
	// Initialized when the marker centerline is "started"
	unsigned int minY;
	// Incremented whenever the marker centerline is "extended"
	unsigned int maxY;
	// Number of "elements" below each other that constitute a center
	unsigned int signalCount;
	// Confidence between minY and maxY
	int confidence;

	/** Construct with un-initialized data */
	MarkerCenter() {	}

	/** Construct with data to "start" a marker centerline right now */
	MarkerCenter(unsigned int x, unsigned int y, uint8_t order) {
		// This might load cache of next object
		// This ensures faster addition nearby each in some cases
		// due to bogus and random cache optimizations :D
		for(int i = 0; i < (1 + MAX_ORDER - MIN_ORDER); ++i) {
			ord[i] = 0;
		} // hope for loop unrolling here...
		// This loads cache for our data
		lastX = x;
		// Fill in other data linearly
		minX = x;
		maxX = x;
		minY = y;
		maxY = y;
		signalCount = 1;
		confidence = START_CONFIDENCE;
		confidenceTemp = START_CONFIDENCE;
		// We try to count the various supported order values
		// and return the most common one later qwhen closing this.
		ord[order + MIN_ORDER - 1] = 1;
	}

	/**
	 * FORCE SKIP happened for this scanline.
	 * Should be called when this marker center was not found / skipped
	 * in the current scanline and we know this in advance for some reason.
	 * Returns the calculated temporal confidence value after the skip.
	 */
	inline unsigned int skipUpd() {
		return --confidenceTemp;
	}

	/**
	 * Tells if this marker-center should be "closed" according to the given values.
	 * Rem.: This method expects that we read scanlines from top-to-bottom order!
	 *       If that is not the case, try simulating it by thinking in other order.
	 *       If there is no order, this cannot work, if the order is bottom-to-top,
	 *       Then just imagine values that other way around and it will work unchanged!
	 */
	inline bool shouldClose(unsigned int y, unsigned int maxYDiff) {
		// y is always bigger or equal than the current maxY here
		// so no abs(..) is needed
		return ((y - maxY) > maxYDiff);
	}

	/**
	 * Should be called when this marker center should be tried to be extended.
	 * Returns true when extended and false when skipped extension in which case
	 * skipUpd() was called internally.
	 */
	inline bool tryExtend(unsigned int x, unsigned int y, uint8_t order,
		   	unsigned int deltaDiffMax, unsgined int widthDiffMax) {
		// lastX access loads cache properly
		if(abs(lastX - x) > deltaDiffMax) {
			// Skipped
			skipUpd();
			return false;
		}

		// Check width constrain
		// Rem.: This might load cache of next object
		// This ensures faster addition nearby each in some cases
		// due to bogus and random cache optimizations :D
		unsigned int newMinX = (minX < x) ? minX : x;
		unsigned int newMaxX = (maxX > x) ? maxX : x;
		if((newMaxX - newMinX) > widthDiffMax) {
			// Skipped
			skipUpd();
			return false;
		}

		// When here, we are extending!
		// Update the "last X coordinate" for delta checking the next line
		lastX = x;

		// Update order counts
		ord[order + MIN_ORDER - 1] = 1;

		// Update min-max X
		minX = newMinX;
		maxX = newMaxX;

		// update min-max Y
		// Rem.: because of ordered reading we can automatically update maxY!
		maxY = y;

		// Increase confidence counter
		++confidenceTemp;
		// Count how many markers we had (signal quality?)
		++signalCount;

		// We update "confidence" to return only when we can 'extend' successfully
		// Rem.: this means that this value is always showing the 'real'
		//       confidence between minY and maxY and not counting the 
		//       lot of missing values AFTER the marker center.
		confidence = confidenceTemp;
	}

	/**
	 * Construct a marker using the centerline information we have gathered so far.
	 * Rem.: Best used when this center became "CLOSED" (but will return data anyways)
	 */
	inline 2DMarker constructMarker() {
		// TODO: maybe do better than this avarage?
		unsigned int x = ((maxX - minX) / 2) + minX;
		unsigned int y = ((maxY - minY) / 2) + minY;

		unsigned int order = 2;
		uint8_t orderMaxCnt = ord[0];
		for(int i = 1; i < (1 + MAX_ORDER - MIN_ORDER); ++i) {
			if(ord[i] > orderMaxCnt) {
				orderMaxCnt = ord[i];
				order = i+2; // 1 and 0 is unused to spare space
			}
		} // hope for loop unrolling here...

		return 2DMarker{
			x, y,
			confidence,
			order
		};
	}

private:
	// Rem.: marker.confidence is only updated with this when maxY is also updated!
	// This is needed to have the real confidence value there, but we need
	// a different variable for ending the marker center then. This is it.
	int confidenceTemp;

	// Number of order2s, order3s, order4s, order5s among per-scanline data
	// counted in ord[0], ord[1], ord[2], ord[3] respectively
	// We need this so that we can return with the most probable one
	// !! SHOULD BE LAST - or close to end
	uint8_t ord[MAX_ORDER - MIN_ORDER];
};

/**
 * Result of parsing marker centers in an image frame
 */
struct ImageFrameResult{
	/** Build using the found and properly closed MarkerCenters */
	std::vector<2DMarker> markers;
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
template<typename MT = uint8_t, typename CT = int>
class MCParser {
public:

	/** Create a markercenter-parser with default configuration */
	MCParser() {
	}

	/** Create a markercenter-parser with the given configuration */
	MCParser(MCParserConfig parserConfig) {
		config = parserConfig;
	}

	/** Returns the same data as HoParser - mostly debug-only! */
	inline NexRes next(MT mag) noexcept {
		auto ret = hp.next();

		// See if the hoparser finds a(n 1D) marker at the pixel in this scanline
		if(ret.foundMarker) {
			// get marker data
			int centerX = hp.getMarkerX();
			auto order = hp.getOrder();
			if(config.ignoreOrderSmallerThan < order) {
				// If not too small to ignore, process it!

				// PRE-READ TECHNIQUE
				// ==================
				//
				// Advance list position when we are the first test on a newline
				// And the list is not empty. The second handles cases when the
				// list is completely empty
				// TODO: ensure this line is OK:
				if(afterNewLine && !(mcCurrentList.isEmpty())) {
					lastPos = listPos;
					listPos = mcCurrentList.next(listPos);
					afterNewLine = false;
				}

				// AFTER THIS POINT WE ARE IN THE INVARIANT!
				// =========================================
				//
				// The best way to imagine this is to imagine always two lists:
				// - A list of already suspected marker centers up until the last scanline
				// - And a list of "1D markers at this scanline"
				//
				// These two lists are ordered by the x-coordinate (the first is ordered by
				// its "lastX") and now in this place we are processing them to make a
				// vertical parsing. If we would have these two lists at every scanline end
				// then we could go through both of them and update the first list with the 
				// latter new line data list.
				//
				// In reality however this is an on-line algorithm so there are no two
				// seperate lists - just this "next(..)" function that gets the pixels
				// by some provider as a source and the list about what we think where
				// the markers center lines are! The existing list is the one we named
				// the "first" above and we are ourselves in the iterator of the other
				// list actually: we do that second one on-the-fly without collecting.
				// At the first moment we had to move the "first" list to a valid pos.
				// if that was possible as next(..) call indicates there is a valid
				// suspected pixel in this scanline! (see marker1_eval test app for
				// these green pixels when you are clicking in that application).
				// 
				// Rem.: This is basically a two-variabled-one-valued elementwise
				//       processing algorithm for unification update if the list.
				//       The difference is that this is an "online" working version.
				//       (*): In Hungarian, this special case is an "Időszerűsítés".

				// Add this new token we have found as a new marker centerline or
				// extend an already existing marker centerline!
				bool tokenProcessed = false;
				while(!tokenProcessed) {
					if(listPos.isNil()) {
						// End of list is reached and the token is not processed yet.
						// In this case we need to add it to the end of the list right
						// after the last list Position. We need to insert at the last
						// valid position - we cannot insert at a NIL position as that
						// means insertion at the head - the case of empty list is 
						// handled here too and exactly that way as you can see!
						mcCurrentList.insertAfter(
								std::move(MarkerCenter(x, y, order)),
							   	lastPos);
						tokenProcessed = true;
					} else {
						// Compare if we can merge the next()-ed element into the list
						// position element... For this we better get a reference to it
						MarkerCenter &currentCenter = mcCurrentList[listPos];

						// Try extending the existing element
						// This is a NO-OP when we cannot extend it
						bool extendedIt = currentCenter.tryExtend(
								x, y, order, 
								config.deltaDiffMax, config.widthDiffMax);

						// See if we succeeded or not
						if(extendedIt) {
							// Because if we did, we processed this token:
							// Both the lists moves and the user can provide next()
							// so if there would be two lists, we would move with both
							// and in this case we move the current list and exit the
							// loop so that the next() function can be called with the
							// next token-pixel that is a marker center somewhere...
							// These both happen in an x-ordered way!
							tokenProcessed = true;
							lastPos = listPos;
							listPos = mcCurrentList.next(listPos);
						} else {
							// If we did not succeed, we need to see if the element
							// is so much before the one in the earlier list that
							// it should be added as a new element at the current
							// lastPos insertion position or not:
							if((((currentCenter.maxX - currentCenter.minX) / 2) - (config.widthDiffMax / 2)) > x) {
								// Completely new suspected marker - in the middle of the list
								mcCurrentList.insertAfter(
										std::move(MarkerCenter(x, y, order)),
										lastPos);
								// Mark this token as processed
								tokenProcessed = true;
							} else { // Rem.: This else is necessary or we would need to step with the lastPos too!
								// See if things indicate we need to close the earlier found stuff
								if(currentCenter.shouldClose(y, config.closeDiffY)) {
									// Add the generated marker from it to the frame results
									// Rem.: This adds poor quality markers too, but with small confidence
									frameResult.markers.push_back(currentCenter.constructMarker());

									// Close / Unlink the added one as it is considered to be closed!
									mcCurrentList.unlinkAfter(lastPos);
								}
							}

							// If we did still haven't succeeded, then we still always step
							// with the earlier list and try finding the next element.
							lastPos = listPos;
							listPos = mcCurrentList.next(listPos);
						}
					}
				}
			}
		}

		return ret;
	}

	/**
	 * Indicates that the line has ended and "next" pixels are on a following line.
	 * Rem.: Lines should be normally of the same size otherwise the algorithm can fail!
	 *       This is not a strict requirement, but no resizing will happen along the changes!
	 */
	inline void endLine() noexcept {
		// Reset read head in the ordered list
		suspectionPos = NIL_POS;
		// Reset x book-keeping
		x = 0;
		// Increment y book-keeping
		++y;
		// Indicate newline
		afterNewLine = true;
	}

	/**
	 * Ends the current image frame and returns all found 2D marker locations on the image.
	 * Rem.: The returned reference is only valid until the next() function is called once again.
	 */
	inline const ImageFrameResult endImageFrame() noexcept {
		// TODO: Check - I just hope this is optimal and works too...
		// This both resets the original variable
		// and returns the collection of markers.
		ImageFrameResult ret;
		std::swap(frameResult, ret);
		return std::move(ret);
	}
private:
	/**
	 * We are collecting the results in this
	 */
	ImageFrameResult frameResult;

	/**
	 * Holds the current configuration values
	 */
	MCParserConfig config;

	/** Used for parsing per-scanline data and tokenizing it as 1D markers */
	Hoparser<MT, CT> hp;

	/** We start with "before head" position */
	FFLPosition lastPos;
	/** We start with "before head" position */
	FFLPosition listPos;

	/** For book-keeping x: start in upper-left corner */
	unsigned int x = 0;
	/** For book-keeping y: start in upper-left corner */
	unsigned int y = 0;

	/**
	 * Always holds the currently suspected marker centers
	 * This list is sorted by the X coordinate from left-to-right
	 */
	FastForwardList<MarkerCenter, MAX_MARKER_PER_SCANLINE> mcCurrentList;

	/** True when we are right after a newline - false otherwise */
	bool afterNewLine = true;
};

#endif // _FT_MC_PARSER_H

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
