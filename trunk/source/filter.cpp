/*
Most of this file are DirectShow base classes
Copyright (c) 1992-2002 Microsoft Corporation.  All rights reserved.
Some unused classes, methods and parameters were removed.
VisualizationFilter is mixed from CBaseFilter+CTransformFilter+CTransInPlaceFilter
*/
#include "hdr.h"
#include "sound.h"

extern int dsBufferSize;

#pragma warning( disable : 4355 )

//------------------------------------------------------------------------------
// File: WXUtil.cpp
//
// implements helper classes for building multimedia filters.
//------------------------------------------------------------------------------

// NOTE: as we need to use the same binaries on Win95 as on NT this code should
// be compiled WITHOUT unicode being defined.  Otherwise we will not pick up
// these internal routines and the binary will not run on Win95.

#ifndef UNICODE
// Windows 95 doesn't implement this, so we provide an implementation.
LPWSTR WINAPI lstrcpyWInternal(LPWSTR lpString1, LPCWSTR lpString2)
{
	LPWSTR lpReturn = lpString1;
	while((*lpString1++ = *lpString2++)!=0) ;
	return lpReturn;
}

int WINAPI lstrcmpWInternal(LPCWSTR lpString1, LPCWSTR lpString2)
{
	do {
		WCHAR c1 = *lpString1;
		WCHAR c2 = *lpString2;
		if(c1 != c2)  return (int) c1 - (int) c2;
	} while(*lpString1++ && *lpString2++);
	return 0;
}


int WINAPI lstrlenWInternal(LPCWSTR lpString)
{
	int i = -1;
	while(*(lpString+(++i))) ;
	return i;
}


#endif


// Return a wide string - allocating memory for it
// Returns:
//    S_OK          - no error
//    E_POINTER     - ppszReturn == NULL
//    E_OUTOFMEMORY - can't allocate memory for returned string
STDAPI AMGetWideString(LPCWSTR psz, LPWSTR *ppszReturn)
{
	CheckPointer(ppszReturn);
	DWORD nameLen = sizeof(WCHAR)* (lstrlenW(psz)+1);
	*ppszReturn = (LPWSTR)CoTaskMemAlloc(nameLen);
	if(*ppszReturn == NULL){
		return E_OUTOFMEMORY;
	}
	CopyMemory(*ppszReturn, psz, nameLen);
	return NOERROR;
}


//------------------------------------------------------------------------------
// File: MType.cpp
//
// implements a class that holds and manages media type information.
//------------------------------------------------------------------------------

// helper class that derived pin objects can use to compare media
// types etc. Has same data members as the struct AM_MEDIA_TYPE defined
// in the streams IDL file, but also has (non-virtual) functions

CMediaType::~CMediaType(){
	FreeMediaType(*this);
}


CMediaType::CMediaType()
{
	InitMediaType();
}


// this class inherits publicly from AM_MEDIA_TYPE so the compiler could generate
// the following assignment operator itself, however it could introduce some
// memory conflicts and leaks in the process because the structure contains
// a dynamically allocated block (pbFormat) which it will not copy correctly

CMediaType&
CMediaType::operator=(const AM_MEDIA_TYPE& rt)
{
	Set(rt);
	return *this;
}

CMediaType&
CMediaType::operator=(const CMediaType& rt)
{
	*this = (AM_MEDIA_TYPE &)rt;
	return *this;
}

BOOL
CMediaType::operator == (const CMediaType& rt) const
{
	// I don't believe we need to check sample size or
	// temporal compression flags, since I think these must
	// be represented in the type, subtype and format somehow. They
	// are pulled out as separate flags so that people who don't understand
	// the particular format representation can still see them, but
	// they should duplicate information in the format block.

	return ((IsEqualGUID(majortype, rt.majortype) == TRUE) &&
		(IsEqualGUID(subtype, rt.subtype) == TRUE) &&
		(IsEqualGUID(formattype, rt.formattype) == TRUE) &&
		(cbFormat == rt.cbFormat) &&
		((cbFormat == 0) ||
		(memcmp(pbFormat, rt.pbFormat, cbFormat) == 0)));
}


BOOL
CMediaType::operator != (const CMediaType& rt) const
{
	/* Check to see if they are equal */

	if(*this == rt){
		return FALSE;
	}
	return TRUE;
}


HRESULT
CMediaType::Set(const CMediaType& rt)
{
	return Set((AM_MEDIA_TYPE &)rt);
}


HRESULT
CMediaType::Set(const AM_MEDIA_TYPE& rt)
{
	if(&rt != this){
		FreeMediaType(*this);
		return CopyMediaType(this, &rt);
	}
	return S_OK;
}

// initialise a media type structure
void CMediaType::InitMediaType()
{
	ZeroMemory((PVOID)this, sizeof(*this));
	lSampleSize = 1;
	bFixedSizeSamples = TRUE;
}


// a partially specified media type can be passed to IPin::Connect
// as a constraint on the media type used in the connection.
// the type, subtype or format type can be null.
BOOL
CMediaType::IsPartiallySpecified() const
{
	if((majortype == GUID_NULL) ||
		(formattype == GUID_NULL)){
		return TRUE;
	}
	else{
		return FALSE;
	}
}

BOOL
CMediaType::MatchesPartial(const CMediaType* ppartial) const
{
	if((ppartial->majortype != GUID_NULL) &&
		(majortype != ppartial->majortype)){
		return FALSE;
	}
	if((ppartial->subtype != GUID_NULL) &&
		(subtype != ppartial->subtype)){
		return FALSE;
	}

	if(ppartial->formattype != GUID_NULL){
		// if the format block is specified then it must match exactly
		if(formattype != ppartial->formattype){
			return FALSE;
		}
		if(cbFormat != ppartial->cbFormat){
			return FALSE;
		}
		if((cbFormat != 0) &&
			(memcmp(pbFormat, ppartial->pbFormat, cbFormat) != 0)){
			return FALSE;
		}
	}

	return TRUE;

}


// general purpose function to delete a heap allocated AM_MEDIA_TYPE structure
// which is useful when calling IEnumMediaTypes::Next as the interface
// implementation allocates the structures which you must later delete
// the format block may also be a pointer to an interface to release

void WINAPI DeleteMediaType(AM_MEDIA_TYPE *pmt)
{
	if(pmt == NULL) return;
	FreeMediaType(*pmt);
	CoTaskMemFree((PVOID)pmt);
}


// this also comes in useful when using the IEnumMediaTypes interface so
// that you can copy a media type, you can do nearly the same by creating
// a CMediaType object but as soon as it goes out of scope the destructor
// will delete the memory it allocated (this takes a copy of the memory)

AM_MEDIA_TYPE * WINAPI CreateMediaType(AM_MEDIA_TYPE const *pSrc)
{
	ASSERT(pSrc);

	// Allocate a block of memory for the media type

	AM_MEDIA_TYPE *pMediaType =
		(AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));

	if(pMediaType == NULL){
		return NULL;
	}
	// Copy the variable length format block

	HRESULT hr = CopyMediaType(pMediaType, pSrc);
	if(FAILED(hr)){
		CoTaskMemFree((PVOID)pMediaType);
		return NULL;
	}

	return pMediaType;
}


//  Copy 1 media type to another

HRESULT WINAPI CopyMediaType(AM_MEDIA_TYPE *pmtTarget, const AM_MEDIA_TYPE *pmtSource)
{
	//  We'll leak if we copy onto one that already exists - there's one
	//  case we can check like that - copying to itself.
	ASSERT(pmtSource != pmtTarget);
	*pmtTarget = *pmtSource;
	if(pmtSource->cbFormat != 0){
		ASSERT(pmtSource->pbFormat != NULL);
		pmtTarget->pbFormat = (PBYTE)CoTaskMemAlloc(pmtSource->cbFormat);
		if(pmtTarget->pbFormat == NULL){
			pmtTarget->cbFormat = 0;
			return E_OUTOFMEMORY;
		}
		else{
			CopyMemory((PVOID)pmtTarget->pbFormat, (PVOID)pmtSource->pbFormat,
				pmtTarget->cbFormat);
		}
	}
	if(pmtTarget->pUnk != NULL){
		pmtTarget->pUnk->AddRef();
	}

	return S_OK;
}

//  Free an existing media type (ie free resources it holds)

void WINAPI FreeMediaType(AM_MEDIA_TYPE& mt)
{
	if(mt.cbFormat != 0){
		CoTaskMemFree((PVOID)mt.pbFormat);

		// Strictly unnecessary but tidier
		mt.cbFormat = 0;
		mt.pbFormat = NULL;
	}
	if(mt.pUnk != NULL){
		mt.pUnk->Release();
		mt.pUnk = NULL;
	}
}




//------------------------------------------------------------------------------
// File: ComBase.cpp
//
// implements class hierarchy for creating COM objects.
//------------------------------------------------------------------------------


MyUnknown::MyUnknown()
: m_cRef(0)
, m_pUnknown(reinterpret_cast<LPUNKNOWN>(static_cast<PNDUNKNOWN>(this)))
/* Set our pointer to our IUnknown interface.                      */
/* This pointer effectivly points to the owner of                  */
/* this object and can be accessed by the GetOwner() method.       */
/* Why the double cast?  Well, the inner cast is a type-safe cast */
/* to pointer to a type from which we inherit.  The second is     */
/* type-unsafe but works because INonDelegatingUnknown "behaves   */
/* like" IUnknown. (Only the names on the methods change.)        */
{
}



/* QueryInterface */

STDMETHODIMP MyUnknown::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
	CheckPointer(ppv);

	/* We know only about IUnknown */
	if(riid == IID_IUnknown){
		GetInterface((LPUNKNOWN)(PNDUNKNOWN) this, ppv);
		return NOERROR;
	}
	else{
		*ppv = NULL;
		return E_NOINTERFACE;
	}
}

/* We have to ensure that we DON'T use a max macro, since these will typically   */
/* lead to one of the parameters being evaluated twice.  Since we are worried    */
/* about concurrency, we can't afford to access the m_cRef twice since we can't  */
/* afford to run the risk that its value having changed between accesses.        */

template<class T> inline static T ourmax(const T & a, const T & b)
{
	return a > b ? a : b;
}

/* AddRef */

STDMETHODIMP_(ULONG) MyUnknown::NonDelegatingAddRef()
{
#ifdef DEBUG    
	LONG lRef =
#endif
		InterlockedIncrement(const_cast<LONG*>(&m_cRef));
	ASSERT(lRef > 0);
	return ourmax(ULONG(m_cRef), 1ul);
}


/* Release */

STDMETHODIMP_(ULONG) MyUnknown::NonDelegatingRelease()
{
	/* If the reference count drops to zero delete ourselves */

	LONG lRef = InterlockedDecrement(const_cast<LONG*>(&m_cRef));
	ASSERT(lRef >= 0);

	if(lRef == 0){

		// COM rules say we must protect against re-entrancy.
		// If we are an aggregator and we hold our own interfaces
		// on the aggregatee, the QI for these interfaces will
		// addref ourselves. So after doing the QI we must release
		// a ref count on ourselves. Then, before releasing the
		// private interface, we must addref ourselves. When we do
		// this from the destructor here it will result in the ref
		// count going to 1 and then back to 0 causing us to
		// re-enter the destructor. Hence we add an extra refcount here
		// once we know we will delete the object.
		// for an example aggregator see filgraph\distrib.cpp.

		m_cRef++;

		delete this;
		return ULONG(0);
	}
	else{
		return ourmax(ULONG(m_cRef), 1ul);
	}
}


/* Return an interface pointer to a requesting client
performing a thread safe AddRef as necessary */

STDAPI GetInterface(LPUNKNOWN pUnk, void **ppv)
{
	CheckPointer(ppv);
	*ppv = pUnk;
	pUnk->AddRef();
	return NOERROR;
}



//------------------------------------------------------------------------------
// File: AMFilter.cpp
//
// implements class hierarchy for streams architecture.
//------------------------------------------------------------------------------

//=====================================================================
//=====================================================================
//
// VisualizationFilter         Support for IBaseFilter (incl. IMediaFilter)
// CEnumPins                   Enumerate input and output pins
// CEnumMediaTypes             Enumerate the preferred pin formats
// CBasePin                    Abstract base class for IPin interface
//    VisualizationOutputPin   Adds data provider member functions
//    VisualizationInputPin    Implements IMemInputPin interface
//
//=====================================================================
//=====================================================================



//=====================================================================
// Helpers
//=====================================================================
STDAPI CreateMemoryAllocator(IMemAllocator **ppAllocator)
{
	return CoCreateInstance(CLSID_MemoryAllocator, 0,
		CLSCTX_INPROC_SERVER, IID_IMemAllocator, (void **)ppAllocator);
}

//  Put this one here rather than in ctlutil to avoid linking
//  anything brought in by ctlutil
STDAPI CreatePosPassThru(LPUNKNOWN pAgg,
												 BOOL bRenderer,
												 IPin *pPin,
												 IUnknown **ppPassThru)
{
	*ppPassThru = NULL;
	IUnknown *pUnkSeek;
	HRESULT hr = CoCreateInstance(CLSID_SeekingPassThru,
		pAgg,
		CLSCTX_INPROC_SERVER,
		IID_IUnknown,
		(void **)&pUnkSeek
		);
	if(FAILED(hr)) return hr;

	ISeekingPassThru *pPassThru;
	hr = pUnkSeek->QueryInterface(IID_ISeekingPassThru, (void**)&pPassThru);
	if(FAILED(hr)){
		pUnkSeek->Release();
		return hr;
	}
	hr = pPassThru->Init(bRenderer, pPin);
	pPassThru->Release();
	if(FAILED(hr)){
		pUnkSeek->Release();
		return hr;
	}
	*ppPassThru = pUnkSeek;
	return S_OK;
}



//=====================================================================
//=====================================================================
// VisualizationFilter
//=====================================================================
//=====================================================================

VisualizationFilter::VisualizationFilter(HRESULT *phr) :
m_State(State_Stopped),
m_pClock(NULL),
m_pGraph(NULL),
m_pSink(NULL),
m_pOutput(NULL),
plugin(NULL)
{
	*phr=E_OUTOFMEMORY;
	format.Format.nChannels=0;
	m_pInput = new VisualizationInputPin(this);
	if(m_pInput){
		m_pOutput = new VisualizationOutputPin(this);
		if(m_pOutput){
			m_pInput->m_pOutput = m_pOutput;
			m_pOutput->m_pInput = m_pInput;
			Npins=2;
			*phr=S_OK;
		}
	}
}


VisualizationFilter::~VisualizationFilter()
{
	delete m_pOutput;
	delete m_pInput;

	/* Release any clock we were using */
	if(m_pClock){
		m_pClock->Release();
	}
}


STDMETHODIMP VisualizationFilter::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	if(riid == IID_IBaseFilter){
		return GetInterface((IBaseFilter *) this, ppv);
	}
	else if(riid == IID_IMediaFilter){
		return GetInterface((IMediaFilter *) this, ppv);
	}
	else{
		return MyUnknown::NonDelegatingQueryInterface(riid, ppv);
	}
}

#ifdef DEBUG
STDMETHODIMP_(ULONG) VisualizationFilter::NonDelegatingRelease()
{
	ASSERT(!(m_cRef == 1 && m_pGraph));
	return MyUnknown::NonDelegatingRelease();
}
#endif


STDMETHODIMP
VisualizationFilter::GetClassID(CLSID *)
{
	return E_FAIL;
}


STDMETHODIMP
VisualizationFilter::GetState(DWORD dwMSecs, FILTER_STATE *State)
{
	UNREFERENCED_PARAMETER(dwMSecs);
	CheckPointer(State);
	*State = m_State;
	return S_OK;
}


/* Set the clock we will use for synchronisation (might be NULL) */
STDMETHODIMP
VisualizationFilter::SetSyncSource(IReferenceClock *pClock)
{
	filterLock.Lock();

	// Ensure the new one does not go away - even if the same as the old
	if(pClock) pClock->AddRef();

	// if we have a clock, release it
	if(m_pClock) m_pClock->Release();

	m_pClock = pClock;
	filterLock.Unlock();
	return NOERROR;
}


/* Return the clock we are using for synchronisation */
STDMETHODIMP
VisualizationFilter::GetSyncSource(IReferenceClock **pClock)
{
	CheckPointer(pClock);
	filterLock.Lock();

	// returning an interface... addref it...
	if(m_pClock) m_pClock->AddRef();
	*pClock = (IReferenceClock*)m_pClock;
	filterLock.Unlock();
	return NOERROR;
}


// The time parameter is the offset to be added to the samples'
// stream time to get the reference time at which they should be presented.
// You can either add these two and compare it against the reference clock,
// or you can call StreamTime and compare that against the sample timestamp
STDMETHODIMP
VisualizationFilter::Run(REFERENCE_TIME tStart)
{
	filterLock.Lock();

	// remember the stream time offset
	m_tStart = tStart;

	HRESULT hr=S_OK;
	if(m_State == State_Stopped){
		hr = Pause();
	}
	if(SUCCEEDED(hr)){
		m_State = State_Running;
	}
	filterLock.Unlock();
	return hr;
}


// return the current stream time - samples with start timestamps of this
// time or before should be rendered by now
HRESULT
VisualizationFilter::StreamTime(REFERENCE_TIME& rtStream)
{
	// Caller must lock for synchronization
	// We can't grab the filter lock because we want to be able to call
	// this from worker threads without deadlocking
	if(m_pClock == NULL) return VFW_E_NO_CLOCK;

	// get the current reference time
	HRESULT hr = m_pClock->GetTime(&rtStream);

	// subtract the stream offset to get stream time
	rtStream -= m_tStart;
	return hr;
}


/* Create an enumerator for the pins attached to this filter */
STDMETHODIMP
VisualizationFilter::EnumPins(IEnumPins **ppEnum)
{
	CheckPointer(ppEnum);

	*ppEnum = new CEnumPins(this, 0);
	return *ppEnum == NULL ? E_OUTOFMEMORY : NOERROR;
}


/* Provide the filter with a filter graph */
STDMETHODIMP
VisualizationFilter::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR)
{
	filterLock.Lock();

	// we NOT AddRef() on the graph (m_pGraph, m_pSink)
	m_pGraph = pGraph;
	if(pGraph){
		HRESULT hr= m_pGraph->QueryInterface(IID_IMediaEventSink, (void**)&m_pSink);
		if(FAILED(hr)){
			ASSERT(m_pSink == NULL);
		}
		else m_pSink->Release();
	}
	else{
		// if graph pointer is null, then we should
		// also release the IMediaEventSink on the same object - we don't
		// refcount it, so just set it to null
		m_pSink = NULL;
	}
	filterLock.Unlock();
	return NOERROR;
}


STDMETHODIMP
VisualizationFilter::QueryVendorInfo(LPWSTR* pVendorInfo)
{
	UNREFERENCED_PARAMETER(pVendorInfo);
	return E_NOTIMPL;
}


// pPin is the pin to reconnect
// pmt is the type to reconnect with - can be NULL
// Calls ReconnectEx on the filter graph
HRESULT
VisualizationFilter::ReconnectPin(IPin *pPin, AM_MEDIA_TYPE const *pmt)
{
	if(!m_pGraph) return VFW_E_NOT_IN_GRAPH;

	IFilterGraph2 *pGraph2;
	HRESULT hr = m_pGraph->QueryInterface(IID_IFilterGraph2, (void **)&pGraph2);
	if(SUCCEEDED(hr)){
		hr = pGraph2->ReconnectEx(pPin, pmt);
		pGraph2->Release();
		return hr;
	}
	else{
		return m_pGraph->Reconnect(pPin);
	}
}


STDMETHODIMP
VisualizationFilter::FindPin(LPCWSTR Id, IPin **ppPin)
{
	CheckPointer(ppPin);

	if(!lstrcmpW(Id, L"Input")){
		*ppPin = m_pInput;
	}
	else if(!lstrcmpW(Id, L"Output") && Npins>1){
		*ppPin = m_pOutput;
	}
	else{
		*ppPin = NULL;
		return VFW_E_NOT_FOUND;
	}
	(*ppPin)->AddRef();
	return NOERROR;
}


STDMETHODIMP
VisualizationFilter::Stop()
{
	filterLock.Lock();
	if(m_State != State_Stopped){
		// Succeed the Stop if we are not completely connected
		if(!m_pInput->IsConnected() || !m_pOutput->IsConnected()){
			m_State = State_Stopped;
		}
		else{
			// decommit the input pin before locking or we can deadlock
			m_pInput->m_bRunTimeError = FALSE;
			if(m_pInput->m_pAllocator) m_pInput->m_pAllocator->Decommit();
			else m_pInput->m_bFlushing = FALSE;

			receiveLock.Lock();
			if(m_pOutput->m_pAllocator) m_pOutput->m_pAllocator->Decommit();

			// complete the state transition
			m_State = State_Stopped;
			receiveLock.Unlock();
		}
	}
	filterLock.Unlock();
	return S_OK;
}


STDMETHODIMP
VisualizationFilter::Pause()
{
	filterLock.Lock();

	HRESULT hr=S_OK;
	if(m_State != State_Paused){
		if(m_pInput->IsConnected() && m_pOutput->IsConnected()){
			if(m_State == State_Stopped){
				// commint output allocator
				if(!m_pOutput->m_pAllocator){ hr=VFW_E_NO_ALLOCATOR; goto lend; }
				hr= m_pOutput->m_pAllocator->Commit();
				if(FAILED(hr)) goto lend;
			}
		}
		m_State = State_Paused;
	}
lend:
	filterLock.Unlock();
	return hr;
}


// return a pointer to an identical copy of pSample
IMediaSample *VisualizationFilter::Copy(IMediaSample *pSource)
{
	IMediaSample * pDest;

	HRESULT hr;
	REFERENCE_TIME tStart, tStop;
	const BOOL bTime = S_OK == pSource->GetTime(&tStart, &tStop);

	// this may block for an indeterminate amount of time
	hr = m_pOutput->PeekAllocator()->GetBuffer(&pDest,
		bTime ? &tStart : NULL,
		bTime ? &tStop : NULL,
		0);

	if(FAILED(hr)) return NULL;

	ASSERT(pDest);
	IMediaSample2 *pSample2;
	if(SUCCEEDED(pDest->QueryInterface(IID_IMediaSample2, (void **)&pSample2))){
		HRESULT hr = pSample2->SetProperties(
			FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, pbBuffer),
			(PBYTE)m_pInput->SampleProps());
		pSample2->Release();
		if(FAILED(hr)){
			pDest->Release();
			return NULL;
		}
	}
	else{
		if(bTime){
			pDest->SetTime(&tStart, &tStop);
		}

		if(S_OK == pSource->IsSyncPoint()){
			pDest->SetSyncPoint(TRUE);
		}
		if(S_OK == pSource->IsDiscontinuity()){
			pDest->SetDiscontinuity(TRUE);
		}
		if(S_OK == pSource->IsPreroll()){
			pDest->SetPreroll(TRUE);
		}

		// Copy the media type
		AM_MEDIA_TYPE *pMediaType;
		if(S_OK == pSource->GetMediaType(&pMediaType)){
			pDest->SetMediaType(pMediaType);
			DeleteMediaType(pMediaType);
		}
	}

	// Copy the sample media times
	REFERENCE_TIME TimeStart, TimeEnd;
	if(pSource->GetMediaTime(&TimeStart, &TimeEnd) == NOERROR){
		pDest->SetMediaTime(&TimeStart, &TimeEnd);
	}

	// Copy the actual data length and the actual data.
	const long lDataLength = pSource->GetActualDataLength();
	pDest->SetActualDataLength(lDataLength);

	BYTE *pSourceBuffer, *pDestBuffer;
#ifdef DEBUG
	long lSourceSize  =
#endif
		pSource->GetSize();
#ifdef DEBUG
	long lDestSize =
#endif
		pDest->GetSize();

	ASSERT(lDestSize >= lSourceSize && lDestSize >= lDataLength);

	pSource->GetPointer(&pSourceBuffer);
	pDest->GetPointer(&pDestBuffer);
	ASSERT(lDestSize == 0 || pSourceBuffer != NULL && pDestBuffer != NULL);

	CopyMemory((PVOID)pDestBuffer, (PVOID)pSourceBuffer, lDataLength);
	return pDest;
}



//=====================================================================
//=====================================================================
// CEnumPins
//=====================================================================
//=====================================================================

CEnumPins::CEnumPins(VisualizationFilter *pFilter, int pos) :
m_Position(pos),
m_pFilter(pFilter),
m_cRef(1)
{
	pFilter->AddRef();
}


CEnumPins::~CEnumPins()
{
	m_pFilter->Release();
}


STDMETHODIMP
CEnumPins::QueryInterface(REFIID riid, void **ppv)
{
	CheckPointer(ppv);

	if(riid == IID_IEnumPins || riid == IID_IUnknown){
		return GetInterface((IEnumPins *) this, ppv);
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
CEnumPins::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG)
CEnumPins::Release()
{
	ULONG cRef = InterlockedDecrement(&m_cRef);
	if(cRef == 0){
		delete this;
	}
	return cRef;
}

STDMETHODIMP
CEnumPins::Clone(IEnumPins **ppEnum)
{
	CheckPointer(ppEnum);
	*ppEnum = new CEnumPins(m_pFilter, m_Position);
	if(*ppEnum == NULL) return E_OUTOFMEMORY;
	return NOERROR;
}


STDMETHODIMP
CEnumPins::Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched)
{
	CheckPointer(ppPins);

	/* Calculate the number of available pins */
	int cRealPins = min(m_pFilter->Npins - m_Position, (int)cPins);
	if(pcFetched!=NULL){
		*pcFetched = cRealPins;
	}
	else if(cPins>1){
		return E_INVALIDARG;
	}
	if(cRealPins == 0){
		return S_FALSE;
	}

	while(cRealPins){
		CBasePin *pPin = m_Position ? (CBasePin*)m_pFilter->m_pOutput : (CBasePin*)m_pFilter->m_pInput;
		m_Position++;
		*ppPins++ = pPin;
		pPin->AddRef();
		cRealPins--;
	}
	return NOERROR;
}


STDMETHODIMP
CEnumPins::Skip(ULONG cPins)
{
	if(cPins > ULONG(m_pFilter->Npins - m_Position)) return S_FALSE;
	m_Position += cPins;
	return NOERROR;
}


STDMETHODIMP
CEnumPins::Reset()
{
	m_Position = 0;
	return S_OK;
}



//=====================================================================
//=====================================================================
// Base Pin
//=====================================================================
//=====================================================================

CBasePin::CBasePin(VisualizationFilter *pFilter,
									 PIN_DIRECTION dir) :
m_pFilter(pFilter),
m_Connected(NULL),
m_dir(dir),
m_pQSink(NULL)
{
	m_pName = (m_dir==PINDIR_INPUT) ? L"Input" : L"Output";
	m_pLock= &pFilter->filterLock;
}


STDMETHODIMP
CBasePin::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
	if(riid == IID_IPin){
		return GetInterface((IPin *) this, ppv);
	}
	else if(riid == IID_IQualityControl){
		return GetInterface((IQualityControl *) this, ppv);
	}
	else{
		return MyUnknown::NonDelegatingQueryInterface(riid, ppv);
	}
}


/* Override to increment the owning filter's reference count */
STDMETHODIMP_(ULONG)
CBasePin::NonDelegatingAddRef()
{
	return m_pFilter->AddRef();
}


/* Override to decrement the owning filter's reference count */
STDMETHODIMP_(ULONG)
CBasePin::NonDelegatingRelease()
{
	return m_pFilter->Release();
}


BOOL CBasePin::IsStopped()
{
	return (m_pFilter->m_State == State_Stopped);
}


/* Asked to connect to a pin. A pin is always attached to an owning filter
object so we always delegate our locking to that object. We first of all
retrieve a media type enumerator for the input pin and see if we accept
any of the formats that it would ideally like, failing that we retrieve
our enumerator and see if it will accept any of our preferred types */

STDMETHODIMP
CBasePin::Connect(IPin * pReceivePin, const AM_MEDIA_TYPE *mt)
{
	const CMediaType* pmt = (const CMediaType*)mt;
	CheckPointer(pReceivePin);
	m_pLock->Lock();
	HRESULT hrFailure = VFW_E_NO_ACCEPTABLE_TYPES;

	// Find a mutually agreeable media type -
	// Pass in the template media type. If this is partially specified,
	// each of the enumerated media types will need to be checked against it.
	// If it is non-null and fully specified, try to connect with this.
	if(pmt && !pmt->IsPartiallySpecified()){
		// we are only allowed to connect using a type that matches pmt
		hrFailure= AttemptConnection(pReceivePin, pmt, true);
	}
	else{
		for(int i = 0; i < 2; i++){
			HRESULT hr;
			IEnumMediaTypes *pEnumMediaTypes;
			if(i == 0){
				// Try the other pin's enumerator 
				hr = pReceivePin->EnumMediaTypes(&pEnumMediaTypes);
			}
			else{
				hr = EnumMediaTypes(&pEnumMediaTypes);
			}
			if(SUCCEEDED(hr)){
				hr = TryMediaTypes(pReceivePin, pmt, pEnumMediaTypes);
				pEnumMediaTypes->Release();
				if(SUCCEEDED(hr)){ hrFailure = NOERROR; break; }
				// try to remember specific error codes if there are any
				if(hr!=VFW_E_NO_ACCEPTABLE_TYPES) hrFailure = hr;
			}
		}
	}
	m_pLock->Unlock();
	return hrFailure;
}


// given a specific media type, attempt a connection (includes
// checking that the type is acceptable to this pin)
HRESULT
CBasePin::AttemptConnection(IPin* pReceivePin,      // connect to this pin
														const CMediaType* pmt,  // using this type
														bool receive)
{
	// The caller should hold the filter lock becasue this function
	// uses m_Connected.  The caller should also hold the filter lock
	// because this function calls SetMediaType(), IsStopped() and
	// CompleteConnect().

	/* See if we are already connected */
	if(m_Connected){
		return VFW_E_ALREADY_CONNECTED;
	}
	/* See if the filter is active */
	if(!IsStopped()){
		return VFW_E_NOT_STOPPED;
	}
	// Check that the connection is valid
	HRESULT hr = CheckConnect(pReceivePin);
	if(SUCCEEDED(hr)){
		/* Check we will accept this media type */
		hr = CheckMediaType(pmt);
		if(hr == NOERROR){
			/*  Make ourselves look connected otherwise ReceiveConnection
			may not be able to complete the connection
			*/
			m_Connected = pReceivePin;
			m_Connected->AddRef();
			hr = SetMediaType(pmt);
			if(SUCCEEDED(hr)){
				/* See if the other pin will accept this type */
				if(receive) hr = pReceivePin->ReceiveConnection((IPin *)this, pmt);
				if(SUCCEEDED(hr)){
					/* Complete the connection */
					hr = CompleteConnect(pReceivePin);
					if(SUCCEEDED(hr)) return hr;
					if(receive) pReceivePin->Disconnect();
				}
			}
		}
		else{ // we cannot use this media type
			// return a specific media type error if there is one
			// S_FALSE gets changed to an error code
			if(SUCCEEDED(hr) || hr == E_FAIL || hr == E_INVALIDARG){
				hr = VFW_E_TYPE_NOT_ACCEPTED;
			}
		}
	}

	// release any connection here in case CheckMediaType
	// failed, or if we set anything up during a call back during
	// ReceiveConnection.
	BreakConnect();

	/*  If failed then undo our state */
	if(m_Connected){
		m_Connected->Release();
		m_Connected = NULL;
	}
	return hr;
}


HRESULT CBasePin::TryMediaTypes(IPin *pReceivePin,
																const CMediaType *pmt,
																IEnumMediaTypes *pEnum)
{
	/* Reset the current enumerator position */
	HRESULT hr = pEnum->Reset();
	if(FAILED(hr)) return hr;

	HRESULT hrFailure = VFW_E_NO_ACCEPTABLE_TYPES;

	for(;;){
		/* Retrieve the next media type NOTE each time round the loop the
		 enumerator interface will allocate another AM_MEDIA_TYPE structure
		 If we are successful then we copy it into our output object, if
		 not then we must delete the memory allocated before returning */

		CMediaType *pMediaType;
		hr = pEnum->Next(1, (AM_MEDIA_TYPE**)&pMediaType, NULL);
		if(hr!=S_OK) return hrFailure;

		// check that this matches the partial type (if any)
		if((pmt == NULL) || pMediaType->MatchesPartial(pmt)){

			hr = AttemptConnection(pReceivePin, pMediaType, true);

			// attempt to remember a specific error code
			if(FAILED(hr) &&
				hr != E_FAIL &&
				hr != E_INVALIDARG &&
				hr != VFW_E_TYPE_NOT_ACCEPTED){
				hrFailure = hr;
			}
		}
		else{
			hr = VFW_E_NO_ACCEPTABLE_TYPES;
		}
		DeleteMediaType(pMediaType);

		if(hr==S_OK) return hr;
	}
}


/* This is called to set the format for a pin connection - CheckMediaType
will have been called to check the connection format and if it didn't
return an error code then this (virtual) function will be invoked */
HRESULT
CBasePin::SetMediaType(const CMediaType *pmt)
{
	return m_mt.Set(*pmt);
}


/* This is called during Connect() to provide a virtual method that can do
any specific check needed for connection such as QueryInterface. This
base class method just checks that the pin directions don't match
*/
HRESULT
CBasePin::CheckConnect(IPin *pPin)
{
	/* Check that pin directions DONT match */

	PIN_DIRECTION pd;
	pPin->QueryDirection(&pd);

	ASSERT((pd == PINDIR_OUTPUT) || (pd == PINDIR_INPUT));
	ASSERT((m_dir == PINDIR_OUTPUT) || (m_dir == PINDIR_INPUT));

	// we should allow for non-input and non-output connections?
	if(pd == m_dir){
		return VFW_E_INVALID_DIRECTION;
	}
	return NOERROR;
}


// Called by an output pin on an input pin to establish a connection.
STDMETHODIMP
CBasePin::ReceiveConnection(IPin * pConnector,      // this is the pin who we will connect to
														const AM_MEDIA_TYPE *pmt)    // this is the media type we will exchange
{
	CheckPointer(pConnector);
	CheckPointer(pmt);
	m_pLock->Lock();
	HRESULT hr= AttemptConnection(pConnector, (const CMediaType*)pmt, false);
	m_pLock->Unlock();
	return hr;
}


/* Called when we want to terminate a pin connection */
STDMETHODIMP
CBasePin::Disconnect()
{
	m_pLock->Lock();

	HRESULT hr;
	/* See if the filter is active */
	if(!IsStopped()){
		hr=VFW_E_NOT_STOPPED;
	}
	else if(m_Connected){
		BreakConnect();
		m_Connected->Release();
		m_Connected = NULL;
		hr=S_OK;
	}
	else{
		// no connection - not an error
		hr=S_FALSE;
	}
	m_pLock->Unlock();
	return hr;
}


/* Return an AddRef()'d pointer to the connected pin if there is one */
STDMETHODIMP
CBasePin::ConnectedTo(IPin **ppPin)
{
	CheckPointer(ppPin);
	//
	//  It's pointless to lock here.
	//  The caller should ensure integrity.
	//
	IPin *pPin = m_Connected;
	*ppPin = pPin;
	if(pPin != NULL){
		pPin->AddRef();
		return S_OK;
	}
	else{
		ASSERT(*ppPin == NULL);
		return VFW_E_NOT_CONNECTED;
	}
}

/* Return the media type of the connection */
STDMETHODIMP
CBasePin::ConnectionMediaType(AM_MEDIA_TYPE *pmt)
{
	CheckPointer(pmt);
	m_pLock->Lock();
	HRESULT hr;
	/*  Copy constructor of m_mt allocates the memory */
	if(IsConnected()){
		CopyMediaType(pmt, &m_mt);
		hr=S_OK;
	}
	else{
		((CMediaType *)pmt)->InitMediaType();
		hr=VFW_E_NOT_CONNECTED;
	}
	m_pLock->Unlock();
	return hr;
}


/* Return information about the pin */
STDMETHODIMP
CBasePin::QueryPinInfo(PIN_INFO *pInfo)
{
	CheckPointer(pInfo);

	pInfo->pFilter = m_pFilter;
	m_pFilter->AddRef();

	lstrcpyW(pInfo->achName, m_pName);
	pInfo->dir = m_dir;
	return NOERROR;
}


STDMETHODIMP
CBasePin::QueryDirection(PIN_DIRECTION * pPinDir)
{
	CheckPointer(pPinDir);
	*pPinDir = m_dir;
	return NOERROR;
}


// return the pin's name
STDMETHODIMP
CBasePin::QueryId(LPWSTR * Id)
{
	return AMGetWideString(m_pName, Id);
}


/* Does this pin support this media type WARNING this interface function does
not lock the main object as it is meant to be asynchronous by nature - if
the media types you support depend on some internal state that is updated
dynamically then you will need to implement locking in a derived class
*/
STDMETHODIMP
CBasePin::QueryAccept(const AM_MEDIA_TYPE *pmt)
{
	CheckPointer(pmt);

	/* The CheckMediaType method is valid to return error codes if the media
	type is horrible, an example might be E_INVALIDARG. What we do here
	is map all the error codes into either S_OK or S_FALSE regardless */

	HRESULT hr = CheckMediaType((CMediaType*)pmt);
	if(FAILED(hr)){
		return S_FALSE;
	}
	// note that the only defined success codes should be S_OK and S_FALSE...
	return hr;
}


STDMETHODIMP
CBasePin::SetSink(IQualityControl * piqc)
{
	m_pLock->Lock();
	m_pQSink = piqc;
	m_pLock->Unlock();
	return NOERROR;
}


STDMETHODIMP
CBasePin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
	return S_OK;
}


//=====================================================================
//=====================================================================
// Output Pin
//=====================================================================
//=====================================================================

VisualizationOutputPin::VisualizationOutputPin(VisualizationFilter *pFilter) :
CBasePin(pFilter, PINDIR_OUTPUT),
m_pAllocator(NULL),
m_pInputPin(NULL),
m_pPosition(NULL)
{
}

VisualizationOutputPin::~VisualizationOutputPin()
{
	if(m_pPosition) m_pPosition->Release();
}


// overriden to expose IMediaPosition and IMediaSeeking control interfaces
STDMETHODIMP
VisualizationOutputPin::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	CheckPointer(ppv);
	*ppv = NULL;

	if(riid == IID_IMediaPosition || riid == IID_IMediaSeeking){
		if(m_pPosition == NULL){
			HRESULT hr = CreatePosPassThru(GetOwner(), FALSE, (IPin *)m_pInput, &m_pPosition);
			if(FAILED(hr)) return hr;
		}
		return m_pPosition->QueryInterface(riid, ppv);
	}
	else{
		return CBasePin::NonDelegatingQueryInterface(riid, ppv);
	}
}


// EnumMediaTypes - pass through to upstream filter
STDMETHODIMP
VisualizationOutputPin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
	// Can only pass through if connected.
	if(!m_pInput->IsConnected())
		return VFW_E_NOT_CONNECTED;

	return m_pInput->GetConnected()->EnumMediaTypes(ppEnum);
}


// CheckMediaType
// agree to anything if not connected,
// otherwise pass through to the upstream filter.
HRESULT VisualizationOutputPin::CheckMediaType(const CMediaType *pmt)
{
	// Don't accept any output pin type changes if we're copying
	// between allocators - it's too late to change the input
	// allocator size.
	if(m_pFilter->UsingDifferentAllocators() && !IsStopped()){
		if(*pmt == m_mt){
			return S_OK;
		}
		else{
			return VFW_E_TYPE_NOT_ACCEPTED;
		}
	}

	// Assumes the type does not change.  That's why we're calling
	// CheckINPUTType here on the OUTPUT pin.
	HRESULT hr = m_pFilter->CheckInputType(pmt);
	if(hr!=S_OK) return hr;

	if(m_pInput->IsConnected())
		return m_pInput->GetConnected()->QueryAccept(pmt);
	else
		return S_OK;
}


// Pass on the Quality notification to
// our QualityControl sink (if we have one) or to upstream filter
// and if that doesn't work, throw it away with a bad return code
STDMETHODIMP
VisualizationOutputPin::Notify(IBaseFilter * pSender, Quality q)
{
	UNREFERENCED_PARAMETER(pSender);

	if(m_pInput->m_pQSink){
		return m_pInput->m_pQSink->Notify(m_pFilter, q);
	}
	else{
		// no sink set, so pass it upstream
		HRESULT hr = VFW_E_NOT_FOUND;
		if(m_pInput->m_Connected){
			IQualityControl * pIQC;
			m_pInput->m_Connected->QueryInterface(IID_IQualityControl, (void**)&pIQC);
			if(pIQC){
				hr = pIQC->Notify(m_pFilter, q);
				pIQC->Release();
			}
		}
		return hr;
	}
}


/* This is called after a media type has been proposed
	 Try to complete the connection by agreeing the allocator
	 */
HRESULT
VisualizationOutputPin::CompleteConnect(IPin *pReceivePin)
{
	UNREFERENCED_PARAMETER(pReceivePin);
	HRESULT hr = DecideAllocator(m_pInputPin, &m_pAllocator);
	if(FAILED(hr)) return hr;

	if(m_pInput->IsConnected()){
		return m_pFilter->ReconnectPin(m_pInput, &CurrentMediaType());
	}
	return NOERROR;
}


/* This method is called when the output pin is about to try and connect to
an input pin. It is at this point that you should try and grab any extra
interfaces that you need, in this case IMemInputPin. Because this is
only called if we are not currently connected we do NOT need to call
BreakConnect. This also makes it easier to derive classes from us as
BreakConnect is only called when we actually have to break a connection
(or a partly made connection) and not when we are checking a connection */
HRESULT
VisualizationOutputPin::CheckConnect(IPin * pPin)
{
	HRESULT hr = CBasePin::CheckConnect(pPin);
	if(FAILED(hr)) return hr;

	// get an input pin and an allocator interface
	return pPin->QueryInterface(IID_IMemInputPin, (void **)&m_pInputPin);
}


void VisualizationOutputPin::BreakConnect()
{
	/* Release any allocator we hold */
	if(m_pAllocator){
		// Always decommit the allocator because a downstream filter may or
		// may not decommit the connection's allocator.  A memory leak could
		// occur if the allocator is not decommited when a connection is broken.
		m_pAllocator->Decommit();
		m_pAllocator->Release();
		m_pAllocator = NULL;
	}

	/* Release any input pin interface we hold */
	if(m_pInputPin){
		m_pInputPin->Release();
		m_pInputPin = NULL;
	}
}


// Tell the output pin's allocator what size buffers we require.
// *pAlloc will be the allocator our output pin is using.
HRESULT
VisualizationOutputPin::DecideBufferSize(
	IMemAllocator * pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
	ALLOCATOR_PROPERTIES Request, Actual;
	HRESULT hr;

	// If we are connected upstream, get his views
	if(m_pInput->IsConnected()){
		// Get the input pin allocator, and get its size and count.
		// we don't care about his alignment and prefix.
		hr = m_pInput->PeekAllocator()->GetProperties(&Request);
		if(FAILED(hr)){
			// Input connected but with a secretive allocator - enough!
			return hr;
		}
	}
	else{
		// We're reduced to blind guessing.  Let's guess one byte and if
		// this isn't enough then when the other pin does get connected
		// we can revise it.
		ZeroMemory(&Request, sizeof(Request));
		Request.cBuffers = 1;
		Request.cbBuffer = 1;
	}

	// Pass the allocator requirements to our output side
	// but do a little sanity checking first or we'll just hit
	// asserts in the allocator.

	pProperties->cBuffers = Request.cBuffers;
	pProperties->cbBuffer = Request.cbBuffer;
	pProperties->cbAlign = Request.cbAlign;
	if(pProperties->cBuffers<=0){ pProperties->cBuffers = 1; }
	if(pProperties->cbBuffer<=0){ pProperties->cbBuffer = 1; }
	hr = pAllocator->SetProperties(pProperties, &Actual);

	if(FAILED(hr)) return hr;

	// Make sure we got the right alignment and at least the minimum required
	if((Request.cBuffers > Actual.cBuffers)
		|| (Request.cbBuffer > Actual.cbBuffer)
		|| (Request.cbAlign  > Actual.cbAlign)
		){
		return E_FAIL;
	}
	return NOERROR;
}



// Save the allocator pointer in the output pin
void
VisualizationOutputPin::SetAllocator(IMemAllocator * pAllocator)
{
	pAllocator->AddRef();
	if(m_pAllocator){
		m_pAllocator->Release();
	}
	m_pAllocator = pAllocator;
}


/* Decide on an allocator, override this if you want to use your own allocator
Override DecideBufferSize to call SetProperties. If the input pin fails
the GetAllocator call then this will construct a CMemAllocator and call
DecideBufferSize on that, and if that fails then we are completely hosed.
If the you succeed the DecideBufferSize call, we will notify the input
pin of the selected allocator. NOTE this is called during Connect() which
therefore looks after grabbing and locking the object's critical section */

// We query the input pin for its requested properties and pass this to
// DecideBufferSize to allow it to fulfill requests that it is happy
// with (eg most people don't care about alignment and are thus happy to
// use the downstream pin's alignment request).

HRESULT
VisualizationOutputPin::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)
{
	HRESULT hr = NOERROR;
	*ppAlloc = NULL;

	// get downstream prop request
	// the derived class may modify this in DecideBufferSize, but
	// we assume that he will consistently modify it the same way,
	// so we only get it once
	ALLOCATOR_PROPERTIES prop;
	ZeroMemory(&prop, sizeof(prop));

	// whatever he returns, we assume prop is either all zeros
	// or he has filled it out.
	pPin->GetAllocatorRequirements(&prop);

	// if he doesn't care about alignment, then set it to 1
	if(prop.cbAlign == 0){
		prop.cbAlign = 1;
	}

	/* Try the allocator provided by the input pin */

	hr = pPin->GetAllocator(ppAlloc);
	if(SUCCEEDED(hr)){

		hr = DecideBufferSize(*ppAlloc, &prop);
		if(SUCCEEDED(hr)){
			hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
			if(SUCCEEDED(hr)){
				return NOERROR;
			}
		}
	}

	/* If the GetAllocator failed we may not have an interface */
	if(*ppAlloc){
		(*ppAlloc)->Release();
		*ppAlloc = NULL;
	}

	hr = CreateMemoryAllocator(ppAlloc);
	if(SUCCEEDED(hr)){
		// note - the properties passed here are in the same
		// structure as above and may have been modified by
		// the previous call to DecideBufferSize
		hr = DecideBufferSize(*ppAlloc, &prop);
		if(SUCCEEDED(hr)){
			hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
			if(SUCCEEDED(hr)){
				return NOERROR;
			}
		}
	}

	/* Likewise we may not have an interface to release */
	if(*ppAlloc){
		(*ppAlloc)->Release();
		*ppAlloc = NULL;
	}
	return hr;
}


// we have a default handling of EndOfStream which is to return
// an error, since this should be called on input pins only
STDMETHODIMP
VisualizationOutputPin::EndOfStream()
{
	return E_UNEXPECTED;
}


// BeginFlush should be called on input pins only
STDMETHODIMP
VisualizationOutputPin::BeginFlush()
{
	return E_UNEXPECTED;
}

// EndFlush should be called on input pins only
STDMETHODIMP
VisualizationOutputPin::EndFlush()
{
	return E_UNEXPECTED;
}


//=====================================================================
//=====================================================================
// Input Pin
//=====================================================================
//=====================================================================

/* Constructor creates a default allocator object */

VisualizationInputPin::VisualizationInputPin(VisualizationFilter *pFilter) :
CBasePin(pFilter, PINDIR_INPUT),
m_pAllocator(NULL),
m_bFlushing(FALSE),
m_bRunTimeError(FALSE)
{
	ZeroMemory(&m_SampleProps, sizeof(m_SampleProps));
}

/* Destructor releases it's reference count on the default allocator */

VisualizationInputPin::~VisualizationInputPin()
{
	if(m_pAllocator != NULL){
		m_pAllocator->Release();
		m_pAllocator = NULL;
	}
}


// override this to publicise our interfaces
STDMETHODIMP
VisualizationInputPin::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	if(riid == IID_IMemInputPin){
		return GetInterface((IMemInputPin *) this, ppv);
	}
	else{
		return CBasePin::NonDelegatingQueryInterface(riid, ppv);
	}
}

// provide EndOfStream that passes straight downstream
// (there is no queued data)
STDMETHODIMP
VisualizationInputPin::EndOfStream()
{
	m_pFilter->receiveLock.Lock();
	HRESULT hr = CheckStreaming();
	if(hr==S_OK){
		if(!m_pOutput->m_Connected) hr=VFW_E_NOT_CONNECTED;
		else hr= m_pOutput->m_Connected->EndOfStream();
	}
	m_pFilter->receiveLock.Unlock();
	return hr;
}



// EnumMediaTypes
// pass through to our downstream filter
STDMETHODIMP
VisualizationInputPin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
	// Can only pass through if connected
	if(!m_pOutput->IsConnected())
		return VFW_E_NOT_CONNECTED;

	return m_pOutput->GetConnected()->EnumMediaTypes(ppEnum);
}


// CheckMediaType
// - agree to anything if not connected,
// otherwise pass through to the downstream filter.
// This assumes that the filter does not change the media type.

HRESULT VisualizationInputPin::CheckMediaType(const CMediaType* pmt)
{
	HRESULT hr = m_pFilter->CheckInputType(pmt);
	if(hr!=S_OK) return hr;

	if(m_pOutput->IsConnected())
		return m_pOutput->GetConnected()->QueryAccept(pmt);
	return S_OK;
}


// override to pass downstream
STDMETHODIMP
VisualizationInputPin::NewSegment(
															 REFERENCE_TIME tStart,
															 REFERENCE_TIME tStop,
															 double dRate)
{
	//  Save the values in the pin
	CBasePin::NewSegment(tStart, tStop, dRate);
	if(m_pFilter->Npins==1) return S_OK;
	IPin *con = m_pOutput->m_Connected;
	if(con == NULL){
		return VFW_E_NOT_CONNECTED;
	}
	return con->NewSegment(tStart, tStop, dRate);
}


/* Return the allocator interface that this input pin would like the output
pin to use. NOTE subsequent calls to GetAllocator should all return an
interface onto the SAME object so we create one object at the start

The allocator is Released on disconnect and replaced on NotifyAllocator
*/
STDMETHODIMP
VisualizationInputPin::GetAllocator(IMemAllocator **ppAllocator)
{
	CheckPointer(ppAllocator);
	m_pLock->Lock();
	HRESULT hr;

	if(m_pOutput->IsConnected()){
		//  Store the allocator we got
		hr = m_pOutput->ConnectedIMemInputPin()->GetAllocator(ppAllocator);
		if(SUCCEEDED(hr)){
			m_pOutput->SetAllocator(*ppAllocator);
		}
	}
	else{
		//  Help upstream filter (eg TIP filter which is having to do a copy)
		//  by providing a temp allocator here - we'll never use
		//  this allocator because when our output is connected we'll
		//  reconnect this pin
		if(m_pAllocator == NULL){
			hr = CreateMemoryAllocator(&m_pAllocator);
			if(FAILED(hr)) goto lend;
		}
		*ppAllocator = m_pAllocator;
		m_pAllocator->AddRef();
		hr=NOERROR;
	}
lend:
	m_pLock->Unlock();
	return hr;
}


/* Tell the input pin which allocator the output pin is actually going to use
NOTE the locking we do both here and also in
GetAllocator is unnecessary but derived classes that do something useful
will undoubtedly have to lock the object so this might help remind people */

STDMETHODIMP
VisualizationInputPin::NotifyAllocator(IMemAllocator * pAllocator, BOOL bReadOnly)
{
	HRESULT hr = S_OK;
	CheckPointer(pAllocator);

	m_pLock->Lock();

	//  If our output is not connected just accept the allocator
	//  We're never going to use this allocator because when our
	//  output pin is connected we'll reconnect this pin
	if(!m_pOutput->IsConnected()){
		IMemAllocator *pOldAllocator = m_pAllocator;
		pAllocator->AddRef();
		m_pAllocator = pAllocator;
		if(pOldAllocator) pOldAllocator->Release();
		hr=NOERROR;
	}
	else{
		hr = m_pOutput->ConnectedIMemInputPin()
			->NotifyAllocator(pAllocator, bReadOnly);
		if(SUCCEEDED(hr)){
			m_pOutput->SetAllocator(pAllocator);

			// It's possible that the old and the new are the same thing.
			// AddRef before release ensures that we don't unload it.
			pAllocator->AddRef();
			if(m_pAllocator) m_pAllocator->Release();
			m_pAllocator = pAllocator;
		}
	}
	m_pLock->Unlock();
	return hr;
}


void VisualizationInputPin::BreakConnect()
{
	if(m_pAllocator){
		// Always decommit the allocator because a downstream filter may or
		// may not decommit the connection's allocator.  A memory leak could
		// occur if the allocator is not decommited when a pin is disconnected.
		m_pAllocator->Decommit();
		m_pAllocator->Release();
		m_pAllocator = NULL;
	}
}


HRESULT
VisualizationInputPin::CompleteConnect(IPin *pReceivePin)
{
	// Reconnect output if necessary
	if(m_pOutput->IsConnected()){
		if(CurrentMediaType() != m_pOutput->CurrentMediaType()){
			return m_pFilter->ReconnectPin(m_pOutput, &CurrentMediaType());
		}
	}
	return NOERROR;
}


/* Do something with this media sample - this class checks to see if the
format has changed with this media sample and if so checks that the filter
will accept it, generating a run time error if not. Once we have raised a
run time error we set a flag so that no more samples will be accepted
*/
STDMETHODIMP
VisualizationInputPin::Receive(IMediaSample *pSample)
{
	m_pFilter->receiveLock.Lock();

	HRESULT hr = Receive1(pSample);
	if(hr==S_OK){

		IMemInputPin *pin = m_pOutput->m_pInputPin;
		if(!pin){
			hr=VFW_E_NOT_CONNECTED;
		}
		else{
			/*  Check for other streams and pass them on */
			AM_SAMPLE2_PROPERTIES * const pProps = SampleProps();
			if(pProps->dwStreamId != AM_STREAM_MEDIA){
				hr=pin->Receive(pSample);
			}
			else{
				if(m_pFilter->UsingDifferentAllocators()){
					// We have to copy the data.
					pSample = m_pFilter->Copy(pSample);
					if(pSample==NULL){ hr=E_UNEXPECTED; goto lend; }
				}
				// call the visualization plugin
				hr=m_pFilter->Transform(pSample);
				// send the sample to the audio renderer
				if(SUCCEEDED(hr)){
					hr = pin->Receive(pSample);
				}
				else{
					hr = S_OK;
				}
				// release the buffer. If the connected pin still needs it,
				// it will have addrefed it itself.
				if(m_pFilter->UsingDifferentAllocators()){
					pSample->Release();
				}
			}
		}
	}
lend:
	m_pFilter->receiveLock.Unlock();
	return hr;
}

HRESULT VisualizationInputPin::Receive1(IMediaSample *pSample)
{
	CheckPointer(pSample);

	HRESULT hr = CheckStreaming();
	if(hr!=S_OK) return hr;

	/* Check for IMediaSample2 */
	IMediaSample2 *pSample2;
	if(SUCCEEDED(pSample->QueryInterface(IID_IMediaSample2, (void **)&pSample2))){
		hr = pSample2->GetProperties(sizeof(m_SampleProps), (PBYTE)&m_SampleProps);
		pSample2->Release();
		if(FAILED(hr)) return hr;
	}
	else{
		/*  Get the properties the hard way */
		m_SampleProps.cbData = sizeof(m_SampleProps);
		m_SampleProps.dwTypeSpecificFlags = 0;
		m_SampleProps.dwStreamId = AM_STREAM_MEDIA;
		m_SampleProps.dwSampleFlags = 0;
		if(S_OK == pSample->IsDiscontinuity()){
			m_SampleProps.dwSampleFlags |= AM_SAMPLE_DATADISCONTINUITY;
		}
		if(S_OK == pSample->IsPreroll()){
			m_SampleProps.dwSampleFlags |= AM_SAMPLE_PREROLL;
		}
		if(S_OK == pSample->IsSyncPoint()){
			m_SampleProps.dwSampleFlags |= AM_SAMPLE_SPLICEPOINT;
		}
		if(SUCCEEDED(pSample->GetTime(&m_SampleProps.tStart,
			&m_SampleProps.tStop))){
			m_SampleProps.dwSampleFlags |= AM_SAMPLE_TIMEVALID |
				AM_SAMPLE_STOPVALID;
		}
		if(S_OK == pSample->GetMediaType(&m_SampleProps.pMediaType)){
			m_SampleProps.dwSampleFlags |= AM_SAMPLE_TYPECHANGED;
		}
		pSample->GetPointer(&m_SampleProps.pbBuffer);
		m_SampleProps.lActual = pSample->GetActualDataLength();
		m_SampleProps.cbBuffer = pSample->GetSize();
	}

	/* Has the format changed in this sample */

	if(!(m_SampleProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED)){
		return NOERROR;
	}

	/* Check the derived class accepts this format */
	/* This shouldn't fail as the source must call QueryAccept first */

	hr = CheckMediaType((CMediaType *)m_SampleProps.pMediaType);
	if(hr == NOERROR) return NOERROR;

	/* Raise a runtime error if we fail the media type */
	m_bRunTimeError = TRUE;
	EndOfStream();

	// Snapshot so we don't have to lock up
	IMediaEventSink *pSink = m_pFilter->m_pSink;
	if(pSink)  pSink->Notify(EC_ERRORABORT, VFW_E_TYPE_NOT_ACCEPTED, 0);

	return VFW_E_INVALIDMEDIATYPE;
}


/*  Receive multiple samples */
STDMETHODIMP
VisualizationInputPin::ReceiveMultiple(IMediaSample **pSamples,
																				long nSamples, long *nSamplesProcessed)
{
	CheckPointer(pSamples);

	*nSamplesProcessed = 0;
	while(nSamples-- > 0){
		HRESULT hr = Receive(pSamples[*nSamplesProcessed]);
		/*  S_FALSE means don't send any more */
		if(hr != S_OK) return hr;
		(*nSamplesProcessed)++;
	}
	return S_OK;
}


STDMETHODIMP
VisualizationInputPin::ReceiveCanBlock()
{
	IMemInputPin *pin = m_pOutput->m_pInputPin;
	if(!pin) return S_OK;
	return pin->ReceiveCanBlock();
}


// Default handling for BeginFlush - call at the beginning
// of your implementation (makes sure that all Receive calls
// fail). After calling this, you need to call downstream.
STDMETHODIMP
VisualizationInputPin::BeginFlush()
{
	// BeginFlush is NOT synchronized with streaming but is part of
	// a control action - hence we synchronize with the filter
	m_pLock->Lock();
	HRESULT hr;

	if(!IsConnected() || !m_pOutput->IsConnected()){
		hr=VFW_E_NOT_CONNECTED;
	}
	else{
		ASSERT(!m_bFlushing);
		m_bFlushing = TRUE;

		// now call downstream 
		hr= m_pOutput->m_Connected->BeginFlush();
	}
	m_pLock->Unlock();
	return hr;
}


// handling for EndFlush - call at end of your implementation
// clear the m_bFlushing flag to re-enable receives
STDMETHODIMP
VisualizationInputPin::EndFlush()
{
	// Endlush is NOT synchronized with streaming but is part of
	// a control action - hence we synchronize with the filter
	m_pLock->Lock();
	HRESULT hr=S_OK;

	IPin *connected= m_pOutput->m_Connected;
	if(!IsConnected() || !connected){
		hr=VFW_E_NOT_CONNECTED;
	}
	else{
		// now call downstream 
		hr = connected->EndFlush();
		if(!FAILED(hr)){
			// now re-enable Receives
			ASSERT(m_bFlushing);
			m_bFlushing = FALSE;
			m_bRunTimeError = FALSE;
		}
	}
	m_pLock->Unlock();
	return hr;
}


STDMETHODIMP
VisualizationInputPin::Notify(IBaseFilter *pSender, Quality q)
{
	UNREFERENCED_PARAMETER(q);
	DbgBreak("IQuality::Notify called on an input pin");
	return NOERROR;
}


// If upstream asks us what our requirements are, we will try to ask downstream
// if that doesn't work, we'll just take the defaults.
STDMETHODIMP
VisualizationInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
{
	IMemInputPin *pin = m_pOutput->m_pInputPin;
	if(!pin) return E_NOTIMPL;
	return pin->GetAllocatorRequirements(pProps);
}


//  Check if it's OK to process data
HRESULT
VisualizationInputPin::CheckStreaming()
{
	if(m_pFilter->Npins>1 && !m_pOutput->IsConnected()){
		return VFW_E_NOT_CONNECTED;
	}
	//  Shouldn't be able to get any data if we're not connected!
	ASSERT(IsConnected());

	//  we're flushing
	if(m_bFlushing){
		return S_FALSE;
	}
	//  Don't process stuff in Stopped state
	if(IsStopped()){
		return VFW_E_WRONG_STATE;
	}
	if(m_bRunTimeError){
		return VFW_E_RUNTIME_ERROR;
	}
	return S_OK;
}



//------------------------------------------------------------------------------
// File: WXDebug.cpp
//
// implements ActiveX system debugging facilities.    
//------------------------------------------------------------------------------

#ifdef DEBUG

#ifdef UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif 
#endif 

// The Win32 wsprintf() function writes a maximum of 1024 characters to it's output buffer.
// See the documentation for wsprintf()'s lpOut parameter for more information.
const INT iDEBUGINFO = 1024;

struct MsgBoxMsg
{
	HWND hwnd;
	TCHAR *szTitle;
	TCHAR *szMessage;
	DWORD dwFlags;
	INT iResult;
};


// create a thread to call MessageBox(). calling MessageBox() on
// random threads at bad times can confuse the host (eg IE).
DWORD WINAPI MsgBoxThread(
													LPVOID lpParameter   // thread data
													)
{
	MsgBoxMsg *pmsg = (MsgBoxMsg *)lpParameter;
	pmsg->iResult = MessageBox(
		pmsg->hwnd,
		pmsg->szTitle,
		pmsg->szMessage,
		pmsg->dwFlags);

	return 0;
}

INT MessageBoxOtherThread(HWND hwnd, TCHAR *szTitle, TCHAR *szMessage,
													DWORD dwFlags)
{
	MsgBoxMsg msg ={hwnd, szTitle, szMessage, dwFlags, 0};
	DWORD dwid;
	HANDLE hThread = CreateThread(
		0,                      // security
		0,                      // stack size
		MsgBoxThread,
		(void *)&msg,           // arg
		0,                      // flags
		&dwid);
	if(hThread)
	{
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		return msg.iResult;
	}

	// break into debugger on failure.
	return IDCANCEL;
}


/* Displays a message box at a break point */
void WINAPI DbgBreakPoint(const TCHAR *pCondition, const TCHAR *pFileName, INT iLine)
{
	TCHAR szInfo[iDEBUGINFO];

	wsprintf(szInfo, _T("%s \nAt line %d of %s\nContinue? (Cancel to debug)"),
		pCondition, iLine, pFileName);

	INT MsgId = MessageBoxOtherThread(NULL, szInfo, _T("Hard coded break point"),
		MB_SYSTEMMODAL |
		MB_ICONHAND |
		MB_YESNOCANCEL |
		MB_SETFOREGROUND);
	switch(MsgId){
		case IDNO:              /* Kill the application */
			FatalAppExit(FALSE, _T("Application terminated"));
			break;

		case IDCANCEL:          /* Break into the debugger */
			DebugBreak();
			break;

		case IDYES:             /* Ignore break point continue execution */
			break;
	}
}

#endif



//------------------------------------------------------------------------------
// File: Schedule.cpp
//------------------------------------------------------------------------------

CAMSchedule::CAMSchedule() :
head(&z, 0), z(0, MAX_TIME),
m_dwNextCookie(0),
m_pAdviseCache(0),
m_ev(CreateEvent(NULL, FALSE, FALSE, NULL))
{
	head.m_dwAdviseCookie = z.m_dwAdviseCookie = 0;
}

CAMSchedule::~CAMSchedule()
{
	// Delete cache
	CAdvisePacket * p = m_pAdviseCache;
	while(p){
		CAdvisePacket *const p_next = p->m_next;
		delete p;
		p = p_next;
	}
	// Better to be safe than sorry
	while(!head.m_next->IsZ()){
		head.DeleteNext();
	}

	CloseHandle(GetEvent());
}

REFERENCE_TIME CAMSchedule::GetNextAdviseTime()
{
	m_Serialize.Lock(); // Need to stop the linked list from changing
	REFERENCE_TIME result= head.m_next->m_rtEventTime;
	m_Serialize.Unlock();
	return result;
}

#ifdef DEBUG
int Nadvise1, Nadvise2, Nadvise3;
#endif

DWORD_PTR CAMSchedule::AddAdvisePacket(CAdvisePacket *pPacket)
{
	ASSERT(pPacket->m_rtEventTime >= 0 && pPacket->m_rtEventTime < MAX_TIME);

	CAdvisePacket * p_prev = &head;
	CAdvisePacket * p_n;

	const DWORD_PTR Result = pPacket->m_dwAdviseCookie = ++m_dwNextCookie;
	// This relies on the fact that z is a sentry with a maximal m_rtEventTime
	for(;; p_prev = p_n){
		p_n = p_prev->m_next;
		if(p_n->m_rtEventTime >= pPacket->m_rtEventTime) break;
	}
	p_prev->InsertAfter(pPacket);

	// If packet added at the head, then clock needs to re-evaluate wait time.
	if(p_prev == &head){
		REFERENCE_TIME rtNow = AudioFilter::GetPrivateTime();
		Advise(10000 + rtNow);
		if(head.m_next==pPacket){
			//wake up working thread
			SetEvent(m_ev);
#ifdef DEBUG
			Nadvise3++;
#endif
		}
	}

	return Result;
}


DWORD_PTR CAMSchedule::AddAdvisePacket(const REFERENCE_TIME &time1,
																			 const REFERENCE_TIME &time2,
																			 HANDLE h, BOOL periodic)
{
	// Since we use MAX_TIME as a sentry, we can't afford to
	// schedule a notification at MAX_TIME
	ASSERT(time1 < MAX_TIME);
	DWORD_PTR Result;
	CAdvisePacket * p;

	m_Serialize.Lock();

	if(time1 <= lastAdviseTime && !periodic){
		// packet is late => return immediately
		SetEvent(h);
		Result = ++m_dwNextCookie;
#ifdef DEBUG
		Nadvise1++;
#endif
	}
	else{
#ifdef DEBUG
		Nadvise2++;
#endif
		Result = 0;
		if(m_pAdviseCache){
			p = m_pAdviseCache;
			m_pAdviseCache = p->m_next;
		}
		else{
			p = new CAdvisePacket();
		}
		if(p){
			p->m_rtEventTime = time1; p->m_rtPeriod = time2;
			p->m_hNotify = h; p->m_bPeriodic = periodic;
			Result = AddAdvisePacket(p);
		}
	}

	m_Serialize.Unlock();

	return Result;
}

HRESULT CAMSchedule::Unadvise(DWORD_PTR dwAdviseCookie)
{
	HRESULT hr = S_FALSE;
	CAdvisePacket * p_prev = &head;
	CAdvisePacket * p_n;
	m_Serialize.Lock();
	while((p_n = p_prev->Next())!=0){ // The Next() method returns NULL when it hits z
		if(p_n->m_dwAdviseCookie == dwAdviseCookie){
			Delete(p_prev->RemoveNext());
			hr = S_OK;
			// Having found one cookie that matches, there should be no more
#ifdef DEBUG
			while((p_n = p_prev->Next())!=0){
				ASSERT(p_n->m_dwAdviseCookie != dwAdviseCookie);
				p_prev = p_n;
			}
#endif
			break;
		}
		p_prev = p_n;
	}
	m_Serialize.Unlock();
	return hr;
}

REFERENCE_TIME CAMSchedule::Advise(const REFERENCE_TIME & rtTime)
{
	REFERENCE_TIME  rtNextTime;
	CAdvisePacket * pAdvise;

	m_Serialize.Lock();
	lastAdviseTime=rtTime;

	//  Note - DON'T cache the difference, it might overflow 
	while(rtTime >= (rtNextTime = (pAdvise=head.m_next)->m_rtEventTime) &&
		!pAdvise->IsZ()){
		ASSERT(pAdvise->m_dwAdviseCookie); // If this is zero, its the head or the tail!!

		ASSERT(pAdvise->m_hNotify != INVALID_HANDLE_VALUE);

		if(pAdvise->m_bPeriodic){
			ReleaseSemaphore(pAdvise->m_hNotify, 1, NULL);
			pAdvise->m_rtEventTime += pAdvise->m_rtPeriod;
			ShuntHead();
		}
		else{
			EXECUTE_ASSERT(SetEvent(pAdvise->m_hNotify));
			Delete(head.RemoveNext());
		}

	}
	m_Serialize.Unlock();
	return rtNextTime;
}

void CAMSchedule::Delete(CAdvisePacket * pPacket)
{
	pPacket->m_next = m_pAdviseCache;
	m_pAdviseCache = pPacket;
}


// Takes the head of the list & repositions it
void CAMSchedule::ShuntHead()
{
	CAdvisePacket * p_prev = &head;
	CAdvisePacket * p_n;

	m_Serialize.Lock();
	CAdvisePacket *const pPacket = head.m_next;

	// This will catch both an empty list,
	// and if somehow a MAX_TIME time gets into the list
	// (which would also break this method).
	ASSERT(pPacket->m_rtEventTime < MAX_TIME);

	// This relies on the fact that z is a sentry with a maximal m_rtEventTime
	for(;; p_prev = p_n){
		p_n = p_prev->m_next;
		if(p_n->m_rtEventTime > pPacket->m_rtEventTime) break;
	}
	// If p_prev == pPacket then we're already in the right place
	if(p_prev != pPacket){
		head.m_next = pPacket->m_next;
		(p_prev->m_next = pPacket)->m_next = p_n;
	}
	m_Serialize.Unlock();
}


DWORD __stdcall AdviseThreadFunction(void* p)
{
	reinterpret_cast<AudioFilter*>(p)->AdviseThread();
	return 0;
}

void AudioFilter::initSchedule()
{
	scheduleLock.Lock();
	if(!m_hThread){
		m_bAbort= false;
		m_pSchedule = new CAMSchedule();
		m_pSchedule->lastAdviseTime= GetPrivateTime();
		DWORD ThreadID;
		m_hThread = CreateThread(NULL, 0, AdviseThreadFunction, this, 0, &ThreadID);
		if(m_hThread){
			SetThreadPriority(m_hThread, THREAD_PRIORITY_HIGHEST/*THREAD_PRIORITY_TIME_CRITICAL*/);
		}
		else{
			delete m_pSchedule;
		}
	}
	scheduleLock.Unlock();
}

/* Ask for an async notification that a time has elapsed */

HRESULT AudioFilter::AdviseTime(REFERENCE_TIME baseTime,
	REFERENCE_TIME streamTime, HEVENT hEvent, DWORD_PTR *pdwAdviseCookie)
{
	CheckPointer(pdwAdviseCookie);
	HRESULT hr;

	REFERENCE_TIME lRefTime = baseTime + streamTime;

	/*if( lRefTime <= 0 || lRefTime == MAX_TIME ){
		hr = E_INVALIDARG;
		}else*/
	///
	{
		initSchedule();
		hr=E_OUTOFMEMORY;
		if(m_hThread){
			*pdwAdviseCookie = m_pSchedule->AddAdvisePacket(lRefTime, 0, HANDLE(hEvent), FALSE);
			if(*pdwAdviseCookie) hr=NOERROR;
		}
	}
	return hr;
}


/* Ask for an asynchronous periodic notification that a time has elapsed */

STDMETHODIMP AudioFilter::AdvisePeriodic(REFERENCE_TIME StartTime,         // starting at this time
																				 REFERENCE_TIME PeriodTime,        // time between notifications
																				 HSEMAPHORE hSemaphore,           // advise via a semaphore
																				 DWORD_PTR *pdwAdviseCookie)          // where your cookie goes
{
	CheckPointer(pdwAdviseCookie);

	HRESULT hr=E_INVALIDARG;
	if(StartTime > 0 && PeriodTime > 0 && StartTime != MAX_TIME){
		initSchedule();
		hr=E_OUTOFMEMORY;
		if(m_hThread){
			*pdwAdviseCookie = m_pSchedule->AddAdvisePacket(StartTime, PeriodTime, HANDLE(hSemaphore), TRUE);
			if(*pdwAdviseCookie) hr=NOERROR;
		}
	}
	return hr;
}


STDMETHODIMP AudioFilter::Unadvise(DWORD_PTR dwAdviseCookie)
{
	CheckPointer(m_hThread);
	return m_pSchedule->Unadvise(dwAdviseCookie);
}


void AudioFilter::AdviseThread()
{
	DWORD dwWait = 300;

	// Set up the highest resolution timer we can manage
	TIMECAPS tc;
	UINT resolution=1;
	if(timeGetDevCaps(&tc, sizeof(tc))==TIMERR_NOERROR) resolution=tc.wPeriodMin;
	timeBeginPeriod(resolution);

	while(!m_bAbort){

		// Wait for an interesting event to happen
		WaitForSingleObject(m_pSchedule->GetEvent(), (dwWait>0) ? dwWait : 1);
		if(m_bAbort) break;

		// There are several reasons why we need to work from the internal
		// time, mainly to do with what happens when time goes backwards.
		// Mainly, it stop us looping madly if an event is just about to
		// expire when the clock goes backward (i.e. GetTime stop for a
		// while).
		const REFERENCE_TIME  rtNow = GetPrivateTime();

		double llWait = (m_pSchedule->Advise(10000 + rtNow) - rtNow)/rate/10000;

		// DON'T replace REFERENCE_TIME(0xffffffff) with 0xffffffff !!
		dwWait = (llWait >= REFERENCE_TIME(0xffffffff)) ? 0xffffffff : DWORD(llWait);

		// check buffer underrun
		int f=(10*AudioFilter::getFilled()/int(AudioFilter::bufFormat.Format.nAvgBytesPerSec/100+1))-10;
		if(f<50){
			if(playing) OnUnderRun();
			f=100;
		}
		else if(isUnderRun){
			if(f>dsBufferSize-70 ||
				f>200 && GetTickCount()-writeTime>300){
				isUnderRun=false;
				play();
			}
			else{
				f=50;
			}
		}
		if(dwWait>(unsigned)f) dwWait=f;
	}
	timeEndPeriod(resolution);
}
