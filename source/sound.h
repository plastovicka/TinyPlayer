/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#include "filter.h"


class CEnumMediaTypes : public IEnumMediaTypes
{
	int m_Position;
	LONG m_cRef;

public:
	CEnumMediaTypes(int pos);
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP Next(ULONG cTypes, AM_MEDIA_TYPE ** ppTypes, ULONG * pcFetched);
	STDMETHODIMP Skip(ULONG cTypes);
	STDMETHODIMP Reset();
	STDMETHODIMP Clone(IEnumMediaTypes **ppTypes);
};


class AudioFilter : public VisualizationFilter, public IBasicAudio,
	public IReferenceClock, public IAMFilterMiscFlags
{
public:
	static IDirectSound *object;
	static IDirectSoundBuffer *buffer;
	static IDirectSoundBuffer8 *buffer8;
	static IDirectSoundNotify *notify;
	static HANDLE notifyEvent, playEvent;
	static DWORD curPos, writePos, bufSize, flushTime, writeTime;
	static int cycleDif, underrun, locksDone;
	static LONGLONG offsetBuf;
	static bool playing, isUnderRun, useNotify, ending, waited, formatChanged;
	static WAVEFORMATEXTENSIBLE bufFormat;
	static CCritSec clockLock, scheduleLock;
	static double rate;

	static HRESULT init();
	static void shut();
	static void flush(int skip);
	static HRESULT skip(DWORD d);
	static HRESULT createBuffer(WAVEFORMATEX *fmt);
	static void setPrimaryFormat();
	static HRESULT setRate(double r);
	static void setNotify();
	static HRESULT restoreBuffer();
	static void initEqualizer(bool all);
	static void setEqualizer();
	static HRESULT play();
	static HRESULT stop();
	static HRESULT getCurPos();
	static int getFree();
	static HRESULT put(void *data, int len);
	static REFERENCE_TIME GetPrivateTime();
	static REFERENCE_TIME getWriteTime();
	static void waitForEnd();
	static int msToB(int ms);
	static int getFilled();
	static void OnUnderRun();
	static bool isPreBufDone(int f);

	DECLARE_IUNKNOWN
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

	AudioFilter(HRESULT *phr);
	~AudioFilter();
	HRESULT CheckInputType(const CMediaType* pmt);
	HRESULT receive(void *data, int len);

	//IMediaFilter
	STDMETHODIMP Run(REFERENCE_TIME tStart);
	STDMETHODIMP Pause();
	STDMETHODIMP Stop();

	//Reference clock members
	CAMSchedule *m_pSchedule;
	bool m_bAbort; // Flag used for thread shutdown
	HANDLE m_hThread; // Thread handle
	REFERENCE_TIME beginTime;
	bool ended;

	//IReferenceClock
	STDMETHODIMP GetTime(REFERENCE_TIME *pTime);
	STDMETHODIMP AdviseTime(REFERENCE_TIME rtBaseTime,
		REFERENCE_TIME rtStreamTime, HEVENT hEvent, DWORD_PTR *pdwAdviseCookie);
	STDMETHODIMP AdvisePeriodic(REFERENCE_TIME rtStartTime, REFERENCE_TIME rtPeriodTime,
		HSEMAPHORE hSemaphore, DWORD_PTR *pdwAdviseCookie);
	STDMETHODIMP Unadvise(DWORD_PTR dwAdviseCookie);
	void AdviseThread(); // Method in which the advise thread runs
	void initSchedule();
	void setBeginTime();

	//IBasicAudio
	STDMETHODIMP get_Balance(long *plBalance);
	STDMETHODIMP put_Balance(long plBalance);
	STDMETHODIMP get_Volume(long *plVolume);
	STDMETHODIMP put_Volume(long plVolume);

	//IDispatch
	STDMETHODIMP GetIDsOfNames(REFIID riid, OLECHAR **rgszNames, unsigned int cNames, LCID lcid, DISPID *rgDispId);
	STDMETHODIMP GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo **ppTInfo);
	STDMETHODIMP GetTypeInfoCount(unsigned int *pctinfo);
	STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, unsigned int *puArgErr);

	//IAMFilterMiscFlags
	STDMETHODIMP_(ULONG) GetMiscFlags(){ return AM_FILTER_MISC_FLAGS_IS_RENDERER; }

	//IMediaPosition by passing upstream
	IUnknown *m_pPosition;

};


class AudioInputPin : public VisualizationInputPin
{
public:
	AudioFilter *dsFilter;

	AudioInputPin(AudioFilter *pFilter);
	STDMETHODIMP Receive(IMediaSample *pSample);
	STDMETHODIMP ReceiveCanBlock();
	STDMETHODIMP BeginFlush();
	STDMETHODIMP EndFlush();
	STDMETHODIMP EndOfStream();
	STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum);
};


extern int noPlay, noStop;
