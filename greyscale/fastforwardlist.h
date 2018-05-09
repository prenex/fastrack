#ifndef _FASTFORWARD_LIST_H
#define _FASTFORWARD_LIST_H

#include<array>         // std::array
#include<utility>       // std::pair

// Rem.: BASICALLY JUST AN INTEGER WITH MORE TYPE SAFETY :-)
/** Simple iterator-like index to an element of a FastForwardList. Useful for getting the successor and the value. */
class FFLPosition final {
	friend class FastForwardList; // let them use the underlying index and constructor
private:
	// Rem.: This is much faster then always returning the complete NODE!!!
	int index;

	/** Construct the position handle with i as its underlying index */
	FFLPosition(int i) noexcept {
		index = i;
	}
public:
	/**
	 * Returns true if the handle is not referring to any element.
	 * A FastForwardList cannot return anything reasonable for an invalid handle.
	 */
	inline bool isNil() noexcept {
		// We only check against -1 because when someone uses the list
		// properly (as in the handle does not come from an other list)
		// then the index is either negative (nil index) or a good one!
		return (index < 0);
	}
};

/**
 * A simple forward list like thing with better caching
 * when the maximum number of elements is known beforehand.
 * Useful when insertion in the middle is needed, but 
 * with fastest speed and access in realtime code!
 *
 * Rem.: Most optimized for "small" lists but good all-round.
 */
template<typename T, int MAX>
class FastForwardList {
	// An elem of type T and the index of next
	using NODE = std::pair<T, int>;

	// The current length of this list
	int curLen = 0;

	// Index of the current head element
	// -1 represents the nullptr in indices
	// Rem.: This being right before data array means to ensure good
	//       cache behaviour when asking the head and iterating over
	//       a short list. It will be nearly always cache hit then!
	int headIndex = -1;

	// Our list is represented as an array
	// Rem.: this enables simple default std::move and copy!!!
	// Rem.: Maybe better than two distict arrays - but it depends...
	// Rem.: THIS VARIABLE COMING LAST ENSURES A GOOD CACHE BEHAVIOUR!
	//       The reason is that in case the list is always small, it will
	//       fit easily into the cache lines so asking for the head will
	//       just load teh whole list, because headIndex is read above!
	std::array<NODE, MAX> data;

public:
	/** This is a logical position before the head. Useful for inserting before head! */
	constexpr FFLPosition NIL_POS = FFLPosition(-1);

	// Move and copy constructors are just the default generated ones!
	FastForwardList(const FastForwardList& ffl)            = default;
	FastForwardList(FastForwardList&& ffl)                 = default;
	FastForwardList& operator=(const FastForwardList& ffl) = default;
	FastForwardList& operator=(FastForwardList&& ffl)      = default;

	/**
	 * Get a handle to the head.
	 * Rem.:  In case of empty list returned.isNil() == true
	 */
	inline FFLPosition head() noexcept {
		// This is just a return of an integer - but typesafe
		// Rem.: (-1) means that head() is called on an empty list 
		return FFLPosition(headIndex);
	}

	/**
	 * Reset this forward list for reuse in-place.
	 * Rem.: Much faster than assigning an empty list!
	 * Rem.: Every earlier handle is considered invalid!
	 */
	inline reset() noexcept {
		// This should be enough
		headIndex = -1;
		curLen=0;
	}

	/**
	 * Gets the next position after the provided one.
	 * This function does returns an invalid position 
	 * if the list ended with the provided current position!
	 */
	inline FFLPosition next(FFLPosition current) noexcept {
		// This is just a return of an integer - but typesafe
		return FFLPosition(data[current.index].second);
	}

	/**
	 * Returns reference to the stored value for the given
	 * non-NILL position from this list.
	 */
	inline T& operator[](FFLPosition positionFromTheList) noexcept  {
		return &(data[positionFromTheList.index].first);
	}

	/**
	 * Returns the const reference to the stored value for 
	 * the given non-NILL position from this list
	 */
	inline const T& operator[](FFLPosition positionFromTheList) const noexcept {
		return &(data[positionFromTheList.index].first);
	}

	/**
	 * Inserts a copy of the provided element as the new head. The earlier
	 * head becomes the "next" after the new one - if there was space for it.
	 * Returns false only when there is no more place to insert the element!
	 */
	inline bool push_front(T element) noexcept {
		// When -1 is given it is basically the same as if insertAfter(head())
		// is called on the very first element - and handled of course properly.
		return insertAfter(element, FFLPosition(-1));
	}

	inline bool isEmpty() {
		return (curLen == 0);
	}

	// TODO: do something to "compact the list" maybe? Or insert at an "empty" position?
	// TODO: how do we know which positions are empty when not on the end?
	/**
	 * Inserts a copy of the provided element AFTER the provided position.
	 * Rem.: insertAfter(elem, head()); ensured to work for an empty list!
	 * Returns false only when there is no more place to insert the element!
	 */
	inline bool insertAfter(T element, FFLPosition position) noexcept {
		// Do range check to ensure: (curLen+1 <= MAX)
		if(curLen < MAX) {
			// Do the core stuff:
			// 1.) Copy data to the "new" node at the end
			data[curLen].first = element;
			// 2.) Save "position.next" and UPDATE the earlier to point to us
			// Rem.: We need to handle the spec. case of  the completely empty list!
			// Rem.: In most architectures forwards jumps are not predicted to be taken
			//       so we try writing the code knowing this and helping the CPU where can!
			// Rem.: WE also need to handle the case when position is (-1) because we a push_front!
			int nextToUse;
		   	if(!position.isNil()) {
				// Fast-path: adding non-head element
				nextToUse = data[position.index].second;
				data[position.index].second = curLen;
			} else {
				// Slower-path: adding new head
				if(isEmpty()) {
					// Completely empty list (no head)
					// Set (-1) as nextptr as it stands for NIL
					nextToUse = (-1);
				} else {
					// Adding a new head BEFORE an existing head
					// Save its index as our "next" value
					nextToUse = (head().index);
				}
			}
			// 3.) Update the 'next' of the added node holding the new element to the saved one.
			//     This ensures the proper linkage
			data[curLen].second = nextToUse;

			// 4.) Update head pointer when there is no valid head pointer right now
			if(isEmpty()) {
				headIndex = curLen;
			}

			// Update state that defines if we are isEmpty() or not:
			// Update size if range checking is on
			++curLen;
			// If we are here we surely return true as
			// either the range check was ok, or we do 
			// not care for range checking...
			return true;
		} else {
			// Range check failed and there is no place
			return false;
		}
	}
};

#endif // _FASTFORWARD_LIST_H

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
