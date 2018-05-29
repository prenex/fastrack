#ifndef FASTTRACK_HOMER_H
#define FASTTRACK_HOMER_H

// Enable this (from client code) if you want to profile which branches of the next(..) call got called how many times
//#define HOMER_MEASURE_NEXT_BRANCHES 1
// Enable this (from client code) if you want more "imul" operations per pixel in exchange for better precision :-)
//#define SLOW_PRECISE_HOMER 1 

#include <vector>
#include <cstdint>
#include <cmath>
#include <limits> // for templated min-max integer values

#include "microshackz.h"

/* Use this for exponential: affection #define EXPONENTIAL_ATTRITION */

/** For parametrization of the lenAffect(...) methods */
struct LenAffectParams final {
	/** Until this length we keep values unaffected */
	int fullAffectLenUpCons = 50;
	/** This defines the end length of consideration for calculating */
   	int leastAffectLenBottCons = 300;
	/** There will be (2^stepPointExponential) steps in the interpolation */
	unsigned int stepPointExponential = 2;
	/** An "attrition" value: when 0, we change simply linearly, when bigger value we change less steep! */
	unsigned int attrExp = 2;
};

/**
 * Devalvate "value" using the length
 * Useful for: devalving check constraints so that they are relative to length 
 * and not absolute! Many constrainst are best when relative.
 *
 * BEWARE: Only 0, 1, 2, 4, 8, ... "steps" are supported because of performance, so stepPointExponential
 *         values 0, 1, 2, 3, 4 means the above mentioned powers of two when considering step counts!
 * BEWARE: The fullAffectLenUpCons must be smaller than leastAffectLenBottCons. Just give both
 *         of these parameters something big in order to default to not affect the value at all!
 *
 * Remarks:
 * - Any length smaller than fullAffectLenUpCons will have the full "value"
 * - Any length longer than leastAffectLenBottCons will have the value halved "step"-times...
 * - Any length in-between is (approximately) interpolateda with "2^(stepPointExponential)" number of halving-steps!
 * Remarks: Best is to use only a small number of "stepPointExponential" and the possibly longerst fullAffectLenUpCons
 *          to achieve the best speed and branch prediction operations here.
 */
template<typename T>
inline T lenAffect(T value, int len, LenAffectParams params) {
	T ret = value;
#ifdef NO_ATTRITION
	return ret;
#else

#ifdef SIMPLE_ATTRITION
	// Usually the fullAffectLenUpCons is not a really-really big value!
	if(UNLIKELY(len < params.fullAffectLenUpCons)) return ret;
	else return (ret<<1);
#else
	// Reasons for the fast-path:
	// 1.) Not reached minimal delta length for starting the stepping procedure
	// 2.) We are configured to not do any step at all (zero stepping)
	// 3.) The special case when the length is zero at the start of suspecting areas
	// 4.) In case the original value is zero (which would stay zero - just faster without calc.)
	if(LIKELY((len < params.fullAffectLenUpCons) || (params.stepPointExponential == 0) || (len == 0) || (ret == 0))) {
		// fast-path - mostly we are coming here with a right setup!!!
		return ret;
	} else {
		// slower path (still just fast halving-params.stepPointExponential and simple calculations)
		int curStepLen = params.leastAffectLenBottCons - params.fullAffectLenUpCons;
		// Rem.: Here substraction is needed as the stepping should start from full
		//       magnitude at the least affected length bottom constraint!
		int curLen = len - params.fullAffectLenUpCons;
		unsigned int realSteps = (1 << (params.stepPointExponential - 1)); // always bit shift
		// The loop variable realSteps runs like [...16, 8, 4, 2, 1]
		// But the loop runs always "params.stepPointExponential" times and not more!
		do {
			// Step the loop variable used both as exit condition and division rate
			realSteps >>= 1; // always bit-shift
			curStepLen >>= 1; // halving the binary tree of walking
//printf("rs:%d  ", realSteps);
			if(curLen > curStepLen) {
				// Remaining length is in the upper half of this halving-step
				// We calculate halving-params.stepPointExponential like walking on a binary
				// tree while the loop is going!
				// Rem.: this shift does all divisions beforehand!
#ifdef EXPONENTIAL_ATTRITION
				ret <<= realSteps; // division by 2^(realSteps)
#else
//printf("***");
				ret = ret + ((ret >> params.attrExp) * realSteps);
#endif // EXPONENTIAL_ATTRITION

				// For iterating with the tree, we must update the len to the remaining
				// (the right side of the cut-down part of the stepping is kept)
				curLen -= curStepLen;

			}/* else {
				// Remaining lenght is in the lower half of the binary tree step
				// NO-OP
			}*/
		} while(realSteps);
	}

//printf("l:%d r:%d", len, ret);

	return ret;
#endif // SIMPLE_ATTRITION
#endif // NO_ATTRITION
}

/** Holds configuration data values for Homer */
struct HomerSetup final {
	/** Lenght of pixels with close to same magnitude to consider the area homogenous */
	int hodeltaLen = 6;

	/** 
	 * Delta value for telling if this pixel differs too much from the earlier or not.
	 * Must differ less than this to suspect a start of a homogenous area in our consideration
	 */
	int hodeltaDiff = 15;

	/**
	 * Delta value for telling if this pixel differs too much from the avarage of the earlier 
	 * (in case we are in an isHo) or not: must differ less than this
	 */
	int hodeltaAvgDiff = 15;

	/**
	 * The maximum difference of the current magnitude from the avarage between the min and max
	 * magnitudes of the currently homogenous area. If a pixel is found to change more than this
	 * we consider the area closed/ended! Similar to the minMaxDeltaMax - but this value is not
	 * a difference between the extremal values - but a difference from their avarage!
	 */
	int hodeltaMinMaxAvgDiff = 18;

	/**
	 * The maximum difference between minimal and maximal values in a homogenous area to consider
	 * it still being homogenous. Area change happens if a bigger than this change happens.
	 * BEWARE: This must be bigger than hodeltaMinMaxAvgDiff!
	 */
	int minMaxDeltaMax = 20;

	/** Uses lenAffect(..) to change the values in a returned copy - does not change the original  */
	inline HomerSetup applyLenAffection(int len, LenAffectParams params = LenAffectParams{}) noexcept {
		// Create a new setup with default values
		HomerSetup ret;

		// Apply affections and copy original data
		ret.hodeltaLen = lenAffect(this->hodeltaLen, len, params);
		ret.hodeltaDiff = lenAffect(this->hodeltaDiff, len, params);
		ret.hodeltaAvgDiff = lenAffect(this->hodeltaAvgDiff, len, params);
		ret.hodeltaMinMaxAvgDiff = lenAffect(this->hodeltaMinMaxAvgDiff, len, params);
		ret.minMaxDeltaMax = lenAffect(this->minMaxDeltaMax, len, params);

		return ret;
	}
};


/** 
 * A scanline-parser as described below.
 * Simple driver for instantly analysing 1D scanlines (in-place) for homogenous areas of interest.
 * MT: "Magnitude Type" and CT: "Magnitude Collector type" (later is for sum calculations)
 */
template<typename MT = uint8_t, typename CT = int>
class Homer final {
#ifdef HOMER_MEASURE_NEXT_BRANCHES
	unsigned int branch_1_looking = 0;
	unsigned int branch_2_reset = 0;
	unsigned int branch_3_closed = 0;
	unsigned int branch_4_stillopen = 0;
	unsigned int branch_5_susreset = 0;
	unsigned int branch_6_openedNew = 0;
#endif // HOMER_MEASURE_NEXT_BRANCHES
public:
	/** An inlined NOOP except when HOMER_MEASURE_NEXT_BRANCHES is #define-d */
	inline void flushBranchProfileData() {
#ifdef HOMER_MEASURE_NEXT_BRANCHES
		printf("branch_1_looking   = %u\n", branch_1_looking);
		printf("branch_2_reset     = %u\n", branch_2_reset);
		printf("branch_3_closed    = %u\n", branch_3_closed);
		printf("branch_4_stillopen = %u\n", branch_4_stillopen);
		printf("branch_5_susreset  = %u\n", branch_5_susreset);
		printf("branch_6_openedNew = %u\n", branch_6_openedNew);
		branch_1_looking = 0;
		branch_2_reset = 0;
		branch_3_closed = 0;
		branch_4_stillopen = 0;
		branch_5_susreset = 0;
		branch_6_openedNew = 0;
#endif // HOMER_MEASURE_NEXT_BRANCHES
	}
	/** 
	 * Create a driver for analysing 1D homogenous areas in scanlines
	 * - using default state
	 */
	Homer() noexcept {
		// Set default state
		reset();
	}

	/** 
	 * Create a driver for analysing 1D homogenous areas in scanlines
	 * - using default state and given setup
	 */
	Homer(HomerSetup setup) noexcept {
		// Save setup
		homerSetup = setup;

		// Set default state
		reset();
	}

	/**
	 * Set default state
	 * - resets all the system knows about its homarea, but keeps configuration as-is
	 */
	// Rem.: This is not inlined because it is rare!!!
	void NOINLINE reset() noexcept {
		// reset to default
		homarea = Homarea();
	}

	/** 
	 * Set default state then sets the last element as if it was the given one
	 * First resets all the system knows about its homarea, but keeps configuration 
	 * as-is then change this state with "last"
	 */
	// Rem.: This is not inlined because it is rare!!!
	void NOINLINE reset(MT last) noexcept {
		reset();
		homarea.last = last;
	}

	/** Send the next magnitude - return true if we are (still?) in an isHo area after the new element */
	inline bool next(MT mag) noexcept {
		if(LIKELY(homarea.isHo)) {
			// FAST-PATH: CONTINUE A HOMAREA - this nearly always happens!
			// ===========================================================
			// Decide if we need to CONTINUE this area

			// Apply lengthAffection to the homerSetup values when in a homogenous area!
			auto lenAffectedHomerSetup = homerSetup.applyLenAffection(homarea.getLen());

			// Check delta difference from the min/max magnitudes centerlne - too much indicates non-homogenity
			// Rem.: First the faster check so the optimizer can optimize jumps better...
			// Rem.: Because when we come here if we already have a "suspected" area, len is > 0
			//       and this is real value - not the zero indicating the zero len!!!
			bool tooMuchDiffFromMinMaxAvg = (abs(homarea.magMinMaxAvg() - mag) > lenAffectedHomerSetup.hodeltaMinMaxAvgDiff);
			// Check difference from the avarage being too much
			// Rem.: it is faster to multiply here twice at every pixel than to use magAvg() which uses division!!!
#ifdef SLOW_PRECISE_HOMER
			bool tooMuchDiffFromAvg = (abs((long long)homarea.getMagSum() - (long long)(mag * homarea.getLen()))
					> ((long long)lenAffectedHomerSetup.hodeltaAvgDiff * homarea.getLen()));
#endif // SLOW_PRECISE_HOMER
			if(LIKELY(!tooMuchDiffFromMinMaxAvg
#ifdef SLOW_PRECISE_HOMER
						&& !tooMuchDiffFromAvg
#endif // SLOW_PRECISE_HOMER
						)) {
				// Rem.: This will always return true EXCEPT when the min-max does not differ greatly
				//       because all other checks are done above...
				bool isOpenStill = homarea.tryOpenOrKeepWith(mag, lenAffectedHomerSetup.hodeltaLen, lenAffectedHomerSetup.minMaxDeltaMax);
				// Do our reset if someone closed the area
				if(LIKELY(isOpenStill)) {
#ifdef HOMER_MEASURE_NEXT_BRANCHES
					// 82% of times on last measurement!
					++branch_4_stillopen;
#endif // HOMER_MEASURE_NEXT_BRANCHES
					// Indicate if we are open or not
					//printf("C: %d\n", (int)isOpenStill);
					return isOpenStill;
				} else {
					reset(mag);
#ifdef HOMER_MEASURE_NEXT_BRANCHES
					++branch_3_closed;
#endif // HOMER_MEASURE_NEXT_BRANCHES
					return isOpenStill;
				}
			} else {
				//printf("B: %d,%d\n", tooMuchDiffFromMinMaxAvg, tooMuchDiffFromAvg);
				//printf("B1: abs(%d - %d) > %d [len:%d]", homarea.magMinMaxAvg(), mag, lenAffectedHomerSetup.hodeltaMinMaxAvgDiff, homarea.len);
				// Too big is the difference - reset current homarea :-(
				reset(mag); // Rem.: We need to set the "last" to "mag" here!
#ifdef HOMER_MEASURE_NEXT_BRANCHES
				++branch_2_reset;
#endif // HOMER_MEASURE_NEXT_BRANCHES
				return false;
			}
		} else {
			return slowNext(homarea, homerSetup, mag);
		}
	}

	/** Tells if we are in a homogenous area according to the last "next" call or not */
	inline bool isHo() const noexcept {
		return homarea.isHo;
	}

	/**
	 * Avarages of the magnitudes already collected for an area
	 * Rem.: returns zero in zero lengh areas and might return bogus values even when isHo() is false!
	 */
	inline MT magAvg() const noexcept {
		return homarea.magAvg();
	}

	/** Sum of the magnitues in the area - good for optimizations */
	inline CT getMagSum() const noexcept {
		return homarea.getMagSum();
	}

	/**
	 * When isHo() returns true - this is the lenght of the homogenous area.
	 * Otherwise it is the length of the currently suspected homogenous area (unsure!)
	 */
	inline int getLen() const noexcept {
		return homarea.len;
	}

private:
	/** Data holder for a (suspected) homogenous area and its state */
	struct Homarea final {
		/**
		 * Length of the homogenous area
		 * Not only non-zero when we are in a homogenous area according to the last few values 
		 * and the configuration!
		 * Rem.: In cases when lengh is non-zero and isHo is false
		 *       - that means that we seem to be starting a homogenous area - but not sure yet!
		 */
		int len = 0;

		/** Sum of the magnitudes */
		CT magSum = 0;

		// Rem.: min/max values are need to be set according to the magnitude type!
		/** Minimal magnitude in this area */
		MT magMin = std::numeric_limits<MT>::max();
		MT magMax = std::numeric_limits<MT>::min();

		/**
		 * The sum of the avarage differences 
		 */
		CT minMaxMagDiffAvgSum = 0;

		/**
		 * Only true when we are in a homogenous area according to the last few values
		 * and the configuration
		 * Rem.: In cases when this is false, but lengh is non-zero
		 *       - that means that we seem to be starting a homogenous area - but not sure yet!
		 */
		bool isHo = false;

		/** The last magnitude */
		MT last = 0;

		/**
		 * Try to increment and open this area with the given magnitude.
		 * hodeltaLen and minMaxDeltaMax are values from the setup
		 * Updates: length, magSum, min/max values, isHo
		 * Returns true if the area is (still) open - basically it returns isHo!
		 */
		inline bool tryOpenOrKeepWith(MT mag, int hodeltaLen, int minMaxDeltaMax) noexcept {
			// Update core data
			++len;
			magSum += mag;
			last = mag;
			// Update min/max
			if(UNLIKELY(magMax < mag)) magMax = mag;
			if(UNLIKELY(magMin > mag)) magMin = mag;

			// Checks: minMax and length checking
			// Rem.: Most checks that we check HERE are are those checks we do before
			//       we consider a homogenous area to be "open" (checks that can
			//       stop us "considering" and area to be homogenous)!
			// Rem.: The zero len is a special case and must  go through checking
			//       manually (otherwise there would be crazy big difference)
			bool lenCheck = isLenOK(hodeltaLen);
			bool minMaxCheck = isMinMaxDeltaMaxOk(minMaxDeltaMax);
			isHo = (lenCheck && minMaxCheck);
			return isHo;
		}

		// Rem.: This is inline so that the optimizer can keep values 
		//       from other inline calls where we use this when you use it directly too!
		/** Checks the current min/max magnitudes difference if it is too big or not */
		inline bool isMinMaxDeltaMaxOk(int minMaxDeltaMax) const noexcept {
			bool ret = ((len == 0) || ((magMax - magMin) < minMaxDeltaMax));
			//if(!ret) printf("***not(%d-%d<%d)@%d***\n", magMax, magMin, minMaxDeltaMax, len);
			return ret;
		}

		// Rem.: This is inline so that the optimizer can keep values 
		//       from other inline calls where we use this when you use it directly too!
		/** Checks the current length agains the provided one */
		inline bool isLenOK(int hodeltaLen) const noexcept {
			return (len >= hodeltaLen);
		}

		/**
		 * Avarages of the magnitudes already collected for an area
		 * - returns zero in zero lengh areas!
		 */
		inline MT magAvg() const noexcept {
			if(UNLIKELY(len == 0)) return 0;
			// this must fit into an MT as it is the avarage of MT typed values...
			auto ret = (MT) (magSum / len);
			return ret;
		}

		/** Sum of the magnitues in the area */
		inline CT getMagSum() const noexcept {
			return magSum;
		}

		/** Length of this area */
		inline int getLen() const noexcept {
			return len;
		}

		/** Avarage between the min and max magnitudes - returns zero at start (on len 0)! */
		inline MT magMinMaxAvg() const noexcept {
			if(UNLIKELY(len == 0)) return 0; // Against calculation errors
			else return ((magMax - magMin) / 2) + magMin; // Rem.: no division, but bit-shift usually
		}
	
		/**
		 * Devalvate "value" using len of this Homarea as the length
		 * Useful for: devalving check constraints so that they are relative to length 
		 * and not absolute! Many constrainst are best when relative.
		 *
		 * BEWARE: Only 0, 1, 2, 4, 8, ... "steps" are supported because of performance, so stepPointExponential
		 *         values 0, 1, 2, 3, 4 means the above mentioned powers of two when considering step counts!
		 * BEWARE: The fullAffectLenUpCons must be smaller than leastAffectLenBottCons. Just give both
		 *         of these parameters something big in order to default to not affect the value at all!
		 *
		 * Remarks:
		 * - Any length smaller than fullAffectLenUpCons will have the full "value"
		 * - Any length longer than leastAffectLenBottCons will have the value halved "step"-times...
		 * - Any length in-between is (approximately) interpolateda with "2^(stepPointExponential)" number of halving-steps!
		 * Remarks: Best is to use only a small number of "stepPointExponential" and the possibly longerst fullAffectLenUpCons
		 *          to achieve the best speed and branch prediction operations here.
		 */
		template<typename T>
		inline T lenAffect(T value, LenAffectParams params) const noexcept {
			lenAffect(value, len, params);
		}
	};

	// Rem.: Not inlined because this is the rare part and is only here to make the hot-spot more cache friendly!
	bool NOINLINE slowNext(Homarea &homarea, HomerSetup &homerSetup, MT mag) {
		if(!(abs((CT)homarea.last - (CT)mag) > homerSetup.hodeltaDiff)) {
			// The difference was small enough - but we are not in the homarea
			// ===============================================================
			// SUSPECTED NEW AREA?

			// Can we "open" an area? (Can we set isHo already?)
			// Rem.: When we are here, (abs((CT)homarea.last - (CT)mag) <= hodeltaDiff) is sure
			// Rem.: When we are here we are waiting for length to be enough!!!
			bool openedNew = homarea.tryOpenOrKeepWith(mag, homerSetup.hodeltaLen, homerSetup.minMaxDeltaMax);
			// ONLY RESET if there is a problem with the min-max delta max check!
			// We come here also when everything is good except the length, so we cannot rely on isHo!
			// Rem.: Because both this and the above is inline, the optimizer should optimize duplications...
			if(UNLIKELY(!homarea.isMinMaxDeltaMaxOk(homerSetup.minMaxDeltaMax))) {
				//printf("D: %d; ", homarea.len);
				reset(mag);
#ifdef HOMER_MEASURE_NEXT_BRANCHES
				++branch_5_susreset;
#endif // HOMER_MEASURE_NEXT_BRANCHES
			}
#ifdef HOMER_MEASURE_NEXT_BRANCHES
			// 15% of times on last measurement!
			else ++branch_6_openedNew;
#endif // HOMER_MEASURE_NEXT_BRANCHES
			// Indicate if we are open or not
			//printf("D: %d\n", (int)stillOpen);
			return openedNew;
		} else {
			// Looking for new homarea- but too big difference
			// ===============================================
			// Too big difference between the last and current magnitudes...
			// Reset the current homarea as there cannot be any current area
			reset(mag); // Rem.: We need to set the "last" to "mag" here!

			// NO AREA
			//printf("A: 0\n");
#ifdef HOMER_MEASURE_NEXT_BRANCHES
			++branch_1_looking;
#endif // HOMER_MEASURE_NEXT_BRANCHES
			return false;
		}
	}


	/** Current homogenous area */
	Homarea homarea;
	/** Current configuration */
	HomerSetup homerSetup;
};

#endif //FASTTRACK_HOMER_H
// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
