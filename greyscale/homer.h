#include <vector>
#include <cstdint>
#include <cmath>

/** 
 * Simple driver for instantly analysing 1D scanlines (in-place) for homogenous areas of interest.
 * MT: "Magnitude Type" and CT: "Magnitude Collector type" (later is for sum calculations)
 */
template<typename MT = uint8_t, typename CT = int>
class Homer {
public:
	/** Holds configuration data values for Homer */
	struct HomerSetup {
		/** Lenght of pixels with close to same magnitude to consider the area homogenous */
		int hodeltaLen = 5;

		/** Delta value for telling if this pixel differs too much from the earlier or not: must differ less than this */
		int hodeltaDiff = 30;

		/** Delta value for telling if this pixel differs too much from the avarage of the earlier (in case we are in an isHo) or not: must differ less than this */
		int hodeltaAvgDiff = 30;
	};

	/** Create a driver for analysing 1D homogenous areas in scanlines - using default state */
	Homer() {
		// Set default state
		reset();
	}

	/** Create a driver for analysing 1D homogenous areas in scanlines - using default state and given setup */
	Homer(HomerSetup setup) {
		// Save setup
		homerSetup = setup;

		// Set default state
		reset();
	}

	/** Set default state - resets all the system knows about its homarea, but keeps configuration as-is */
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

			// No area
			return false;
		} else {
			// Either we are in a homarea or difference was small enough (or both)
			// ===================================================================
			if(homarea.isHo) {
				// We were in an area already...
				// Can we continue this area?
				if(abs((CT)homarea.magAvg() - (CT)mag) > homerSetup.hodeltaAvgDiff) {
					// Too big is the difference - reset current homarea :-(
					reset(mag); // Rem.: We need to set the "last" to "mag" here!
					return false;
				} else {
					// Rem.: This will always return true - but we update the data
					return homarea.tryOpenWith(mag, homerSetup.hodeltaLen);
				}
			} else {
				// Can we "open" an area? (Can we set isHo already?)
				// Rem.: When we are here, (abs((CT)homarea.last - (CT)mag) <= hodeltaDiff) is sure
				return homarea.tryOpenWith(mag, homerSetup.hodeltaLen);
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

	/** When isHo() returns true - this is the lenght of the homogenous area. Otherwise it is the length of the currently suspected homogenous area (unsure!) */
	inline int getLen() {
		return homarea.len;
	}

private:
	
	/** Data holder for a a homogenous area / state */
	struct Homarea {
		/**
		 * Length of the homogenous area
		 * Not only non-zero when we are in a homogenous area according to the last few values and the configuration!
		 * Rem.: In cases when lengh is non-zero and isHo is false - that means that we seem to be starting a homogenous area - but not sure yet!
		 */
		int len = 0;

		/** Sum of the magnitudes */
		CT magSum = 0;

		/**
		 * Only true when we are in a homogenous area according to the last few values and the configuration
		 * Rem.: In cases when this is false, but lengh is non-zero - that means that we seem to be starting a homogenous area - but not sure yet!
		 */
		bool isHo = false;

		/** The last magnitude */
		MT last = 0;

		/**
		 * Try to increment and open this area with the given magnitude. Returns true if the area is long enough and got opened (isHo turned true).
		 */
		inline bool tryOpenWith(MT mag, int hodeltaLen) {
			++len;
			magSum += mag;
			last = mag;

			isHo = (len >= hodeltaLen);
			return isHo;
		}
public:
		/** Avarages of the magnitudes already collected for an area - returns zero in zero lengh areas! */
		inline MT magAvg() {
			if(len == 0) return 0;
			// this must fit into an MT as it is the avarage of MT typed values...
			auto ret = (MT) (magSum / len);
			return ret;
		}
	};

	/** Current homogenous area */
	Homarea homarea;
	/** Current configuration */
	HomerSetup homerSetup;
};

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
