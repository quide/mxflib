/*! \file	index.h
 *	\brief	Definition of classes that handle index tables
 *  \note	This index table system is far from efficient
 *
 *	\version $Id: index.h,v 1.12 2011/01/10 10:42:09 matt-beard Exp $
 *
 */
/*
 *	Copyright (c) 2003, Matt Beard
 *
 *	This software is provided 'as-is', without any express or implied warranty.
 *	In no event will the authors be held liable for any damages arising from
 *	the use of this software.
 *
 *	Permission is granted to anyone to use this software for any purpose,
 *	including commercial applications, and to alter it and redistribute it
 *	freely, subject to the following restrictions:
 *
 *	  1. The origin of this software must not be misrepresented; you must
 *	     not claim that you wrote the original software. If you use this
 *	     software in a product, an acknowledgment in the product
 *	     documentation would be appreciated but is not required.
 *	
 *	  2. Altered source versions must be plainly marked as such, and must
 *	     not be misrepresented as being the original software.
 *	
 *	  3. This notice may not be removed or altered from any source
 *	     distribution.
 */
#ifndef MXFLIB__INDEX_H
#define MXFLIB__INDEX_H


namespace mxflib
{
	//! Structure for holding the result of an index table look-up
	class IndexPos : public RefCount<IndexPos>
	{
	public:
		UInt64 ThisPos;			//!< The position (in file package edit units) of the data of which Location indexes the start
								/*!< \note If Exact = false and OtherPos = true this will be the <b>un-reordered</b> or bytestream
								 *         position of a different edit unit of data whose location is returned in Location.
								 *         This happens if the exact location is not indexed for some reason such as a 
								 *         sparse or incomplete index table. The location returned will be a "hint" of
								 *         where to start looking for the un-indexed data.
		                         */
		Position Location;		//!< The location of the start of ThisPos edit unit in the essence container
		Rational PosOffset;		//!< The pos offset for this edit unit (if Offset = true, otherwise undefined)
		bool Exact;				//!< true if ThisPos is the requested edit unit and the location is for the requested sub-item.
								/*!< false if it is a preceeding edit unit or the requested sub-item could not be identified */
		bool OtherPos;			//!< true if ThisPos is not the requested edit unit
								/*!< \note This should be tested if "Exact" is false as the value of "ThisPos" will 
								 *         be the non-reordered position and may equal the requested location even though
								 *         Location does not index the requested edit unit */
		bool Offset;			//!< true if there is a pos offset (stored in PosOffset, only set if Exact = true)
		Int8 KeyFrameOffset;	//!< The offset in edit units to the previous key frame
		Int8 TemporalOffset;	//!< The offset in edit units from stored order, to display order
		Int64 KeyLocation;		//!< The location of the start of the keyframe edit unit in the essence container, ~0 if not available
		UInt8 Flags;			//!< The flags for this edit unit (zero if ThisPos is not the requested edit unit)
	};

	//! Smart pointer to an IndexPos
	typedef SmartPtr<IndexPos> IndexPosPtr;

	//! Structure for holding delta entries
	/*! \note This must be a struct for efficiency - don't make into a (complex) class!! */
	struct DeltaEntry
	{
		Int8	PosTableIndex;
		UInt8	Slice;
		UInt8	ElementDelta[4];		//!< Must be a UInt8 array to allow the struct to be exactly 6 bytes (otherwise compilers add padding!) 
	};

	//! Map of edit unit positions to index table segemnts
	typedef std::map<Position, IndexSegmentPtr> IndexSegmentMap;

	//! Class for holding index entries that may be out of order
	class IndexEntry : public RefCount<IndexEntry>
	{
	public:
		int TemporalOffset;
		int AnchorOffset;
		UInt8 Flags;
		UInt64 StreamOffset;
	};

	//! Smart pointer to index entry
	typedef SmartPtr<IndexEntry> IndexEntryPtr;

//// Map of edit unit with index entries
//typedef std::map<Int64, IndexEntryPtr> IndexEntryMap;
//typedef SmartPtr<IndexEntryMap> IndexEntryMapPtr;

	//! A class that holds entries for an index table that will be temporally reordered
	/*! Temporal offsets need to be written to entries that are either complete already or
	 *  have not yet been filled in.  
	 *  Once the entries have been written including their temporal offsets they can
	 *  be added to a proper index table
	 */
	class ReorderIndex : public RefCount<ReorderIndex>
	{
	protected:
		DataChunk IndexEntries;				//!< Data chunk holding the actual entries
		int CompleteEntryCount;				//!< Number of entries including all details (but not necessarily a temporal offset)
		int EntryCount;						//!< Number of entries containing a either full details or a temporal offset
											/*!< This is actually the index of the highest used entry plus one, so there may
											 *   be some entries that don't contain anything that come before the last counted entry.
											 *	 For example, if the only complete entry is entry[0], and entry[0] and entry[2]
											 *	 both contain a temporal offset then CompleteEntryCount = 1 and EntryCount = 3
											 */
		Position FirstPosition;				//!< The edit unit number of the first position in this index table
		UInt32 IndexEntrySize;				//!< The size of each index entry

	public:
		//! Initialise the ReorderIndex
		ReorderIndex(int UseIndexEntrySize)
		{
			// Set a high granularity to reduce overhead of frequent reallocation
			IndexEntries.SetGranularity(1024 * 16);

			CompleteEntryCount = 0;
			EntryCount = 0;

			mxflib_assert(UseIndexEntrySize);
			IndexEntrySize = UseIndexEntrySize;
		}

		//! Add a new entry to the table (setting flags and anchor offset)
		bool SetEntry(Position Pos, UInt8 Flags, Int8 AnchorOffset, UInt8 *Tables = NULL);

		//! Add a new entry to the table
		bool SetStreamOffset(Position Pos, Position StreamOffset);

		//! Set the temporal offset for an entry in the table
		bool SetTemporalOffset(Position Pos, Int8 TemporalOffset);

		//! Get the number of entries in the table
		Int32 GetEntryCount(void) { return CompleteEntryCount; };

		//! Commit entries to the specified index table
		Int32 CommitEntries(IndexTablePtr Index, Int32 Count = -1);
	};

	typedef SmartPtr<ReorderIndex> ReorderIndexPtr;
	typedef std::map<UInt32, ReorderIndexPtr> ReorderMap;


	//! Class that holds an index table
	class IndexTable : public RefCount<IndexTable>
	{
	public:
		Int64 IndexDuration;
		UInt32 IndexSID;
		UInt32 BodySID;
		Rational EditRate;

		//! Byte count for each and every edit unit, if CBR, else zero
		UInt32 EditUnitByteCount;

		//! Number of entries in BaseDeltaArray
		int BaseDeltaCount;

		//! Deltas for CBR data and base delta array for VBR segments
		DeltaEntry *BaseDeltaArray;

		//! Map of edit unit position to index entry for VBR
		IndexSegmentMap SegmentMap;

		int NSL;			//!< NSL as defined in SMPTE-337M (number of slices minus 1)
		int NPE;			//!< NPE as defined in SMPTE-337M (number of PosTable entries)
		int IndexEntrySize;	//!< Size of each index entry (11 + 4*NSL + 8*NPE)

//		//! Map of index entries that may be out of order
//		/*! The entries will be built into segments by function xxx() */
//		IndexEntryMap IndexOrderEntryMap;
//		IndexEntryMap EssenceOrderEntryMap;

		ReorderIndexPtr		Reorder;			//!< Pointer to our reorder index if we are using one (used for building reordered indexes)


	public:
		//! The lowest valid index position, used to flag omitted "start" parameters
		/*! DRAGONS: Why isn't this initialized here in the header file? Because MSVC 6 won't allow that!!
		 */
		static const Position IndexLowest;

	public:
		//! Construct an IndexTable with no CBRDeltaArray
		IndexTable() : IndexSID(0), BodySID(0), EditUnitByteCount(0) , BaseDeltaCount(0)
		{ 
			IndexDuration=0;
			EditRate.Numerator=0; 
			EditUnitByteCount=0; 
			NSL=0; 
			NPE=0; 
			IndexEntrySize=11; 
		};

		//! Free any memory used by BaseDeltaArray when this IndexTable is destroyed
		~IndexTable() 
		{ 
			if(BaseDeltaCount) delete[] BaseDeltaArray; 
		};

		//! Define the base delta entry array from another delta entry array
		void DefineDeltaArray(int DeltaCount, DeltaEntry *DeltaArray)
		{
			if(BaseDeltaCount) delete[] BaseDeltaArray;

			BaseDeltaCount = DeltaCount;
			if(!DeltaCount) return;
			
			// Build the new array
			BaseDeltaArray = new DeltaEntry[DeltaCount];
			memcpy(BaseDeltaArray, DeltaArray, DeltaCount * sizeof(DeltaEntry));

			// Slice numbers start at zero, PosTable numbers start at 1
			NSL = 0;
			NPE = 0;
			int i;
			for(i=0; i<DeltaCount; i++)
			{
				if(BaseDeltaArray[i].PosTableIndex > NPE) NPE = BaseDeltaArray[i].PosTableIndex;
				if(BaseDeltaArray[i].Slice > NSL) NSL = BaseDeltaArray[i].Slice;
			}
		
			// Calculate the size of each IndexEntry
			IndexEntrySize = (11 + 4*NSL + 8*NPE);
		}

		//! Define the base delta entry array from an array of element sizes
		/*! With this version of DefineDeltaArray Slice numbers are calculated and 
		 *	all PosTableIndex entries are set to 0. Whenever an element size has the
		 *	value zero a new slice is started
		 */
		void DefineDeltaArray(int DeltaCount, UInt32 *ElementSizes)
		{
			if(BaseDeltaCount) delete[] BaseDeltaArray;

			BaseDeltaCount = DeltaCount;
			if(!DeltaCount) return;

			// Build the new array
			BaseDeltaArray = new DeltaEntry[DeltaCount];

			// Slice numbers start at zero, PosTable numbers start at 1
			NSL = 0;
			NPE = 0;
			UInt32 Delta = 0;			//!< Running delta value for current slice
			int i;
			for(i=0; i<DeltaCount; i++)
			{
				// Start of a new slice?
				PutU32(Delta, BaseDeltaArray[i].ElementDelta);
				Delta += ElementSizes[i];
				BaseDeltaArray[i].PosTableIndex = 0;
				BaseDeltaArray[i].Slice = NSL;

				// End of a slice?
				if((i != (DeltaCount-1)) && (ElementSizes[i] == 0))
				{
					Delta = 0;
					NSL++;
				}
			}

			// Calculate the size of each IndexEntry
			IndexEntrySize = (11 + 4*NSL + 8*NPE);
		}

		//! Add an index table segment from an "IndexSegment" MDObject
		IndexSegmentPtr AddSegment(MDObjectPtr Segment);

		//! Add an index table segment from a raw DataChunk containing a section of un-parsed index table data
		/*! DRAGONS: This is far more efficient for loading the index table than using the general metadata functions */
		void AddSegments(DataChunkPtr &IndexChunk);

		//! Add an index table segment from a raw DataChunk containing an un-parsed "IndexSegment"
		/*! DRAGONS: This is far more efficient for loading the index table than using the general metadata functions */
		IndexSegmentPtr AddSegment(UInt8 const *pSegment, Length Size, int LenSize = 2);

		//! Create a new empty index table segment
		IndexSegmentPtr AddSegment(Int64 StartPosition);

		//! Get the segment containing a specified edit unit
		IndexSegmentPtr GetSegment(Position EditUnit);

		//! Add a single index entry creating segments as required
		bool AddIndexEntry(Position EditUnit, Int8 TemporalOffset, Int8 KeyFrameOffset, UInt8 Flags, UInt64 StreamOffset, 
						   int SliceCount = 0, UInt32 *SliceOffsets = NULL, 
						   int PosCount = 0, Rational *PosTable = NULL);

//#####
//##### DRAGONS: Should Lookup also check the pending items?
//#####
		//! Perform an index table look-up
		IndexPosPtr Lookup(Position EditUnit, int SubItem = 0, bool Reorder = true);

		//! Calculate the duration of this index table (the highest indexed position + 1)
		/*! DRAGONS: Also updates public member IndexDuration */
		Length GetDuration(void);

		//! Update the Stream Offset of an index entry
		void Update(Position EditUnit, UInt64 StreamOffset);

		//! Fudge to correct index entry
		void Correct(Position EditUnit, Int8 TemporalOffset, Int8 KeyFrameOffset, UInt8 Flags);

		//! Free memory by purging the specified range from the index
		void Purge(UInt64 FirstPosition, UInt64 LastPosition);

		//! Write this index table to a memory buffer
		size_t WriteIndex(DataChunk &Buffer);

		//! Get a pointer to the reorder index object (if one has been enabled)
		ReorderIndexPtr GetReorder(void)
		{
			return Reorder;
		}

		//! Enable reordering and get a pointer to the reorder index object
		ReorderIndexPtr EnableReorder(void)
		{
			if(!Reorder) Reorder = new ReorderIndex(IndexEntrySize);

			return Reorder;
		}

	};
}


namespace mxflib
{
	class IndexSegment : public RefCount<IndexSegment>
	{
	private:
		static PrimerPtr IndexPrimer;

	public:
		//! Table that owns this segment
		IndexTableParent Parent;
	
		//! Edit unit of the first entry in this segment
		Int64 StartPosition;

		//! Number of entries in DeltaArray
		int DeltaCount;

		//! Deltas for this segment
		DeltaEntry *DeltaArray;

		//! Number of entries in IndexEntryArray
		int EntryCount;

		//! Index Entries for this segment
		/*! \note This can't be an array of structs because they are variable length */
		DataChunk IndexEntryArray;

	private:
		//! Private constructor to force construction via AddIndexSegmentToIndexTable()
		IndexSegment() : EntryCount(0) { };

	public:
		//! Destructor cleans up the segment
		~IndexSegment() { if(DeltaCount) delete[] DeltaArray; };

		//! Index segment pseudo-constructor
		/*! \note <b>Only</b> call this from IndexTable::AddSegment() because it adds the segment to its SegmentMap */
		static IndexSegmentPtr AddIndexSegmentToIndexTable(IndexTablePtr ParentTable, Int64 IndexStartPosition);

		//! Add a single index entry
		bool AddIndexEntry(Int8 TemporalOffset, Int8 KeyFrameOffset, UInt8 Flags, UInt64 StreamOffset, 
						   int SliceCount = 0, UInt32 *SliceOffsets = NULL, 
						   int PosCount = 0, Rational *PosTable = NULL);

		//! Add multiple - pre-formed index entries
		bool AddIndexEntries(int Count, int Size, UInt8 const *Entries);

		//! Update the Stream Offset of an index entry
		void Update(Position EditUnit, UInt64 StreamOffset);
	};
}


namespace mxflib
{
	class IndexManager : public RefCount<IndexManager>
	{
	protected:
		bool UsesReordering;				//!< True if the index table uses reordering
		bool FormatFixed;					//!< True once we have started building an index - can't then change the format
											/*!< DRAGONS: There may be a need to allow changes later... */
		bool DataIsCBR;						//!< True if the index table will be CBR
		int StreamCount;					//!< Number of streams (including the main stream)
		int StreamListSize;					//!< Size of PosTableList and ElementSizeList arrays
		int *PosTableList;					//!< PosTableIndex for each stream
		UInt32 *ElementSizeList;			//!< ElementSize for each stream
		int MasterStream;					//!< The Stream ID of the "master" stream that can set flags and key offset, normally the main stream "0"

		struct IndexData
		{
			int			Status;				//!< Status of this data
											/*!<   bit 0 = stream offset set, bit 1 = temporal offset set, bit 2 = temporal diff set */
			int			Flags;				//!< Flags for this edit unit
			int			KeyOffset;			//!< Key frame offset for this edit unit
			int			TemporalOffset;		//!< Temporal offset for this edit unit
			int			TemporalDiff;		//!< Difference between this edit unit and the edit unit whose stream offsets are store here
											/*!< This is the opposite of TemporalOffset. Temporal Offset gives the offset from the entry
											 *   indexed by a given edit unit to the entry unit holding that edit unit's stream offsets,
											 *   and TemporalDiff give the offset from the entry holding an edit unit's stream offset to
											 *   the entry indexed by that edit unit.
											 */
			UInt64		StreamOffset[1];	//!< Array of stream offsets, one for the main stream and one per sub-stream
											/*!< \note This array is variable length so the entire structure is also variable length */
		};

		int ManagedDataEntrySize;			//!< Size of each entry in the ManagedData array (depends on number of sub streams)

		std::map<Position, IndexData*> ManagedData;
											//!< Map of IndexData entries for all recorded edit units

		/* DRAGONS: Provisional entries are not currently implemented */
		IndexData *ProvisionalEntry;		//!< Provisional entry, not yet added to ManagedData
		Position ProvisionalEditUnit;		//!< Edit unit of ProvisionalEntry

		std::map<Position, int> UnsatisfiedTemporalOffsets;
											//!< Temporal offsets for unknown (possibly future) entries

		std::map<Position, int> UnsatisfiedTemporalDiffs;
											//!< Temporal diffs for unknown (possibly future) entries

		UInt32 BodySID;						//!< The BodySID of the data being indexed
		UInt32 IndexSID;					//!< The IndexSID of any index table generated
		Rational EditRate;					//!< The edit rate of the indexed data

		std::map<int, Position> EntryLog;	//!< Log of edit units of entries of interest
		int NextLogID;						//!< Next ID to use for LogEntry
		bool LogWrapped;					//!< True if NextLogID has wrapped back to 0
		int LogNextEntry;					//!< If >= 0 the next entry recorded should be logged with this as the log ID

		bool AcceptNextEntry;				//!< True if next entry should be accepted regardless of other acceptance rules

		Position LastNewEditUnit;			//!< Edit unit of the last entry added

		bool ValueRelativeIndexing;			//!< Flag to allow value-relative indexing
											/*!< \note This is NOT implemented in the IndexManager, but must be handled by the caller */

		Position SubRangeOffset;			//!< An offset to apply to correct position if sub-ranging (number of edit units discarded at the start of the essence)



		Int64 IndexDuration;                //!Used to froce duration of CBR index if needed

	public:
		//! Construct with main stream details
		IndexManager(int PosTableIndex, UInt32 ElementSize);

		//! Free any memory used
		~IndexManager()
		{
			delete[] PosTableList;
			delete[] ElementSizeList;
			
			std::map<Position, IndexData*>::iterator it = ManagedData.begin();
			while ( ! ManagedData.empty() )
			{
				delete[] (UInt8*)(*it).second;
				ManagedData.erase(it);
				it = ManagedData.begin();
			}
			
			if(ProvisionalEntry) delete[] ProvisionalEntry;
		}

		//! Set the BodySID
		void SetBodySID(UInt32 SID) { BodySID = SID; };

		//! Set the IndexSID
		void SetIndexSID(UInt32 SID) { IndexSID = SID; };

		//! Set the edit rate from a rational
		void SetEditRate(Rational Rate) { EditRate = Rate; };

		//! Set the edit rate from numerator and denominator
		void SetEditRate(Int32 Rate_n, Int32 Rate_d) { EditRate.Numerator = Rate_n;  EditRate.Denominator = Rate_d;};

		//! Get the BodySID
		UInt32 GetBodySID(void) { return BodySID; };

		//! Get the IndexSID
		UInt32 GetIndexSID(void) { return IndexSID; };

		//! Get the edit rate
		Rational GetEditRate(void) { return EditRate; };

		//! Add a sub-stream
		/*! \return Sub-stream ID or 0 if error */
		int AddSubStream(int PosTableIndex, UInt32 ElementSize);

		//! Update the PosTableIndex for a given stream
		void SetPosTableIndex(int StreamID, int PosTableIndex)
		{
			if(StreamID < StreamCount) PosTableList[StreamID] = PosTableIndex;
		}

		//! Set the master stream ID, this stream can change flags and key offset for an entry
		void SetMasterStream(int StreamID) { MasterStream = StreamID; }

		//! Add an edit unit (of a stream) without a known offset
		void AddEditUnit(int SubStream, Position EditUnit, int KeyOffset = 0, int Flags = -1);

		//! Set the offset for a particular edit unit of a stream
		void SetOffset(int SubStream, Position EditUnit, UInt64 Offset, int KeyOffset = 0, int Flags = -1);

		//! Accept or decline an offered edit unit (of a stream) without a known offset
		bool OfferEditUnit(int SubStream, Position EditUnit, int KeyOffset = 0, int Flags = -1);

		//! Accept or decline an offered offset for a particular edit unit of a stream
		bool OfferOffset(int SubStream, Position EditUnit, UInt64 Offset, int KeyOffset = 0, int Flags = -1);

		//! Set the temporal offset for a particular edit unit
		void SetTemporalOffset(Position EditUnit, int Offset);

		//! Accept or decline an offered temporal offset for a particular edit unit
		bool OfferTemporalOffset(Position EditUnit, int Offset);

		//! Set the key-frame offset for a particular edit unit
		void SetKeyOffset(Position EditUnit, int Offset);

		//! Accept or decline an offered key-frame offset for a particular edit unit
		bool OfferKeyOffset(Position EditUnit, int Offset);

		//! Accept provisional entry
		/*! \return The edit unit of the entry accepted - or IndexLowest if none available */
		Position AcceptProvisional(void)
		{
			if(!ProvisionalEntry) return IndexTable::IndexLowest;

			// Add the entry to the managed data
			ManagedData.insert(std::pair<Position, IndexData*>(ProvisionalEditUnit, ProvisionalEntry));
			LastNewEditUnit = ProvisionalEditUnit;

			// The entry no longer exists as a provisional entry
			ProvisionalEntry = NULL;

			return ProvisionalEditUnit;
		}

		//! Read the edit unit of the last entry added (or IndexLowest if none added)
		Position GetLastNewEditUnit(void) { return LastNewEditUnit; }

		//! Accept next edit unit offered
		void AcceptNext(void)
		{
			AcceptNextEntry = true;
		}

		//! Log next edit unit offered
		/*! The next edit unit stored is recorded in the log.
		 *  \return An ID used in a call to CheckLog() to get the EditUnit when available (-1 if error)
		 */
		int LogNext(void);

		//! Accept the next edit unit offered and log it
		int AcceptLogNext(void) { AcceptNext();  return LogNext(); }

		//! Read the edit unit from a given log slot (or IndexLowest if not available)
		Position ReadLog(int LogID)
		{
			std::map<int, Position>::iterator it;
			it = EntryLog.find(LogID);
			if(it == EntryLog.end()) return IndexTable::IndexLowest;
			return (*it).second;
		}

		//! Flush index data to free memory
		void Flush(Position FirstEditUnit, Position LastEditUnit);

		//! Get the edit unit of the first available entry
		UInt64 GetFirstAvailable(void);

		//! Get the edit unit of the last available entry
		/*! In a reordered index this returns the last of the contiguous completed entries */
		UInt64 GetLastAvailable(void);

		//! Generate a CBR index table or empty VBR index table for the managed index
		IndexTablePtr MakeIndex(void);

		//! Add all complete entries in a range to the supplied index table
		/*! \return Number of index entries added */
		int AddEntriesToIndex(IndexTablePtr Index, Position FirstEditUnit = IndexTable::IndexLowest, Position LastEditUnit = UINT64_C(0x7fffffffffffffff))
		{
			return AddEntriesToIndex(false, Index, FirstEditUnit, LastEditUnit);
		}

		//! Add all complete entries in a range to the supplied index table - allow reordering to be undone
		int AddEntriesToIndex(bool UndoReorder, IndexTablePtr Index, Position FirstEditUnit = IndexTable::IndexLowest, Position LastEditUnit = UINT64_C(0x7fffffffffffffff));

		//! Force an index that appears to be CBR to be treated as VBR
		/*! This allows non-indexed KLVs to cause the essence stream to be VBR in nature */
		void ForceVBR(void) { DataIsCBR = false; };

		//! Access function to read CBR flag
		bool IsCBR(void) { return DataIsCBR; };

		//! Set value-relative indexing flag
		/*! Value-relative indexing will produce index tables that count from the first byte of the KLV 
		 *  of clip-wrapped essence rather than the key. These tables can be used internally but must not
		 *  be written to a file as they are not 377M complient */
		void SetValueRelativeIndexing(bool Val) { ValueRelativeIndexing = Val; }

		//! Get value-relative indexing flag
		/*! Value-relative indexing will produce index tables that count from the first byte of the KLV 
		 *  of clip-wrapped essence rather than the key. These tables can be used internally but must not
		 *  be written to a file as they are not 377M complient */
		bool GetValueRelativeIndexing(void) { return ValueRelativeIndexing; }

		//! Set the sub-range offset
		void SetSubRangeOffset(Position Offset) { SubRangeOffset = Offset; }


		void SetIndexDuration( Int64 newVal) {IndexDuration=newVal; };

	protected:
		//! Access an entry in the managed data array - creating or extending the array as required
		IndexData *GetArrayEntry(Position EditUnit);

		//! Log an edit unit if it is of interest
		void Log(Position EditUnit)
		{
			if(LogNextEntry >= 0)
			{
				EntryLog[LogNextEntry] = EditUnit;
				LogNextEntry = -1;
			}
		}
	};

	typedef SmartPtr<IndexManager> IndexManagerPtr;
}


#endif // MXFLIB__INDEX_H


// Temporary Notepad...
//
//
//Criteria for accepting edit units:
//
// Making full index:  Accept all
// Making index segment:   Accept if in range
// Making sparse index (per n edit units): Accept if divisible by n
// Making sparse index (per something else): Accpet first edit unit offered each section
