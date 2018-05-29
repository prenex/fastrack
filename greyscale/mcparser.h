#ifndef FASTTRACK_MC_PARSER_H
#define FASTTRACK_MC_PARSER_H

#include <cstdint>
#include <cstdlib>
#include "hoparser.h"
#include "fastforwardlist.h"

#ifndef MAX_MARKER_PER_SCANLINE // Let the users define this
#define MAX_MARKER_PER_SCANLINE 1024 // Could be smaller I guess
#endif

// Only define to see debug logs
/*#define MC_DEBUG_LOG 1*/

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
struct Marker2D {
	unsigned int x;
	unsigned int y;
	unsigned int confidence;
	unsigned int order;
};

struct MCParserConfig {
	/**
	 * Ignore every markercenter that has smaller than this many signals
	 */
	unsigned int ignoreWhenSignalCountLessThan = 2;

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
	unsigned int widthDiffMax = 30;

	/** Marker is closed if there was no pixel for it in the last 50 rows */
	unsigned int closeDiffY = 20;
};

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
	};

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
	MarkerCenter() { }

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
		// and return the most common one later when closing this.
		ord[order - MIN_ORDER] = 1;
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
	 * Returns the X-coordinate that is the biggest currently acceptable for extending
	 * in the current state. Useful to know if an X position is too big for extending
	 * or small enough that it could have been extended already when going ordered from
	 * left-to-right "handle" token parsing.
	 * 
	 * Mostly used when stepping left-to-right and trying to insert or extend: it can be
	 * seen easily using this method if insertion BEFORE the current head is necessary
	 * or insertion/extension will be necessary only later (AFTER we already know that
	 * we cannot extend the current one, we can check for surely if we NEED to insert
	 * before the current in the ordered list)
	 */
	inline int getRightMostCurrentAcceptableX(unsigned int deltaDiffMax, unsigned int widthDiffMax) {
		// RightMost X calculated by the DelteDiffMax criteria
		int ddmx = lastX + deltaDiffMax;
		// Rightmost X calculated by teh widthDiffMax criteria
		// Rem.: This is minX+widthDiffMax because that is the last X
		//       coordinate that will work with the "if((newMaxX - newMinX) > widthDiffMax)"
		//       criteria as a non-skipped value.
		int wdmx = minX + widthDiffMax;
		// Use the smaller value - it tells the rightmost X-pos that would be 'extensible'
		return ((ddmx > wdmx) ? ddmx : wdmx);
	}

	/**
	 * Should be called when this marker center should be tried to be extended.
	 * Returns true when extended and false when skipped extension in which case
	 * skipUpd() was called internally.
	 */
	inline bool tryExtend(unsigned int x, unsigned int y, uint8_t order,
		   	unsigned int deltaDiffMax, unsigned int widthDiffMax) {
		// lastX access loads cache properly
		if(abs(lastX - x) > deltaDiffMax) {
			// Skipped
			skipUpd();
			return false;
		}

		// Check width constraint
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
		++ord[order - MIN_ORDER];

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

		// Indicate that we extended!
		return true;
	}

	/**
	 * Construct a marker using the centerline information we have gathered so far.
	 * Rem.: Best used when this center became "CLOSED" (but will return data anyways)
	 * Rem.: Returns an order of zero is signalCount was smaller than the configured threshold!
	 */
	inline Marker2D constructMarker(unsigned int ignoreWhenSignalCountLessThan) {
		// TODO: maybe do better than this avarage?
		unsigned int x = ((maxX - minX) / 2) + minX;
		unsigned int y = ((maxY - minY) / 2) + minY;

		unsigned int order = MIN_ORDER;
		if(signalCount >= ignoreWhenSignalCountLessThan) {
			uint8_t orderMaxCnt = ord[0];
			for(int i = 0; i < (1 + MAX_ORDER - MIN_ORDER); ++i) {
				if(ord[i] > orderMaxCnt) {
					orderMaxCnt = ord[i];
					order = i+MIN_ORDER; // 1 and 0 is unused to spare space
				}
			} // hope for loop unrolling here...
		} else {
			// Ignore these
			order = 0;
		}

		return Marker2D{
			x, y,
			static_cast<unsigned int>(confidence),
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
	uint8_t ord[1 + MAX_ORDER - MIN_ORDER];
};

/**
 * Result of parsing marker centers in an image frame
 */
struct ImageFrameResult{
	/** Build using the found and properly closed MarkerCenters */
	std::vector<Marker2D> markers;
};

/**
 * A whole-image parser as described below.
 * Basically finds every marker1 marker on an image!
 * Parses "vertical MarkerCenters" by using an 
 * underlying TOKENIZER (default: Hoparser) results.
 *
 * Use next(<pixel>) call for providing magnitude data
 * Use endLine() call to indicate the end of line
 * Use endImageFrame() to get results and reset the parser
 *     for the next possible frame that might come later.
 * Rem.: Template parameters are those of the TOKENIZER (def.: Hoparser)
 *       and of course the tokenizer itself so this template can be
 *       used with more than one kind of tokenizer that provides
 *       per-scanline 1D marker positions. This enables experimenting!
 */
template<typename MT = uint8_t, typename CT = int, typename TOKENIZER = Hoparser<MT, CT>>
class MCParser {
public:

	/** Used for parsing per-scanline data and tokenizing it as 1D markers */
	TOKENIZER tokenizer;

	/** Create a markercenter-parser with default configuration */
	MCParser() noexcept {
	}

	/** Create a markercenter-parser with the given configuration */
	MCParser(MCParserConfig parserConfig) noexcept {
		config = parserConfig;
	}

	/** Create a markercenter-parser with the given configurations - works only for Hoparser usage */
	MCParser(MCParserConfig parserConfig, HoparserSetup hoparserSetup, HomerSetup homerSetup) noexcept {
		config = parserConfig;
		// TODO: ensure this works when other template parameters are provided
		tokenizer = Hoparser<MT, CT>(homerSetup, hoparserSetup);
	}

	/** FEED OF THE NEXT MAGNITUDE: Returns the same data as HoParser - mostly debug-only return value! */
	inline NexRes next(MT mag) noexcept {
		// Use the tokenizer to only process "tokens" and not every pixel
		// These tokens are already the 1D marker centers that the system
		// suspects with the per scanline algorithm.
		auto ret = tokenizer.next(mag);

		// See if the hoparser finds an 1D marker at the pixel in this scanline
		// We only run all the following code for that rare case (see green dots
		// in the marker1_eval application when it finds these for the scanlines)
		if(LIKELY(!ret.foundMarker)) {
			++x;
			return ret;
		} else {
			// get marker data
			int centerX = tokenizer.getMarkerX();
			auto order = tokenizer.getOrder();
			if(config.ignoreOrderSmallerThan < order) {
				// If not too small to ignore, process it!

				// PRE-READ TECHNIQUE
				// ==================
				//
				// Advance list position when we are the first test on a newline
				// And the list is not empty. The second handles cases when the
				// list is completely empty
				if(afterNewLine) {
					// If the list is empty stays: lastPos == listPos == NIL_POS
					// If it already has data, we step on the valid data and not be NIL
					// Rem.: The above if ensures that we are on the beginning NIL_POS
					if(!(mcCurrentList.isEmpty())) {
						lastPos = listPos;
						listPos = mcCurrentList.next(listPos);
					}
					/*
					else  NIL_POS and NIL_POS for both
					*/

					// Indicate that we have handled the flag
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
				// its FIRST "lastX") and now in this place we are processing them to make a
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
				// At the first moment we had to move the "first" list to a valid pos
				// if that is possible (otherwise just keep listPos, lastPos at NIL_POS
				// if that was possible as next(..) call indicates there is a valid
				// suspected pixel in this scanline! (see marker1_eval test app for
				// these green pixels when you are clicking in that application).
				// 
				// Rem.: This is basically a two-variabled-one-valued elementwise
				//       processing algorithm for unification updates of the list.
				//       The difference is that this is an "online" working version.
				//       (*): In Hungarian, this special case is an "Időszerűsítés".

				// Add this new token we have found as a new marker centerline or
				// extend an already existing marker centerline! Loop until processed.
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
								std::move(MarkerCenter(centerX, y, order)),
							   	lastPos);
						tokenProcessed = true;
#ifdef MC_DEBUG_LOG
printf("+(%d,%d) ", centerX, y);
#endif // MC_DEBUG_LOG
					} else {
						// Compare if we can merge the next()-ed element into the list
						// position element... For this we better get a reference to it
						MarkerCenter &currentCenter = mcCurrentList[listPos];

						bool extendedIt = false;
						if(!currentCenter.shouldClose(y, config.closeDiffY)) {
							// Try extending the existing element
							// This is a NO-OP when we cannot extend it
							extendedIt = currentCenter.tryExtend(
									centerX, y, order, 
									config.deltaDiffMax, config.widthDiffMax);
						}

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
#ifdef MC_DEBUG_LOG
printf("E(%d,%d) ", centerX, y);
#endif // MC_DEBUG_LOG
						} else {
							// If we did not succeed, we need to see if the element
							// is so much before the one in the earlier list that
							// it should be added as a new element at the current
							// lastPos insertion position or not:
							if(currentCenter.getRightMostCurrentAcceptableX(config.deltaDiffMax, config.widthDiffMax)
								   	> centerX) {
								// Completely new suspected marker - in the middle of the list
								// Rem.: We know we need to insert this here and there will be no list position
								//       to extend, because the list is ordered by the 'x' coordinate and next()
								//       is called also in an ordered way. Because of insertion, the list also
								//       kept ordered now so later iterations and calls to next() work as well!
								mcCurrentList.insertAfter(
										std::move(MarkerCenter(centerX, y, order)),
										lastPos); // Rem.: lastPos insertion is needed as we insert BEFORE listPos
								// Increment is needed to keep invariant that lastPost is literally the position 
								// "before" the listpos. Because of the above insertion it would be not true anymore!
								lastPos = mcCurrentList.next(lastPos);
								// Mark this token as processed
								// Rem.: We should not move with the list iteraor as the next time of the next(..)
								//       call might return extension/continuation of what is under the head now!
								tokenProcessed = true;
#ifdef MC_DEBUG_LOG
printf("N(%d,%d) ", centerX, y);
#endif // MC_DEBUG_LOG
							} else { // Rem.: This else is necessary or we would need to step with the lastPos too!
								// See if things indicate we need to close the earlier found stuff
								if(currentCenter.shouldClose(y, config.closeDiffY)) {
									// Add the generated marker from it to the frame results
									// Rem.: This adds poor quality markers too, but with small confidence
									auto marker2d = currentCenter.constructMarker(config.ignoreWhenSignalCountLessThan);
									if(marker2d.order > 0) {
										// negative order means that the signal count was too small for the threshold!
										frameResult.markers.push_back(marker2d);
									}

									// Close / Unlink the added one as it is considered to be closed!
									// Rem.: We need to update list position to a valid position!
									// Rem.: lastPos keeps to be valid too
									listPos = mcCurrentList.unlinkAfter(lastPos);
#ifdef MC_DEBUG_LOG
printf("C(%d,%d) ", centerX, y);
#endif // MC_DEBUG_LOG
								} else {
									// If there was nothing to close, we just update our "iterators"
									lastPos = listPos;
									listPos = mcCurrentList.next(listPos);
#ifdef MC_DEBUG_LOG
printf("*(%d,%d) ", centerX, y);
#endif // MC_DEBUG_LOG
								}
							}
						}
					}
				}
			}

			++x;
			return ret;
		}
	}

	/**
	 * Indicates that the line has ended and "next" pixels are on a following line.
	 * Rem.: Lines should be normally of the same size otherwise the algorithm can fail!
	 *       This is not a strict requirement, but no resizing will happen along the changes!
	 */
	inline void endLine() noexcept {
		// Reset read head in the ordered list
		listPos = NIL_POS;
		lastPos = NIL_POS;
		// Reset x book-keeping
		x = 0;
		// Increment y book-keeping
		++y;
		// Indicate newline
		afterNewLine = true;
		// Necessary for hoparser knowing its state should be reseted for new line
		tokenizer.newLine();
	}

	/**
	 * Ends the current image frame and returns all found 2D marker locations on the image.
	 * Rem.: The returned reference is only valid until the next() function is called once again.
	 */
	inline const ImageFrameResult endImageFrame() noexcept {
		// Add Marker2Ds from any still unclosed MarkerCenters
		// This is necessary as things are only closed because of
		// a Garbage collecting-like operation in the fastforwardlist
		// of ours for cases where many markers are on the image frame!
		auto readHead = mcCurrentList.head();
		while(!readHead.isNil()) {
			auto currentCenter = mcCurrentList[readHead];
			if(currentCenter.shouldClose(y, config.closeDiffY)) {
				// Add the generated marker from it to the frame results
				// Rem.: This adds poor quality markers too, but with small confidence
				auto marker2d = currentCenter.constructMarker(config.ignoreWhenSignalCountLessThan);
				if(marker2d.order > 0) {
					// negative order means that the signal count was too small for the threshold!
					frameResult.markers.push_back(marker2d);
				}
			}
			readHead = mcCurrentList.next(readHead);
		}

		// Reset state
		listPos = NIL_POS;
		lastPos = NIL_POS;
		// Reset x book-keeping
		x = 0;
		// Reset y book-keeping
		y = 0;
		// Indicate newline
		afterNewLine = true;

		// Reset vertical marker-centerline collector list!
		// Needed when it is not empty on the last scanline
		// of the earlier frame!
		mcCurrentList.reset();

		// Reset result and return any earlier collected result
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

	/** We start with "before head" position (NIL_POS) */
	FFLPosition lastPos;
	/** We start with "before head" position (NIL_POS) */
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
