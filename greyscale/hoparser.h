#ifndef FASTTRACK_HOPARSER_H
#define FASTTRACK_HOPARSER_H

// Uncomment to see debug logging
#define DEBUGLOG 1

#include <vector>
#include <cstdint>
#include <cmath>
#include "homer.h"

/** Holds configuration values for a Hoparser*/
struct HoparserSetup {
	/**
	 * At least this many pixels of homogenous colour must be present before
	 * the start of the marker is suspected on a transition.
	 */
	int markStartPrefixHomoLenMin = 30;

	/** 
	 * Maximum this many pixels can pass on the marker start 
	 * between the white->black transition. (depends resolution and camera?)
	 * Rem.: Compare with differece between end of the lastLast and start of last homArea
	 */
	int markStartTransitionLenMax = 20;

	/**
	 * Minimum this big (negative) change is needed in the magnitude to suspect marker start
	 */
	int markStartSuspectionMagDeltaMin = 50;

	/**
	 * Maximum this many pixels we can feel between the ends and starts of the homogen areas
	 * when we are suspected that we are in the marker. When a markStart is suspected, but a
	 * bigger than this difference is found in the Homer areas start/end, we stop suspecting
	 * that we have a marker start and count the situation as a false positive.
	 */
	int markContinueTooBigWidthDelta = 40;
};

/** 
 * This class acts as if we do a "parsing" by considering the result of "homer" as lexer data.
 * The result of the parse are the suspected marker center positions in the scanline!
 */
template<typename MT = uint8_t, typename CT = int>
class Hoparser {
public:
	
	Hoparser() {
		// NO-OP: Just the default values for now
	}

	/** Create a Hoparser using the default configuration and the given Homer setup values */
	Hoparser(HomerSetup hs) {
		homer = Homer<MT, CT>(hs);
	}

	/** Create a Hoparser using the given configuration and the given Homer setup values */
	Hoparser(HomerSetup hs, HoparserSetup hps) {
		homer = Homer<MT, CT>(hs);
		setup = hps;
	}

	/** Should be called to indicate that a new scan line has started - basically a reset */
	inline void newLine() {
#ifdef DEBUGLOG
		printf("===\n");
#endif //DEBUGLOG
		// Reset the homogenity lexer
		homer.reset();
		// Reset our state to start from scratch
		sustate = SuspectionState();
	}

	/**
	 * Should be called for every pixel in the scanline - with the magnitude value.
	 * Returns true when a marker has been found!
	 */
	inline bool next(MT mag) noexcept {
		bool ret;

		// Update previous and pre-previous homogenity datas first.
		sustate.updateLast(homer);

		// Update data in the homogenity lexer
		homer.next(mag);

		// Check if the "homogenity" state has changed or not
		if(!homer.isHo() && sustate.wasInHo) {
			// Here when ended a "homogenity area"
			// This is like a lexical token in compilers
			// so here we need to process this "homogenity token"
			ret = processHotoken(homer);

			// Update the last-before datas (lastLast*)
			sustate.updateLastBefore();
		} else {
			// We are surely not found the marker when we are
			// still in the middle of a homogenity area (or inhomogen)
			ret = false;
		}

		// Increment scanline-pointer
		++sustate.x;

		// TODO: we need to find the marker :-) 
		return false;
	}
private:
	/**
	 * Process a homogenity token right after the homogenity area state changed.
	 * Returns true when marker has been found and marker data can be asked for!
	 * BEWARE: Changes/updates this->sustate!!!
	 */
	inline bool processHotoken(Homer<MT, CT> &homer) {
#ifdef DEBUGLOG
// Rem.: \n is always at the "return" operation!
		printf("Token: AVG= %d at LEN= %d @ i= %d --- ", sustate.lastMagAvg, sustate.lastLen, sustate.x);
#endif //DEBUGLOG

		// Update ending of last two homogenous areas
		// This works because processHotoken is called only on the end of the areas
		// and this method first moves the last* into the lastLast* and then save.
		sustate.updateLastAndLastBeforeEndX();

		// PRE_MARKER
		if(sustate.sState == PRE_MARKER) {
			// CHECK markStartSuspectionMagDeltaMin
			if(
					((sustate.lastLastMagAvg - sustate.lastMagAvg) > 0) && 
					(abs(sustate.lastLastMagAvg - sustate.lastMagAvg) < setup.markStartSuspectionMagDeltaMin)) {
#ifdef DEBUGLOG
				printf("NOT_MARKER_START: markStartSuspectionMagDeltaMin avg(%d - %d)<%d ",
					   	sustate.lastLastMagAvg,
						sustate.lastMagAvg,
						setup.markStartSuspectionMagDeltaMin
				);
#endif //DEBUGLOG
			} else {
				// If we are here, we suspect that this might be a start of a marker
				if(sustate.lastLastLen < setup.markStartPrefixHomoLenMin) {
#ifdef DEBUGLOG
					printf("NOT_MARKER_START: markStartPrefixHomoLenMincheck! ");
#endif //DEBUGLOG
				} else {
					// If we are here, we still suspect that we might change our generic state!

					int lastStartX = sustate.lastEndX - sustate.lastLen;
					//Rem.: abs is not needed here: int transitionLen = abs(sustate.lastLastEndX - lastStartX);
					int transitionLen = (sustate.lastLastEndX - lastStartX);
					if(transitionLen > setup.markStartTransitionLenMax) {
#ifdef DEBUGLOG
						printf("NOT_MARKER_START: markStartTransitionLenMax! ");
#endif //DEBUGLOG
					} else {
						// If we are here, we can suspect that a marker starts here!
						// Still not sure, but it is a good suspicion according to our best knowledge
#ifdef DEBUGLOG
						printf("SUSPECT_MARKER_START! ");
#endif //DEBUGLOG
						// We will wait for the center to come
						sustate.sState = PRE_CENTER;
					}
				}
			}
#ifdef DEBUGLOG
			printf("NOT_MARKER: In PRE_MARKER state!\n");
#endif //DEBUGLOG
			// We will never tell we have found a marker here as
			// we are in the very beginning of searching for it
			// even in the happy cases...
			return false;
		} else if(sustate.sState == PRE_CENTER) {
			// TODO: Analyse if we see a paranthese at all

			// TODO: Analyse if we see an opening or closing parentheses
			bool openParentheses = true; // false == closing one

			if(openParentheses) {
				// OPEN
				
				// TODO: CHECK: This must be nearly the same big as the earlier ones before CENTER.
				// TODO: Increment opening parenthesis count for marker acceptance later
			} else {
				// CLOSE - this is the CENTER!!! (as suspected)

				// TODO CHECK: This must be nearly two times as big as the ealier ones!
				// TODO: CHECK: This must be the same amount change like at the marker start suspection!
				// TODO: Save begin-end x positions for this center!
			}

#ifdef DEBUGLOG
			printf("NOT_MARKER: In PRE_CENTER state!\n");
#endif //DEBUGLOG
			// We will never tell we have found a marker here as
			// we are in the very beginning of searching for it
			// even in the happy cases...
			return "false";
		} else if(sustate.sState == POS_CENTER) {
			// TODO: Count closing parentheses and accept marker if it has been found 

			// Reset our state to look for a next marker in this very same scanline
			sustate.sState == PRE_MARKER;
		}
		
#ifdef DEBUGLOG
		printf("TODO!\n");
#endif //DEBUGLOG
		return false;
	}

	/** Defines the overall suspection state */
	enum SState {
		/** We suspect that we are before marker in this scanline */
		PRE_MARKER = 0,
		/** We suspect that we are in a marker in this scanline - before its center */
		PRE_CENTER = 1,
		/** We suspect that we are in a marker in this scanline - after its center */
		POS_CENTER = 2,
	};

	/**
	 * Holds currently suspected marker state
	 * Rem.: Used for simply resetting the state.
	 */
	struct SuspectionState final {
	// GENERIC DATA
		SState sState = PRE_MARKER;
		/** Contains the current 'x' position in the scanline */
		int x = 0;

	// MARKER SUSPECTION DATA
		/** -1 indicates no suspected marker */
		int markerStart = -1;

		/** -1 indicates no suspected marker center yet */
		int markerCenterStart = -1;

		/** -1 indicates no suspected marker center end yet */
		int markerCenterEnd = -1;

		/** -1 indicates no suspection for the marker end yet */
		int markerEnd = -1;

	// PROPER PARENTHESES CHECK STATE
		/** number of "opening parentheses" */
		int openp = 0;

		/** number of "closing parentheses" */
		int closep = 0;

	// LAST and LAST-BEFORE homogenity state end-X positions
		/** Contains 'x' at the moment when the last */
		int lastEndX = 0;

		/** Contains 'x' at the moment when the last */
		int lastLastEndX = 0;

	// LAST and LAST-BEFORE homogenity states data
		// LAST
		/** Used for storing the one-time earlier state in the "next" operation. */
		bool wasInHo = false;

		/** Used for storing the one-time earlier state in the "next" operation. */
		int lastLen = 0;

		/** Used for storing the one-time earlier state in the "next" operation. */
		MT lastMagAvg = 0;

		// LAST-BEFORE
		/** Used for storing the two-times earlier state in the "next" operation. */
		bool wasWasIsHo = false;

		/** Used for storing the two-times earlier state in the "next" operation. */
		int lastLastLen = 0;

		/** Used for storing the two-times earlier state in the "next" operation. */
		MT lastLastMagAvg = 0;

		/** Updates wasInHo, lastLen, lastMagAvg and wasWasIsHo, lastLastLen, lastLastMagAvg */
		inline void updateLast(Homer<MT, CT> &homer) {
			// Update new state
			// Rem.: default homer values are good for kickstarting the first hotoken
			wasInHo = homer.isHo();
			lastLen = homer.getLen();
			lastMagAvg = homer.magAvg();
		}

		inline void updateLastBefore() {
			// Move the earlier last homogenity states into the last-before-last
			// Rem.: default values ensure there are no-op at start-hotoken until the second!
			wasWasIsHo = wasInHo;
			lastLastLen = lastLen;
			lastLastMagAvg = lastMagAvg;
		}

		/** Update the lastLastEndX and the lastEndX fields */
		inline void updateLastAndLastBeforeEndX() {
			lastLastEndX = lastEndX;
			lastEndX = x;
		}
	};


	/** The undelying homer as lexer of homogenous areas */
	Homer<MT, CT> homer;

	/** Holds configuration values for a Hoparser */
	HoparserSetup setup;

	/** Holds data about current suspections*/
	SuspectionState sustate;
};

#endif // FASTTRACK_HOPARSER_H
// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
