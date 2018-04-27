#ifndef FASTTRACK_HOPARSER_H
#define FASTTRACK_HOPARSER_H

#include <vector>
#include <cstdint>
#include <cmath>
#include "homer.h"

/** Holds configuration values for a Hoparser*/
class HoparserSetup {
	/**
	 * At least this many pixels of homogenous colour must be present before
	 * the start of the marker is suspected on a transition.
	 */
	int markStartPrefixWhiteLenMin = 30;

	/** 
	 * Maximum this many pixels can pass on the marker start 
	 * between the white->black transition. (depends resolution and camera?)
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
printf("===\n");
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
		// Save previous data.
		// Rem.: default homer values are good for kickstarting!
		sustate.wasInHo = homer.isHo();
		sustate.lastLen = homer.getLen();
		sustate.lastMagAvg = homer.magAvg();

		// Update data from the homogenity lexer
		homer.next(mag);

		// Check if the "homogenity" state has changed or not
		if(!homer.isHo() && sustate.wasInHo) {
			// Here when ended a "homogenity area"
printf("AVG: %d at LEN: %d @ i = %d\n", sustate.lastMagAvg, sustate.lastLen, sustate.x);
		}

		// increment scanline-pointer
		++sustate.x;

		// TODO: we need to find the marker :-) 
		return false;
	}
private:
	/** The undelying homer as lexer of homogenous areas */
	Homer<MT, CT> homer;

	/** Holds configuration values for a Hoparser */
	HoparserSetup setup;

	/**
	 * Holds currently suspected marker state
	 * Rem.: Used for simply resetting the state.
	 */
	struct SuspectionState final {
		/** Contains the current 'x' position in the scanline */
		int x = 0;

		/** -1 indicates no suspected marker */
		int markerStart = -1;

		/** -1 indicates no suspected marker center yet */
		int markerCenterStart = -1;

		/** -1 indicates no suspected marker center end yet */
		int markerCenterEnd = -1;

		/** -1 indicates no suspection for the marker end yet */
		int markerEnd = -1;

		/** number of "opening parentheses" */
		int openp = 0;

		/** number of "closing parentheses" */
		int closep = 0;

		/** Used for storing the one-time earlier state in the "next" operation. */
		bool wasInHo = false;

		/** Used for storing the one-time earlier state in the "next" operation. */
		int lastLen = 0;

		/** Used for storing the one-time earlier state in the "next" operation. */
		MT lastMagAvg = 0;
	};

	/** Holds data about current suspections*/
	SuspectionState sustate;
};

#endif // FASTTRACK_HOPARSER_H
// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
