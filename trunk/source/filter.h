/*

	Most of this file are DirectShow base classes
	Copyright (c) 1992-2002 Microsoft Corporation.  All rights reserved.
	Some unused classes, methods and parameters were removed.
	VisualizationFilter is mixed from CBaseFilter+CTransformFilter+CTransInPlaceFilter

	*/

#include "util.h"


//------------------------------------------------------------------------------
// File: WXUtil.h
//------------------------------------------------------------------------------

// eliminate spurious "statement has no effect" warnings.
#pragma warning(disable: 4705)

// wrapper for whatever critical section we have
class CCritSec {

	// make copy constructor and assignment operator inaccessible
	CCritSec(const CCritSec &refCritSec);
	CCritSec &operator=(const CCritSec &refCritSec);

	CRITICAL_SECTION m_CritSec;

public:
	CCritSec() {
		InitializeCriticalSection(&m_CritSec);
	};
	~CCritSec() {
		DeleteCriticalSection(&m_CritSec);
	};
	void Lock() {
		EnterCriticalSection(&m_CritSec);
	};
	void Unlock() {
		LeaveCriticalSection(&m_CritSec);
	};
};



// miscellaneous string conversion functions
// NOTE: as we need to use the same binaries on Win95 as on NT this code should
// be compiled WITHOUT unicode being defined.  Otherwise we will not pick up
// these internal routines and the binary will not run on Win95.

#ifndef UNICODE
#define lstrcpyW lstrcpyWInternal
#define lstrcmpW lstrcmpWInternal
#define lstrcmpiW lstrcmpiWInternal
#define lstrlenW lstrlenWInternal
LPWSTR WINAPI lstrcpyWInternal(LPWSTR lpString1, LPCWSTR lpString2);
int WINAPI lstrcmpWInternal(LPCWSTR lpString1, LPCWSTR lpString2);
int WINAPI lstrcmpiWInternal(LPCWSTR lpString1, LPCWSTR lpString2);
int WINAPI lstrlenWInternal(LPCWSTR lpString);
#endif



// Return a wide string - allocating memory for it
// Returns:
//    S_OK          - no error
//    E_POINTER     - ppszReturn == NULL
//    E_OUTOFMEMORY - can't allocate memory for returned string
STDAPI AMGetWideString(LPCWSTR pszString, LPWSTR *ppszReturn);


STDAPI CreatePosPassThru(LPUNKNOWN pAgg, BOOL bRenderer, IPin *pPin, IUnknown **ppPassThru);


//------------------------------------------------------------------------------
// File: MtType.h
//------------------------------------------------------------------------------

/* Helper class that derived pin objects can use to compare media
types etc. Has same data members as the struct AM_MEDIA_TYPE defined
in the streams IDL file, but also has (non-virtual) functions */

class CMediaType : public _AMMediaType
{
public:

	~CMediaType();
	CMediaType();

	CMediaType& operator=(const CMediaType&);
	CMediaType& operator=(const AM_MEDIA_TYPE&);

	BOOL operator == (const CMediaType&) const;
	BOOL operator != (const CMediaType&) const;

	HRESULT Set(const CMediaType& rt);
	HRESULT Set(const AM_MEDIA_TYPE& rt);

	BYTE*   Format() const { return pbFormat; };

	void InitMediaType();

	BOOL MatchesPartial(const CMediaType* ppartial) const;
	BOOL IsPartiallySpecified() const;
};


/* General purpose functions to copy and delete a task allocated AM_MEDIA_TYPE
structure which is useful when using the IEnumMediaFormats interface as
the implementation allocates the structures which you must later delete */

void WINAPI DeleteMediaType(AM_MEDIA_TYPE *pmt);
AM_MEDIA_TYPE * WINAPI CreateMediaType(AM_MEDIA_TYPE const *pSrc);
HRESULT WINAPI CopyMediaType(AM_MEDIA_TYPE *pmtTarget, const AM_MEDIA_TYPE *pmtSource);
void WINAPI FreeMediaType(AM_MEDIA_TYPE& mt);



//------------------------------------------------------------------------------
// File: schedule.h
//------------------------------------------------------------------------------

const LONGLONG MAX_TIME = 0x7FFFFFFFFFFFFFFF;   /* Maximum LONGLONG value */


class CAMSchedule
{
public:
	CAMSchedule();
	~CAMSchedule();
	REFERENCE_TIME lastAdviseTime;

	REFERENCE_TIME GetNextAdviseTime();

	// We need a method for derived classes to add advise packets, we return the cookie
	DWORD_PTR AddAdvisePacket(const REFERENCE_TIME & time1, const REFERENCE_TIME & time2, HANDLE h, BOOL periodic);
	// And a way to cancel
	HRESULT Unadvise(DWORD_PTR dwAdviseCookie);

	// Tell us the time please, and we'll dispatch the expired events.  We return the time of the next event.
	// NB: The time returned will be "useless" if you start adding extra Advises.  But that's the problem of
	// whoever is using this helper class (typically a clock).
	REFERENCE_TIME Advise(const REFERENCE_TIME & rtTime);

	// Get the event handle which will be set if advise time requires re-evaluation.
	HANDLE GetEvent() const { return m_ev; }

private:
	// We define the nodes that will be used in our singly linked list
	// of advise packets.  The list is ordered by time, with the
	// elements that will expire first at the front.
	class CAdvisePacket
	{
	public:
		CAdvisePacket(){}

		CAdvisePacket * m_next;
		DWORD_PTR       m_dwAdviseCookie;
		REFERENCE_TIME  m_rtEventTime;      // Time at which event should be set
		REFERENCE_TIME  m_rtPeriod;         // Periodic time
		HANDLE          m_hNotify;          // Handle to event or semephore
		BOOL            m_bPeriodic;        // TRUE => Periodic event

		CAdvisePacket(CAdvisePacket * next, LONGLONG time) : m_next(next), m_rtEventTime(time){}

		void InsertAfter(CAdvisePacket * p)
		{
			p->m_next = m_next;
			m_next    = p;
		}

		int IsZ() const // That is, is it the node that represents the end of the list
		{
			return m_next == 0;
		}

		CAdvisePacket * RemoveNext()
		{
			CAdvisePacket *next = m_next;
			m_next = next->m_next;
			return next;
		}

		void DeleteNext()
		{
			delete RemoveNext();
		}

		CAdvisePacket * Next() const
		{
			CAdvisePacket * result = m_next;
			if(result->IsZ()) result = 0;
			return result;
		}

		DWORD_PTR Cookie() const{ return m_dwAdviseCookie; }
	};

	// Structure is:
	// head -> elmt1 -> elmt2 -> z -> null
	// So an empty list is:       head -> z -> null
	// Having head & z as links makes insertaion,
	// deletion and shunting much easier.
	CAdvisePacket   head, z;            // z is both a tail and a sentry

	volatile DWORD_PTR  m_dwNextCookie;     // Strictly increasing
	CCritSec m_Serialize;

	// AddAdvisePacket: adds the packet, returns the cookie (0 if failed)
	DWORD_PTR AddAdvisePacket(CAdvisePacket * pPacket);
	// Event that we should set if the packed added above will be the next to fire.
	const HANDLE m_ev;

	// A Shunt is where we have changed the first element in the
	// list and want it re-evaluating (i.e. repositioned) in
	// the list.
	void ShuntHead();

	// Rather than delete advise packets, we cache them for future use
	CAdvisePacket * m_pAdviseCache;

	void Delete(CAdvisePacket * pLink);// This "Delete" will cache the Link
};



//------------------------------------------------------------------------------
// File: AMFilter.h
//------------------------------------------------------------------------------


/* The following classes are declared in this header: */

class VisualizationFilter;    // IBaseFilter,IMediaFilter support
class CBasePin;               // Abstract base class for IPin interface
class CEnumPins;              // Enumerate input and output pins
class VisualizationOutputPin; // Adds data provider member functions
class VisualizationInputPin;  // Implements IMemInputPin interface


//=====================================================================
//=====================================================================
// Defines CBasePin
//
// Abstract class that supports the basics of IPin
//=====================================================================
//=====================================================================

class  AM_NOVTABLE CBasePin : public MyUnknown, public IPin, public IQualityControl
{
public:

	IPin            *m_Connected;               // Pin we have connected to
	WCHAR *         m_pName;		              // This pin's name
	PIN_DIRECTION   m_dir;                      // Direction of this pin
	CCritSec        *m_pLock;                   // pointer to the filter lock
	VisualizationFilter *m_pFilter;             // Filter we were created by
	IQualityControl *m_pQSink;                  // Target for Quality messages
	CMediaType      m_mt;                       // Media type of connection

	// used to agree a media type for a pin connection
	// given a specific media type, attempt a connection (includes
	// checking that the type is acceptable to this pin)
	HRESULT AttemptConnection(
		IPin* pReceivePin,      // connect to this pin
		const CMediaType* pmt,  // using this type
		bool receive            // call ReceiveConnection 
		);

	// try all the media types in this enumerator - for each that
	// we accept, try to connect using ReceiveConnection.
	HRESULT TryMediaTypes(
		IPin *pReceivePin,       // connect to this pin
		const CMediaType *pmt,   // proposed type from Connect
		IEnumMediaTypes *pEnum); // try this enumerator

	CBasePin(
		VisualizationFilter *pFilter,// Owning filter who knows about pins
		PIN_DIRECTION dir);          // Either PINDIR_INPUT or PINDIR_OUTPUT

	DECLARE_IUNKNOWN

	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
	STDMETHODIMP_(ULONG) NonDelegatingRelease();
	STDMETHODIMP_(ULONG) NonDelegatingAddRef();

	// --- IPin methods ---

	// take lead role in establishing a connection. Media type pointer
	// may be null, or may point to partially-specified mediatype
	// (subtype or format type may be GUID_NULL).
	STDMETHODIMP Connect(
		IPin * pReceivePin,
		const AM_MEDIA_TYPE *pmt   // optional media type
		);

	// (passive) accept a connection from another pin
	STDMETHODIMP ReceiveConnection(
		IPin * pConnector,      // this is the initiating connecting pin
		const AM_MEDIA_TYPE *pmt   // this is the media type we will exchange
		);

	STDMETHODIMP Disconnect();

	STDMETHODIMP ConnectedTo(IPin **pPin);

	STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE *pmt);

	STDMETHODIMP QueryPinInfo(PIN_INFO * pInfo);

	STDMETHODIMP QueryDirection(PIN_DIRECTION * pPinDir);

	STDMETHODIMP QueryId(LPWSTR * Id);

	// does the pin support this media type
	STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE *pmt);

	// return an array of IPin* - the pins that this pin internally connects to
	// All pins put in the array must be AddReffed (but no others)
	// Errors: "Can't say" - FAIL, not enough slots - return S_FALSE
	// Default: return E_NOTIMPL
	// The filter graph will interpret NOT_IMPL as any input pin connects to
	// all visible output pins and vice versa.
	// apPin can be NULL if nPin==0 (not otherwise).
	STDMETHODIMP QueryInternalConnections(
		IPin* *apPin,     // array of IPin*
		ULONG *nPin       // on input, the number of slots
		// on output  the number of pins
		) {
		return E_NOTIMPL;
	}

	// NewSegment notifies of the start/stop/rate applying to the data
	// about to be received. Default implementation records data and
	// returns S_OK.
	// Override this to pass downstream.
	STDMETHODIMP NewSegment(REFERENCE_TIME tStart,
		REFERENCE_TIME tStop,
		double dRate);

	//================================================================================
	// IQualityControl methods
	//================================================================================

	STDMETHODIMP SetSink(IQualityControl * piqc);

	// --- helper methods ---

	// Returns true if the pin is connected. false otherwise.
	BOOL IsConnected() { return (m_Connected != NULL); };
	// Return the pin this is connected to (if any)
	IPin * GetConnected() { return m_Connected; };

	// Check if our filter is currently stopped
	BOOL IsStopped();

	virtual HRESULT CheckMediaType(const CMediaType* mtOut) PURE;

	// set the connection to use this format (previously agreed)
	HRESULT SetMediaType(const CMediaType *);

	// check that the connection is ok before verifying it
	// can be overridden eg to check what interfaces will be supported.
	virtual HRESULT CheckConnect(IPin *);

	// Set and release resources required for a connection
	virtual void BreakConnect() PURE;
	virtual HRESULT CompleteConnect(IPin *pReceivePin);

};


//=====================================================================
//=====================================================================
// Defines CEnumPins
//=====================================================================
//=====================================================================

class CEnumPins : public IEnumPins      // The interface we support
{
	int m_Position;                   // Current ordinal position
	VisualizationFilter *m_pFilter;   // The filter who owns us
	LONG m_cRef;

public:

	CEnumPins(VisualizationFilter *pFilter, int pos);
	~CEnumPins();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IEnumPins
	STDMETHODIMP Next(
		ULONG cPins,         // place this many pins...
		IPin ** ppPins,      // ...in this array of IPin*
		ULONG * pcFetched    // actual count passed returned here
		);

	STDMETHODIMP Skip(ULONG cPins);
	STDMETHODIMP Reset();
	STDMETHODIMP Clone(IEnumPins **ppEnum);
};


//=====================================================================
//=====================================================================
// Defines Output Pin
//
//=====================================================================
//=====================================================================

class VisualizationOutputPin : public CBasePin
{
public:

	IMemAllocator *m_pAllocator;
	IMemInputPin *m_pInputPin;        // interface on the downstreaminput pin
	// set up in CheckConnect when we connect.
	VisualizationInputPin *m_pInput;

	VisualizationOutputPin(VisualizationFilter *pFilter);
	~VisualizationOutputPin();

	// override CompleteConnect() so we can negotiate an allocator
	HRESULT CompleteConnect(IPin *pReceivePin);

	// negotiate the allocator and its buffer size/count and other properties
	// Calls DecideBufferSize to set properties
	HRESULT DecideAllocator(IMemInputPin * pPin, IMemAllocator ** pAlloc);

	// override this to set the buffer size and count. Return an error
	// if the size/count is not to your liking.
	// The allocator properties passed in are those requested by the
	// input pin - use eg the alignment and prefix members if you have
	// no preference on these.
	HRESULT DecideBufferSize(
		IMemAllocator * pAlloc,
		ALLOCATOR_PROPERTIES * ppropInputRequest);

	// override this to control the connection
	HRESULT CheckConnect(IPin *pPin);
	void BreakConnect();

	// we have a default handling of EndOfStream which is to return
	// an error, since this should be called on input pins only
	STDMETHODIMP EndOfStream();

	// same for Begin/EndFlush - we handle Begin/EndFlush since it
	// is an error on an output pin, and we have Deliver methods to
	// call the methods on the connected pin
	STDMETHODIMP BeginFlush();
	STDMETHODIMP EndFlush();

	// implement IMediaPosition by passing upstream
	IUnknown * m_pPosition;

	// override to expose IMediaPosition
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

	// check that we can support this output type
	HRESULT CheckMediaType(const CMediaType* mtOut);

	// inherited from IQualityControl via CBasePin
	STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

	// Media type
	CMediaType& CurrentMediaType() { return m_mt; };


	// Provide a media type enumerator.  Get it from upstream.
	STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum);

	// This just saves the allocator being used on the output pin
	// Also called by input pin's GetAllocator()
	void SetAllocator(IMemAllocator * pAllocator);

	IMemInputPin * ConnectedIMemInputPin() { return m_pInputPin; }

	// Allow the filter to see what allocator we have
	// This does NOT AddRef
	IMemAllocator * PeekAllocator() const { return m_pAllocator; }
};


//=====================================================================
//=====================================================================
// Defines Input Pin
//=====================================================================
//=====================================================================

class VisualizationInputPin : public CBasePin, public IMemInputPin
{
public:

	IMemAllocator *m_pAllocator;    // Default memory allocator

	// in flushing state (between BeginFlush and EndFlush)
	// if TRUE, all Receives are returned with S_FALSE
	BYTE m_bFlushing;

	bool m_bRunTimeError;            // Run time error generated

	// Sample properties - initalized in Receive
	AM_SAMPLE2_PROPERTIES m_SampleProps;

	VisualizationOutputPin *m_pOutput;


	VisualizationInputPin(VisualizationFilter *pFilter);
	~VisualizationInputPin();

	DECLARE_IUNKNOWN

	// override this to publicise our interfaces
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

	// return the allocator interface that this input pin
	// would like the output pin to use
	STDMETHODIMP GetAllocator(IMemAllocator ** ppAllocator);

	// tell the input pin which allocator the output pin is actually
	// going to use.
	STDMETHODIMP NotifyAllocator(
		IMemAllocator * pAllocator,
		BOOL bReadOnly);

	// do something with this media sample
	STDMETHODIMP Receive(IMediaSample *pSample);
	HRESULT Receive1(IMediaSample *pSample);

	// do something with these media samples
	STDMETHODIMP ReceiveMultiple(
		IMediaSample **pSamples,
		long nSamples,
		long *nSamplesProcessed);

	// See if Receive() blocks
	STDMETHODIMP ReceiveCanBlock();

	STDMETHODIMP BeginFlush();
	STDMETHODIMP EndFlush();

	// Release the pin's allocator.
	void BreakConnect();

	// helper method to see if we are flushing
	BOOL IsFlushing() { return m_bFlushing; };

	//  Override this for checking whether it's OK to process samples
	//  Also call this from EndOfStream.
	HRESULT CheckStreaming();


	//================================================================================
	// IQualityControl methods (from CBasePin)
	//================================================================================

	STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

	// Return sample properties pointer
	AM_SAMPLE2_PROPERTIES * SampleProps() {
		ASSERT(m_SampleProps.cbData != 0);
		return &m_SampleProps;
	}

protected:

	// Grab and release extra interfaces if required

	HRESULT CompleteConnect(IPin *pReceivePin);

	// check that we can support this output type
	HRESULT CheckMediaType(const CMediaType* mtIn);

	// --- IMemInputPin -----

	// provide EndOfStream that passes straight downstream
	// (there is no queued data)
	STDMETHODIMP EndOfStream();

	STDMETHODIMP NewSegment(
		REFERENCE_TIME tStart,
		REFERENCE_TIME tStop,
		double dRate);

	// Media type
public:
	CMediaType& CurrentMediaType() { return m_mt; };


	// --- IMemInputPin -----

	// Provide an enumerator for media types by getting one from downstream
	STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum);

	// Allow the filter to see what allocator we have
	// N.B. This does NOT AddRef
	IMemAllocator * PeekAllocator() const { return m_pAllocator; }

	// Pass this on downstream if it ever gets called.
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps);

};


//=====================================================================
//=====================================================================
// VisualizationFilter
//=====================================================================
//=====================================================================


class VisualizationFilter : public MyUnknown, public IBaseFilter
{
public:
	// critical section to stop state changes while processing a sample
	CCritSec  filterLock;
	// critical section Receive() and EndOfStream().
	// If you want to hold both locks then grab filterLock FIRST 
	CCritSec receiveLock;

	FILTER_STATE    m_State;      // current state: running, paused
	IReferenceClock *m_pClock;    // this graph's ref clock
	REFERENCE_TIME  m_tStart;     // offset from stream time to reference time
	IFilterGraph    *m_pGraph;    // Graph we belong to
	IMediaEventSink *m_pSink;     // Called with notify events

	VisualizationInputPin *m_pInput;
	VisualizationOutputPin *m_pOutput;
	int Npins;

	WAVEFORMATEXTENSIBLE format;  //audio format
	void *plugin;                 //visualization plugin
	REFERENCE_TIME EndSample;     //end time of the last sample
	int LastMediaSampleSize;      //size of the last MediaSample
	CCritSec visualPosLock;

	void draw();
	void allocBuffer(void *d);    //set plugin and call plugin->setFormat

	VisualizationFilter(HRESULT *phr);
	~VisualizationFilter();

	DECLARE_IUNKNOWN

	// override this to say what interfaces we support where
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
#ifdef DEBUG
	STDMETHODIMP_(ULONG) NonDelegatingRelease();
#endif

	virtual HRESULT CheckInputType(const CMediaType* mtIn);
	HRESULT CheckInputType1(const CMediaType* mtIn);
	HRESULT Transform(IMediaSample *pSample);
	HRESULT Transform(BYTE *pWave, REFERENCE_TIME tEnd);
	void transformWave(BYTE *pWave);

	// --- IPersist method ---

	STDMETHODIMP GetClassID(CLSID *pClsID);

	// --- IMediaFilter methods ---

	STDMETHODIMP GetState(DWORD dwMSecs, FILTER_STATE *State);
	STDMETHODIMP SetSyncSource(IReferenceClock *pClock);
	STDMETHODIMP GetSyncSource(IReferenceClock **pClock);

	STDMETHODIMP Stop();
	STDMETHODIMP Pause();

	// the start parameter is the difference to be added to the
	// sample's stream time to get the reference time for
	// its presentation
	STDMETHODIMP Run(REFERENCE_TIME tStart);

	// return the current stream time - ie find out what
	// stream time should be appearing now
	HRESULT StreamTime(REFERENCE_TIME &rtStream);

	// Is the filter currently active?
	BOOL IsActive() {
		filterLock.Lock();
		BOOL result=((m_State == State_Paused) || (m_State == State_Running));
		filterLock.Unlock();
		return result;
	};

	// Is this filter stopped (without locking)
	BOOL IsStopped() {
		return (m_State == State_Stopped);
	};

	// --- IBaseFilter methods ---

	STDMETHODIMP EnumPins(IEnumPins ** ppEnum);
	STDMETHODIMP QueryFilterInfo(FILTER_INFO * pInfo);
	STDMETHODIMP JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName);
	STDMETHODIMP QueryVendorInfo(LPWSTR* pVendorInfo);

	// --- helper methods ---

	// return the filter graph we belong to
	IFilterGraph *GetFilterGraph() { return m_pGraph; }

	// Request reconnect
	// pPin is the pin to reconnect
	// pmt is the type to reconnect with - can be NULL
	// Calls ReconnectEx on the filter graph
	HRESULT ReconnectPin(IPin *pPin, AM_MEDIA_TYPE const *pmt);

	STDMETHODIMP FindPin(LPCWSTR Id, IPin **ppPin);


	IMediaSample * Copy(IMediaSample *pSource);

	//  Are the input and output allocators different?
	BOOL UsingDifferentAllocators() const
	{
		return m_pInput->PeekAllocator() != m_pOutput->PeekAllocator();
	}
};

