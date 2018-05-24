#ifndef FASTTRACK_HOPARSER_H
#define FASTTRACK_HOPARSER_H

// Uncomment to see debug logging
//#define DEBUGLOG 1

#include <vector>
#include <cstdint>
#include <cmath>
#include "homer.h"

/** Result of a Hoparser::next() operation */
struct NexRes {
	/** A marker has been found - see details for marker position */
	bool foundMarker;
	/** A token has found at the current position */
	bool isToken;
};

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
	int markContinueTooBigWidthDelta = 20;

	/**
	 * Maximum this many pixels can be between the start of the current stripe and the last
	 * While we are considering the stripes of the marker. Bigger difference means we revert
	 * to search a new marker start and consider this as a false positive!
	 */
	int markContinueStripeSizeMaxDelta = 40;

	/**
	 * We must ignore all "hotoken" that is smaller than this delta length.
	 * Because the "Homer" class might return very small homogenity areas
	 * in some edge-cases, this extra ignore step helps to sort-out mistakes
	 * in the earlier layer! This is what enables the "*Continue*" parameters
	 * to really work: not only they work in the non-homogenous cases, but
	 * also when some error/mistake shows homogenous and is not that in reality!
	 */
	int ignoreSmallHotokenDeltaLen = 10;
};

/** 
 * A scanline-parser as described below.
 * This class acts as if we do a "parsing" by considering the result of "homer" as lexer data.
 * The result of the parse are the suspected marker center positions in the scanline!
 * Rem.: Template parameters are those of Homer!
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
		// This is NO-OP unless we are profiling!
		homer.flushBranchProfileData();
		// Reset our state to start from scratch
		sustate = SuspectionState();
	}

	/** Number of found stripes */
	inline int getOrder() {
		return sustate.openp;
	}

	/** Only returns valid value when a marker is already found */
	inline int getMarkerX() {
		// The best approximation is the avarage of the centerEnd and centerStart positions!
		return (sustate.markerCenterEnd - sustate.markerCenterStart) / 2 + sustate.markerCenterStart;
	}

	/**
	 * Should be called for every pixel in the scanline - with the magnitude value.
	 * Returns true when a marker has been found!
	 */
	inline NexRes next(MT mag) noexcept {
		NexRes ret;

		// Update previous and pre-previous homogenity datas first.
		sustate.updateLast(homer);
		// Some calculations are deferred here because they had division!
		sustate.saveDataForUpdateLastMagAvg(homer); // See: (*)

		// Update data in the homogenity lexer
		homer.next(mag);

		// Check if the "homogenity" state has changed or not
		// And then check if the homogenity area is too small or not
		if(!homer.isHo() && sustate.wasInHo
			   	&& homer.getLen() < setup.ignoreSmallHotokenDeltaLen) {
			sustate.updateLastMagAvg(homer); // (*)
			// Here when ended a "homogenity area"
			// This is like a lexical token in compilers
			// so here we need to process this "homogenity token"
			ret.foundMarker = processHotoken(homer);
			ret.isToken = true;

			// Update the last-before datas (lastLast*)
			sustate.updateLastBefore();
		} else {
			// We are surely not found the marker when we are
			// still in the middle of a homogenity area (or inhomogen)
			ret.foundMarker = false;
			ret.isToken = false;
		}

		// Increment scanline-pointer
		++sustate.x;

		return ret;
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
		printf("Token: AVG= %d at LEN= %d @ %d..%d --- ", sustate.lastMagAvg, sustate.lastLen, sustate.x - sustate.lastLen, sustate.x);
#endif //DEBUGLOG

		// Update ending of last two homogenous areas
		// This works because processHotoken is called only on the end of the areas
		// and this method first moves the last* into the lastLast* and then save.
		sustate.updateLastAndLastBeforeEndX();

		// PRE_MARKER
		if(sustate.sState == PRE_MARKER) {
			// CHECK markStartSuspectionMagDeltaMin
			if(
					((sustate.lastLastMagAvg - sustate.lastMagAvg) <= 0) || 
					(abs(sustate.lastLastMagAvg - sustate.lastMagAvg) < setup.markStartSuspectionMagDeltaMin)) {
#ifdef DEBUGLOG
				printf("NOT_MARKER_START: markStartSuspectionMagDeltaMin abs(%d - %d)<%d ",
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
						printf(" '(' - SUSPECT_MARKER_START! ");
#endif //DEBUGLOG
						// Save markerStart!
						sustate.markerStart = lastStartX;

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
			// Analyse if we see a paranthesis at all
			bool isParenthesis = true;
			// CHECK: markContinueStripeSizeMaxDelta
			// - This means that the length must be basically the same
			// BEWARE: The center has twice the length so we need to check also for
			//         similarity with twice the lengths and take the smaller delta!
			int delta = abs(sustate.lastLen - sustate.lastLastLen);
			int delta_cen = abs(sustate.lastLen - sustate.lastLastLen * 2);
			delta = (delta < delta_cen) ? delta : delta_cen;
			if(delta > setup.markContinueStripeSizeMaxDelta) {
				// PROBLEM: indicate no parenthesis
				isParenthesis = false;
#ifdef DEBUGLOG
				printf("NOT_PARENTHESES: bad stripe diff delta (markContinueStripeSizeMaxDelta) ");
#endif //DEBUGLOG
			}

			// CHECK: markContinueTooBigWidthDelta
			// - Cannot be too much distance between homogen areas
			int lastStartX = sustate.lastEndX - sustate.lastLen;
			//Rem.: abs is not needed here: int stripeLen = abs(sustate.lastLastEndX - lastStartX);
			int stripeLen = (sustate.lastLastEndX - lastStartX);
			if(stripeLen > setup.markContinueTooBigWidthDelta) {
				// PROBLEM: indicate no parenthesis
				isParenthesis = false;
#ifdef DEBUGLOG
				printf("NOT_PARENTHESES: bad stripe len (setup.markContinueTooBigWidthDelta) ");
#endif //DEBUGLOG
			}

			if(!isParenthesis) {
				// We have found this to be not a proper parenthesis that we need
				// Because of this, we need to revert our state machine to search
				// for the start of an other marker as this was false positive.
				sustate.resetToPreMarker();
#ifdef DEBUGLOG
				printf(" -> PRE_MARKER (not parenthesis) ");
#endif //DEBUGLOG
			} else {
				// We have found a parentheses
				// Analyse if we see an opening or closing parentheses
				bool openParentheses = (sustate.lastMagAvg > sustate.lastLastMagAvg); // false == closing one

				if(openParentheses) {
					// OPEN
#ifdef DEBUGLOG
				printf(" '(' ");
#endif //DEBUGLOG
					// Increment opening parenthesis count for marker acceptance later
					++sustate.openp;
				} else {
					// CLOSE - this is the CENTER!!! (as suspected)

					bool isRealCenterSuspected = true;
					/*
					// CHECK: This must be nearly two times as big as the ealier one(s)!
					if((sustate.lastLen < sustate.lastLastLen) ||
						   	(abs(sustate.lastLastLen - (sustate.lastLen - sustate.lastLastLen))
							  < (setup.markContinueStripeSizeMaxDelta))) {
						isRealCenterSuspected = false;
#ifdef DEBUGLOG
						printf(" NOT REAL CENTER (markContinueStripeSizeMaxDelta) ");
#endif //DEBUGLOG
					}
					*/
					// TODO: CHECK: This must be the same amount change like at the marker start suspection!


					if(isRealCenterSuspected) {
						// REAL MARKER CENTER SUSPECTED!!!
						// Save begin-end x positions for this center!
						sustate.markerCenterStart = lastStartX;

#ifdef DEBUGLOG
						printf(" '*' ");
						printf(" -> POS_CENTER_START ");
#endif //DEBUGLOG
						sustate.sState = POS_CENTER_START;
						// This is needed because we do not increment openp when
						// we meet the very first opening parenthesis but we do
						// count the very last in the other direction!!!
						++sustate.openp;
					} else {
						// Not a real center
						sustate.resetToPreMarker();
#ifdef DEBUGLOG
						printf(" -> PRE_MARKER(notrealcenter) ");
#endif //DEBUGLOG
					}
				}
			}

#ifdef DEBUGLOG
			printf("NOT_MARKER: Were in PRE_CENTER!\n");
#endif //DEBUGLOG
			// We will never tell we have found a marker here as
			// we are in the very beginning of searching for it
			// even in the happy cases...
			return false;
		// BEWARE HERE FOR OPTIMIZE MISSING COMPARISON!!! (!!!!)
		} else if((sustate.sState >= POS_CENTER_START) && (sustate.sState <= POS_CENTER_FINISHING)) {
			// After the center when we are here!
			// Check common constrains first
			// Analyse if we see a paranthesis at all
			bool isParenthesis = true;
			// CHECK: markContinueStripeSizeMaxDelta
			// - This means that the length must be basically the same
			// BEWARE: The center has twice the length so we need to check also for
			//         similarity with twice the lengths (of last ones) and take the smaller delta!
			int delta = abs(sustate.lastLen - sustate.lastLastLen);
			int delta_cen = abs(sustate.lastLen - sustate.lastLastLen / 2); // nodiv: just a right shift here!
			delta = (delta < delta_cen) ? delta : delta_cen;
			if(delta > setup.markContinueStripeSizeMaxDelta) {
				// PROBLEM: indicate no parenthesis
				isParenthesis = false;
#ifdef DEBUGLOG
				printf("NOT_PARENTHESES: bad stripe diff delta (markContinueStripeSizeMaxDelta) ");
#endif //DEBUGLOG
			}

			// CHECK: markContinueTooBigWidthDelta
			// - Cannot be too much distance between homogen areas
			int lastStartX = sustate.lastEndX - sustate.lastLen;
			//Rem.: abs is not needed here: int stripeLen = abs(sustate.lastLastEndX - lastStartX);
			int stripeLen = (sustate.lastLastEndX - lastStartX);
			if(stripeLen > setup.markContinueTooBigWidthDelta) {
				// PROBLEM: indicate no parenthesis
				isParenthesis = false;
#ifdef DEBUGLOG
				printf("NOT_PARENTHESES: bad stripe len (setup.markContinueTooBigWidthDelta) ");
#endif //DEBUGLOG
			}

			if(!isParenthesis) {
				// We have found this to be not a proper parenthesis that we need
				// Because of this, we need to revert our state machine to search
				// for the start of an other marker as this was false positive.
				sustate.resetToPreMarker();
#ifdef DEBUGLOG
				printf(" -> PRE_MARKER (not parenthesis) ");
#endif //DEBUGLOG
			} else {
				// We have found a parentheses
				// Analyse if we see an opening or closing parentheses
				bool openParentheses = (sustate.lastMagAvg > sustate.lastLastMagAvg); // false == closing one
				// Right in first case after center start we find the open parenthesis in the opposite magnitude 
				// change direction because the center->nocenter transition is not decreasing in magnitude!
				if(sustate.sState == POS_CENTER_START) {
					openParentheses = !openParentheses;
				}

				if(openParentheses) {
					// OPEN
					// This is not good for as because we expect closing parenthesis now!
					sustate.resetToPreMarker();
#ifdef DEBUGLOG
					printf(" '(' ");
					printf(" -> PRE_MARKER (bad parenthesing) ");
#endif //DEBUGLOG
				} else {
					// CLOSING
					// We count these ones
#ifdef DEBUGLOG
					printf(" ')' ");
#endif //DEBUGLOG
					// Increment closing parenthesis count for comparing with the openp values
					++sustate.closep;

					// Change state when necessary (after first closing parenthesis)
					if(sustate.sState == POS_CENTER_START) {
						sustate.sState = POS_CENTER_FINISHING;
						// Save begin-end x positions for this center!
						sustate.markerCenterEnd = lastStartX;
					}

					// Check if we have found a finish of the marker
					// Rem.: We might get here even if there were only one closing parentheses!
					if(sustate.openp == sustate.closep) {
#ifdef DEBUGLOG
						printf(" REAL MARKER COMPLETED! \n");
#endif //DEBUGLOG
						// Save marker end position!
						sustate.markerEnd = lastStartX;
						// Indicate that we have found a marker
						// Rem.: we cannot clear the state as the user of us need to fetch the data!!!
						return true;
					} else {
						// We still wait until enough parethesis arrives!
#ifdef DEBUGLOG
						//printf(" (%d != %d)! \n", sustate.openp, sustate.closep);
#endif //DEBUGLOG
						return false;
					}
				}
			}
		}
		
		return false;
	}

	/** Defines the overall suspection state */
	enum SState {
		/** We suspect that we are before marker in this scanline */
		PRE_MARKER = 0,
		/** We suspect that we are in a marker in this scanline - before its center */
		PRE_CENTER = 1,
		/** We suspect that we are in a marker in this scanline - right after its center */
		POS_CENTER_START = 2,
		/** We suspect that we are in a marker in this scanline - somewhere after its center */
		POS_CENTER_FINISHING = 3,
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

		/** Updates wasInHo and lastLen */
		inline void updateLast(Homer<MT, CT> &homer) {
			// Update new state
			// Rem.: default homer values are good for kickstarting the first hotoken
			wasInHo = homer.isHo();
			lastLen = homer.getLen();
		}

		// Rem.: This is only here because of optimizing out the division from the inner loop
		//       that runs for every pixel of the image. This way no div will be necessary!
		//       This only saves out simple values as you can see!
		/** Saves data for the updateLastMagAvg(..) call without doing a slow division op */
		inline void saveDataForUpdateLastMagAvg(Homer<MT, CT> &homer) {
			__hackz_saved_homarea_len = homer.getLen();
			__hackz_saved_homarea_magSum = homer.getMagSum();
		}
		int __hackz_saved_homarea_len = 0;
		CT __hackz_saved_homarea_magSum = 0;

		/** Updates lastMagAvg */
		inline void updateLastMagAvg(Homer<MT, CT> &homer) {
			lastMagAvg = (MT) (__hackz_saved_homarea_magSum / __hackz_saved_homarea_len);
			//lastMagAvg = homer.magAvg();
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

		/** Reset to the searching a new marker: reset parenthesing data and state machine */
		inline void resetToPreMarker() {
			// State machine reset
			sState = PRE_MARKER;

			// reset markerStart, markerEnd and markerCenterStart, markerCenterEnd
			markerStart = -1;
			markerCenterStart = -1;
			markerCenterEnd = -1;
			markerEnd = -1;

			// parenthesis collectors reset
			openp = 0;
			closep = 0;
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
