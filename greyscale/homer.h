#ifndef FASTTRACK_HOMER_H
#define FASTTRACK_HOMER_H

#include <vector>
#include <cstdint>
#include <cmath>
#include <limits> // for templated min-max integer values

/** Holds configuration data values for Homer */
struct HomerSetup {
	/** Lenght of pixels with close to same magnitude to consider the area homogenous */
	int hodeltaLen = 6;

	/** 
	 * Delta value for telling if this pixel differs too much from the earlier or not.
	 * Must differ less than this to suspect a start of a homogenous area in our consideration
	 */
	int hodeltaDiff = 13;

	/**
	 * Delta value for telling if this pixel differs too much from the avarage of the earlier 
	 * (in case we are in an isHo) or not: must differ less than this
	 */
	int hodeltaAvgDiff = 27;

	/**
	 * The maximum difference of the current magnitude from the avarage between the min and max
	 * magnitudes of the currently homogenous area. If a pixel is found to change more than this
	 * we consider the area closed/ended! Similar to the minMaxDeltaMax - but this value is not
	 * a difference between the extremal values - but a difference from their avarage!
	 */
	int hodeltaMinMaxAvgDiff = 10;

	/**
	 * The maximum difference between minimal and maximal values in a homogenous area to consider
	 * it still being homogenous. Area change happens if a bigger than this change happens.
	 * BEWARE: This must be bigger than hodeltaMinMaxAvgDiff!
	 */
	int minMaxDeltaMax = 25;
};


/** 
 * Simple driver for instantly analysing 1D scanlines (in-place) for homogenous areas of interest.
 * MT: "Magnitude Type" and CT: "Magnitude Collector type" (later is for sum calculations)
 */
template<typename MT = uint8_t, typename CT = int>
class Homer {
public:
	/** 
	 * Create a driver for analysing 1D homogenous areas in scanlines
	 * - using default state
	 */
	Homer() {
		// Set default state
		reset();
	}

	/** 
	 * Create a driver for analysing 1D homogenous areas in scanlines
	 * - using default state and given setup
	 */
	Homer(HomerSetup setup) {
		// Save setup
		homerSetup = setup;

		// Set default state
		reset();
	}

	/**
	 * Set default state
	 * - resets all the system knows about its homarea, but keeps configuration as-is
	 */
	inline void reset() noexcept {
		// reset to default
		homarea = Homarea();
	}

	/** 
	 * Set default state then sets the last element as if it was the given one
	 * First resets all the system knows about its homarea, but keeps configuration 
	 * as-is then change this state with "last"
	 */
	inline void reset(MT last) noexcept {
		reset();
		homarea.last = last;
	}

	/** Send the next magnitude - return true if we are (still?) in an isHo area after the new element */
	inline bool next(MT mag) noexcept {
		if(!homarea.isHo && (abs((CT)homarea.last - (CT)mag) > homerSetup.hodeltaDiff)) {

			// Looking for new homarea- but too big difference
			// ===============================================
			// Too big difference between the last and current magnitudes...
			// Reset the current homarea as there cannot be any current area
			reset(mag); // Rem.: We need to set the "last" to "mag" here!

			// NO AREA
			//printf("A: 0\n");
			return false;
		} else {
			// Either we are in a homarea or difference was small enough (or both)
			// ===================================================================
			if(homarea.isHo) {
				// CONTINUE AREA?

				// We were in an area already...
				// Can we continue this area?

				// Check delta difference from the min/max magnitudes centerlne - too much indicates non-homogenity
				// Rem.: First the faster check so the optimizer can optimize jumps better...
				// Rem.: Because when we come here if we already have a "suspected" area, len is > 0
				//       and this is real value - not the zero indicating the zero len!!!
				bool tooMuchDiffFromMinMaxAvg = (abs(homarea.magMinMaxAvg() - mag) > homerSetup.hodeltaMinMaxAvgDiff);
				// Check difference from the avarage being too much
				// Rem.: it is faster to multiply here twice at every pixel than to use magAvg() which uses division!!! 
				bool tooMuchDiffFromAvg = (abs((long long)homarea.getMagSum() - (long long)(mag * homarea.getLen()))
					   	> ((long long)homerSetup.hodeltaAvgDiff * homarea.getLen()));
				if(tooMuchDiffFromMinMaxAvg || tooMuchDiffFromAvg) {
					//printf("B: %d,%d\n", tooMuchDiffFromMinMaxAvg, tooMuchDiffFromAvg);
					//printf("B1: abs(%d - %d) > %d [len:%d]", homarea.magMinMaxAvg(), mag, homerSetup.hodeltaMinMaxAvgDiff, homarea.len);
					// Too big is the difference - reset current homarea :-(
					reset(mag); // Rem.: We need to set the "last" to "mag" here!
					return false;
				} else {
					// Rem.: This will always return true EXCEPT when the min-max does not differ greatly
					//       because all other checks are done above...
					bool isOpenStill = homarea.tryOpenOrKeepWith(mag, homerSetup.hodeltaLen, homerSetup.minMaxDeltaMax);
					// Do our reset if someone closed the area
					if(!isOpenStill) {
						reset(mag);
					}
					// Indicate if we are open or not
					//printf("C: %d\n", (int)isOpenStill);
					return isOpenStill;
				}
			} else {
				// SUSPECTED NEW AREA?

				// Can we "open" an area? (Can we set isHo already?)
				// Rem.: When we are here, (abs((CT)homarea.last - (CT)mag) <= hodeltaDiff) is sure
				// Rem.: When we are here we are waiting for length to be enough!!!
				bool openedNew = homarea.tryOpenOrKeepWith(mag, homerSetup.hodeltaLen, homerSetup.minMaxDeltaMax);
				// ONLY RESET if there is a problem with the min-max delta max check!
				// We come here also when everything is good except the length, so we cannot rely on isHo!
				// Rem.: Because both this and the above is inline, the optimizer should optimize duplications...
				if(!homarea.isMinMaxDeltaMaxOk(homerSetup.minMaxDeltaMax)) {
					//printf("D: %d; ", homarea.len);
					reset(mag);
				}
				// Indicate if we are open or not
				//printf("D: %d\n", (int)stillOpen);
				return openedNew;
			}
		}
	}

	/** Tells if we are in a homogenous area according to the last "next" call or not */
	inline bool isHo() {
		return homarea.isHo;
	}

	/**
	 * Avarages of the magnitudes already collected for an area
	 * Rem.: returns zero in zero lengh areas and might return bogus values even when isHo() is false!
	 */
	inline MT magAvg() {
		return homarea.magAvg();
	}

	/**
	 * When isHo() returns true - this is the lenght of the homogenous area.
	 * Otherwise it is the length of the currently suspected homogenous area (unsure!)
	 */
	inline int getLen() {
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
		inline bool tryOpenOrKeepWith(MT mag, int hodeltaLen, int minMaxDeltaMax) {
			// Update core data
			++len;
			magSum += mag;
			last = mag;
			// Update min/max
			if(magMax < mag) magMax = mag;
			if(magMin > mag) magMin = mag;

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
		inline bool isMinMaxDeltaMaxOk(int minMaxDeltaMax) {
			bool ret = ((len == 0) || ((magMax - magMin) < minMaxDeltaMax));
			//if(!ret) printf("***not(%d-%d<%d)@%d***\n", magMax, magMin, minMaxDeltaMax, len);
			return ret;
		}

		// Rem.: This is inline so that the optimizer can keep values 
		//       from other inline calls where we use this when you use it directly too!
		/** Checks the current length agains the provided one */
		inline bool isLenOK(int hodeltaLen) {
			return (len >= hodeltaLen);
		}

		/**
		 * Avarages of the magnitudes already collected for an area
		 * - returns zero in zero lengh areas!
		 */
		inline MT magAvg() {
			if(len == 0) return 0;
			// this must fit into an MT as it is the avarage of MT typed values...
			auto ret = (MT) (magSum / len);
			return ret;
		}

		/** Sum of the magnitues in the area */
		inline CT getMagSum() {
			return magSum;
		}

		/** Length of this area */
		inline int getLen() {
			return len;
		}

		/** Avarage between the min and max magnitudes - returns zero at start (on len 0)! */
		inline MT magMinMaxAvg() {
			if(len == 0) return 0; // Against calculation errors
			else return ((magMax - magMin) / 2) + magMin; // Rem.: no division, but bit-shift usually
		}
	};

	/** Current homogenous area */
	Homarea homarea;
	/** Current configuration */
	HomerSetup homerSetup;
};

#endif //FASTTRACK_HOMER_H
// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
