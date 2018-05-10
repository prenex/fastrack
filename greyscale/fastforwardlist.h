#ifndef _FASTFORWARD_LIST_H
#define _FASTFORWARD_LIST_H

#include<array>         // std::array
#include<utility>       // std::pair

// You need to define this if you want range checks:
/*#define FFL_INSERT_RANGE_CHECK 1*/

// Rem.: BASICALLY JUST AN INTEGER WITH MORE TYPE SAFETY :-)
/**
 * Simple iterator-like index to an element of a FastForwardList. Useful for getting the successor and the value.
 */
class FFLPosition final {
	template<typename U, int MAX>
	friend class FastForwardList; // let them use the underlying index and constructor
private:
	// Rem.: This is much faster then always returning the complete NODE!!!
	int index;

	/** Construct the position handle with i as its underlying index */
	constexpr FFLPosition(int i) noexcept : index(i) {}
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
 * Rem.: Insertion/deletion speed is faster when MAX is 
 * power-of-two minus ONE! (like 63, 127, etc)
 */
template<typename T, int MAX>
class FastForwardList {
	// An elem of type T and the index of next
	using NODE = std::pair<T, int>;

	// The current length of this list
	int curLen;

	// Index of the current head element
	// -1 represents the nullptr in indices
	// Rem.: This being right before data array means to ensure good
	//       cache behaviour when asking the head and iterating over
	//       a short list. It will be nearly always cache hit then!
	int headIndex;

	// Our list is represented as an array
	// Rem.: this enables simple default std::move and copy!!!
	// Rem.: Maybe better than two distict arrays - but it depends...
	// Rem.: THIS VARIABLE COMING LAST ENSURES A GOOD CACHE BEHAVIOUR!
	//       The reason is that in case the list is always small, it will
	//       fit easily into the cache lines so asking for the head will
	//       just load teh whole list, because headIndex is read above!
	std::array<NODE, MAX> data;

	// Data structure for keeping unlink-data
	class HoleKeeper final {
		// Contains indices of "holes" of nodes we have unlinked
		std::array<int, MAX+1> holes;

		// Start of the unlinkHoles circular queue
		unsigned int holeStart;
		// End of the unlinkHoles circular queue
		unsigned int holeEnd;
	public:
		// Updates holeStart, holeEnd and unlinkHoles
		inline void addHolePos(int unlinkPos) {
#ifdef FFL_INSERT_RANGE_CHECK
			// When range checking is on, we should do nothing if:
			// - The two indices are equal and the circular queue is full!
			// - When unlinkPos is negative (Nil)
			// - When unlinkPos is too big (>MAX)
			if((holeStart == holeEnd)
			  ||(unlinkPos < 0)
			  // Rem.: Here we deliberately use MAX and not (MAX+1)!
			  ||(unlinkPos > MAX) {
				return;
			}
#endif
			// Rem.: Because the two arrays are of the same size
			//       theoretically we never get the state when holeStart == holeEnd
			// Save the unlink position
			holes[holeEnd] = unlinkPos;
			// Rem.: This operation is fastest when MAX is (power of two) - 1
			// as the compiler should optimise it as a binary & operator!
			holeEnd = (holeEnd + 1) % (MAX+1);
		}

		/** Gets a previously occupied, but unlinked hole - undefined when there is no such! */
		inline int getHolePos() {
			// We need to increment first to ensure invariants!
			// When the circular buffer is empty, there Start-End point nearby
			// each other and in this circular End is next of start.
			// It is easy to see that we need to increment BEFORE reading when
			// this is the first reading ever.
			holeStart = (holeStart + 1) % (MAX+1);
			// Return the previously saved unlink position
			int ret = holes[holeStart];	
			// Rem.: This operation is fastest when MAX is (power of two) - 1
			// as the compiler should optimise it as a binary & operator!
			return ret;
		}

		/** Tells if there is at least one available hole to get */
		inline bool hasHole() {
			return (holeStart != holeEnd);
		}
	};

	// Data structure for keeping unlink-data
	HoleKeeper holeKeeper;
public:
	/** This is a logical position before the head. Useful for inserting before head! */
	static constexpr FFLPosition NIL_POS = FFLPosition(-1);

	FastForwardList() : curLen(0), headIndex(-1) {}

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
	inline void reset() noexcept {
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
		return (data[positionFromTheList.index].first);
	}

	/**
	 * Returns the const reference to the stored value for 
	 * the given non-NILL position from this list
	 */
	inline const T& operator[](FFLPosition positionFromTheList) const noexcept {
		return (data[positionFromTheList.index].first);
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
	 *         (beware that we always return true if range checking is off!)
	 */
	inline bool insertAfter(T element, FFLPosition position) noexcept {
		// TODO: implement usage of holes
#ifdef FFL_INSERT_RANGE_CHECK
		// Do range check to ensure: (curLen+1 <= MAX)
		if(curLen < MAX) {
#endif
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

			// 4.) Update head pointer when we add at the front
			if(position.isNil()) {
				headIndex = curLen;
			}

			// Update state that defines if we are isEmpty() or not:
			// Update size if range checking is on
			++curLen;
			// If we are here we surely return true as
			// either the range check was ok, or we do 
			// not care for range checking...
			return true;
#ifdef FFL_INSERT_RANGE_CHECK
		} else {
			// Range check failed and there is no place
			return false;
		}
#endif
	}

	/**
	 * Teies to unlink/delete the node at the given position.
	 * Returns a position AFTER the unlinked element
	 * Rem.: Might return NIL_POS on range check errors! 
	 * Rem.: The element at position will get changed to point to the successor!
	 */
	inline FFLPosition unlinkAfter(FFLPosition position) noexcept {
#ifdef FFL_INSERT_RANGE_CHECK
		if(position.index > MAX) {
			// No deletion because of index-checking
			return NIL_POS;
		}
#endif
		// Get successor position
		int unlinkPos;
		if(position.index < 0) {
			// NIL_POS parameter means that we delete the current HEAD
			unlinkPos = headIndex;
		} else {
			// Otherwise we 
			unlinkPos = data[position.index].second;
		}

#ifdef FFL_INSERT_RANGE_CHECK
		if((unlinkPos < 0) || unlinkPos > MAX) {
			// No deletion because there is nothing to delete
			// (just another index checking)
			return NIL_POS;
		}
#endif
		// Get the 'next' of the unlinked position
		FFLPosition succUnlinkPos = next(FFLPosition(unlinkPos));

		// Get the position that is the 'next' of the elem to 'unlink'
		if(position.index < 0) {
			// NIL_POS parameter means that we delete the current HEAD
			// so we need to update the head pointer only after the deletion
			headIndex = succUnlinkPos.index;
		} else {
			// Unlink - quite literally - by 
			data[position.index].second = succUnlinkPos.index;
		}

		// Mark hole for reuse
		holeKeeper.addHolePos(unlinkPos);

		// Return the position of the successor of the unlinked element
		return succUnlinkPos;
	}
};

#endif // _FASTFORWARD_LIST_H

// vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
