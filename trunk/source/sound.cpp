/*
	(C) 2005-2011  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "tinyplay.h"
#include "sound.h"
#include "winamp.h"

#define Mnotif 50

const int
dsMaxPrebuf=1500,
 dsMaxPrebufCD=7000,
 audioPropUpdate=200,
 eqBandwidth=12,
 eqDx=35,
 eqSliderX=5,
 eqSliderY=0,
 eqSliderH=101;

int
dsBufferSize=2500, //ms
 dsPreBuffer=500,
 dsSeekDelay=500,
 dsSleep=100,
 dsNnotif=3,
 dsUseNotif=0,
 dsEq=1,
 dsCheckUnderrun=1,
 noPlay, noStop;


TCHAR eqFreqStr[128];
int Nequalizer;
TeqParam eqParam[Mequalizer];
HWND equalizerWnd;
int eqVisible;

IDirectSound *AudioFilter::object;
DWORD AudioFilter::bufSize, AudioFilter::curPos, AudioFilter::writePos, AudioFilter::flushTime, AudioFilter::writeTime;
HANDLE AudioFilter::notifyEvent;
IDirectSoundBuffer *AudioFilter::buffer;
IDirectSoundBuffer8 *AudioFilter::buffer8;
LONGLONG AudioFilter::offsetBuf;
CCritSec AudioFilter::clockLock, AudioFilter::scheduleLock;
bool AudioFilter::playing, AudioFilter::useNotify, AudioFilter::isUnderRun, AudioFilter::ending, AudioFilter::formatChanged, AudioFilter::waited;
WAVEFORMATEXTENSIBLE AudioFilter::bufFormat;
double AudioFilter::rate;
int AudioFilter::cycleDif, AudioFilter::underrun, AudioFilter::locksDone;
HANDLE AudioFilter::playEvent;
extern bool g_bMenuOn;
//------------------------------------------------------------------
typedef HRESULT(WINAPI *TdsoundCreate)(GUID*, LPDIRECTSOUND*, IUnknown*);

HRESULT AudioFilter::init()
{
	HRESULT hr=DSERR_NODRIVER;
	static HMODULE lib;
	if(!lib) lib= LoadLibrary(_T("dsound.dll"));
	if(lib){
		TdsoundCreate directSoundCreate= (TdsoundCreate)GetProcAddress(lib, "DirectSoundCreate8");
		if(!directSoundCreate) directSoundCreate= (TdsoundCreate)GetProcAddress(lib, "DirectSoundCreate");
		if(directSoundCreate) hr= directSoundCreate(NULL, &object, NULL);
		if(hr==DS_OK){
			hr= object->SetCooperativeLevel(hWin, DSSCL_PRIORITY);
			if(hr==DS_OK){
				playEvent=CreateEvent(0, FALSE, FALSE, 0);
			}
			else{
				RELEASE(object);
			}
		}
	}
	return hr;
}

void AudioFilter::shut()
{
	SAFE_RELEASE(buffer);
	SAFE_RELEASE(buffer8);
	bufSize=0;
	SAFE_RELEASE(object);
	CloseHandle(playEvent);
	playEvent=0;
	CloseHandle(notifyEvent);
	notifyEvent=0;
}

int AudioFilter::msToB(int ms)
{
	return MulDiv(ms, bufFormat.Format.nSamplesPerSec, 1000)*bufFormat.Format.nBlockAlign;
}

HRESULT AudioFilter::createBuffer(WAVEFORMATEX *fmt)
{
	HRESULT hr;
	if(!object){
		hr=init();
		if(hr!=DS_OK) return hr;
	}
	DSBUFFERDESC dsbd;
	ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
	aminmax(dsBufferSize, 150, 600000);
	dsbd.dwBufferBytes= (dsBufferSize*fmt->nSamplesPerSec/1000)*fmt->nBlockAlign;
	if(buffer && bufSize==dsbd.dwBufferBytes && fmt->nChannels==bufFormat.Format.nChannels &&
		fmt->wBitsPerSample==bufFormat.Format.wBitsPerSample &&
		fmt->wFormatTag == bufFormat.Format.wFormatTag &&
		(fmt->wFormatTag != WAVE_FORMAT_EXTENSIBLE || ((WAVEFORMATEXTENSIBLE*)fmt)->Samples.wValidBitsPerSample == bufFormat.Samples.wValidBitsPerSample)
		) return DS_OK;
	waitForEnd();
	SAFE_RELEASE(buffer);
	SAFE_RELEASE(buffer8);

	dsbd.dwSize= sizeof(DSBUFFERDESC);
	DWORD flags = DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLFREQUENCY|(dsUseNotif ? DSBCAPS_CTRLPOSITIONNOTIFY : 0)|DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS;
	dsbd.dwFlags= DSBCAPS_CTRLFX | flags;
	dsbd.lpwfxFormat=fmt;
	if(!dsEq)
		hr= DSERR_UNINITIALIZED;
	else
		hr= object->CreateSoundBuffer(&dsbd, &buffer, NULL);
	if(hr!=DS_OK){
		//equalizer is not available in older versions of DirectX
		dsbd.dwFlags= flags;
		hr= object->CreateSoundBuffer(&dsbd, &buffer, NULL);
	}
	else{
		buffer->QueryInterface(IID_IDirectSoundBuffer8, (void**)&buffer8);
	}
	if(hr==DS_OK){
		bufSize= dsbd.dwBufferBytes;
		memcpy(&bufFormat, fmt, min(sizeof(WAVEFORMATEX)+fmt->cbSize, sizeof(bufFormat)));
		formatChanged=true;
		rate=1;
		playing=false;
		curPos=writePos=0;
		cycleDif=1;
		locksDone=0;
		underrun=0;
		offsetBuf=0;
		setPrimaryFormat();
		setNotify();
		initEqualizer(false);
	}
	return hr;
}

HRESULT AudioFilter::setRate(double r)
{
	if(!buffer) return DSERR_UNINITIALIZED;
	if(r==rate) return DS_OK;
	HRESULT hr= buffer->SetFrequency((DWORD)(r * bufFormat.Format.nSamplesPerSec));
	if(hr==DS_OK) rate=r;
	return hr;
}

void AudioFilter::setNotify()
{
	useNotify=false;
	if(!dsUseNotif) return;
	IDirectSoundNotify *notify;
	HRESULT hr= buffer->QueryInterface(IID_IDirectSoundNotify, (void**)&notify);
	if(hr==S_OK){
		if(!notifyEvent) notifyEvent= CreateEvent(0, FALSE, FALSE, 0);
		aminmax(dsNnotif, 2, Mnotif);
		DSBPOSITIONNOTIFY pn[Mnotif];
		for(int i=0; i<dsNnotif; i++){
			pn[i].dwOffset=bufSize/dsNnotif*(i+1)-1;
			pn[i].hEventNotify=notifyEvent;
		}
		hr= notify->SetNotificationPositions(dsNnotif, pn);
		if(hr==DS_OK && notifyEvent) useNotify=true;
		notify->Release();
	}
}

HRESULT AudioFilter::restoreBuffer()
{
	if(!buffer) return DSERR_UNINITIALIZED;
	DWORD s;
	HRESULT hr = buffer->GetStatus(&s);
	if(hr==DS_OK){
		hr=S_FALSE;
		if(s & DSBSTATUS_BUFFERLOST){
			for(int i=0; i<200; i++){
				hr=buffer->Restore();
				if(hr!=DSERR_BUFFERLOST) break;
				Sleep(50);
			}
		}
	}
	return hr;
}

void AudioFilter::setPrimaryFormat()
{
	LPDIRECTSOUNDBUFFER pDSBPrimary;
	DSBUFFERDESC dsbd;
	ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
	dsbd.dwSize= sizeof(DSBUFFERDESC);
	dsbd.dwFlags= DSBCAPS_PRIMARYBUFFER;
	HRESULT hr= object->CreateSoundBuffer(&dsbd, &pDSBPrimary, NULL);
	if(hr==DS_OK){
		hr= pDSBPrimary->SetFormat((WAVEFORMATEX*)&bufFormat);
		pDSBPrimary->Release();
	}
}

void AudioFilter::initEqualizer(bool all)
{
	int i, n;
	DSEFFECTDESC ded[Mequalizer];

	if(!buffer8) return;
	n=0;
	for(i=0; i<Nequalizer; i++){
		DSEFFECTDESC *d=&ded[i];
		d->dwSize=sizeof(DSEFFECTDESC);
		d->dwFlags=0;
		d->guidDSFXClass=GUID_DSFX_STANDARD_PARAMEQ;
		d->dwReserved1=d->dwReserved2=0;
		eqParam[i].index= (eqParam[i].value || all) ? n++ : -1;
	}

	bool b= playing && (pMC || cdAudioFilter || waAudioFilter);
	int cnt=0;
	if(b){
		if(pMC) pMC->Pause();
		else if(waAudioFilter) Winamp::pause();
		else cdAudioFilter->Pause();
	}
lagain:
	HRESULT hr= buffer8->SetFX(n, n ? ded : 0, 0);
	if(b){
		if(FAILED(hr) && ++cnt<20){
			Sleep(50);
			goto lagain;
		}
		if(pMC) pMC->Run();
		else if(waAudioFilter) Winamp::run();
		else cdAudioFilter->Run(0);
	}

	if(SUCCEEDED(hr)) setEqualizer();
	else RELEASE(buffer8);
}

void AudioFilter::setEqualizer()
{
	if(!buffer8) return;
	for(int i=0; i<Nequalizer; i++){
		if(eqParam[i].index>=0){
			IDirectSoundFXParamEq8 *pFX;
			HRESULT hr= buffer8->GetObjectInPath(GUID_DSFX_STANDARD_PARAMEQ,
				eqParam[i].index, IID_IDirectSoundFXParamEq8, (void**)&pFX);
			if(SUCCEEDED(hr)){
				DSFXParamEq p;
				p.fCenter=eqParam[i].freq;
				p.fGain=eqParam[i].value;
				p.fBandwidth=eqBandwidth;
				pFX->SetAllParameters(&p);
				pFX->Release();
			}
		}
	}
}


HRESULT AudioFilter::play()
{
	if(!buffer) return DSERR_UNINITIALIZED;
lstart:
	HRESULT hr= buffer->Play(0, 0, DSBPLAY_LOOPING);
	if(hr==DS_OK) playing=true;
	if(hr==DSERR_BUFFERLOST){
		hr=restoreBuffer();
		if(SUCCEEDED(hr)) goto lstart;
	}
	return hr;
}

HRESULT AudioFilter::stop()
{
	if(!buffer) return DSERR_UNINITIALIZED;
	HRESULT hr= buffer->Stop();
	playing=false;
	return hr;
}

HRESULT AudioFilter::getCurPos()
{
	if(!buffer) return DSERR_UNINITIALIZED;
	clockLock.Lock();
	DWORD p;
	HRESULT hr= buffer->GetCurrentPosition(&p, 0);
	if(hr==DS_OK){
		static DWORD pp, cnt; ///
		if(p==pp){
			if(playing && ++cnt>9){
				stop();
				play();
				cnt=0;
			}
		}
		else{
			cnt=0;
			pp=p;
		}
		if(p<curPos){
			offsetBuf+=bufSize;
			cycleDif++;
		}
		curPos=p;
	}
	clockLock.Unlock();
	return hr;
}

REFERENCE_TIME AudioFilter::GetPrivateTime()
{
	HRESULT hr= getCurPos();
	if(hr==DS_OK){
		clockLock.Lock();
		REFERENCE_TIME result= UNITS*(curPos+offsetBuf)/bufFormat.Format.nAvgBytesPerSec;
		clockLock.Unlock();
		return result;
	}
	return 0;
}

REFERENCE_TIME AudioFilter::getWriteTime()
{
	clockLock.Lock();
	REFERENCE_TIME result= UNITS*(offsetBuf+(writePos+(1-cycleDif)*bufSize))/bufFormat.Format.nAvgBytesPerSec;
	clockLock.Unlock();
	return result;
}

HRESULT AudioFilter::skip(DWORD d)
{
	if(!buffer) return DSERR_UNINITIALIZED;
	stop();
	clockLock.Lock();
	getCurPos();
	HRESULT hr=buffer->SetCurrentPosition((curPos+d)%bufSize);
	writePos+=d;
	if(writePos>=bufSize){
		writePos-=bufSize;
		cycleDif--;
	}
	clockLock.Unlock();
	return hr;
}


void AudioFilter::flush(int skip)
{
	getCurPos();
	clockLock.Lock();
	amax(skip, getFilled());
	amin(skip, 0);
	cycleDif=1;
	writePos=curPos+skip;
	if(writePos>=bufSize){
		writePos-=bufSize;
		cycleDif--;
	}
	flushTime=GetTickCount();
	clockLock.Unlock();
}

HRESULT AudioFilter::put(void *data, int len)
{
	if(!buffer) return DSERR_UNINITIALIZED;
	void *ptr1, *ptr2;
	DWORD len1, len2;
	HRESULT hr;

lstart:
	locksDone++;
	if(isUnderRun) writeTime=GetTickCount();
	hr= buffer->Lock(writePos, len, &ptr1, &len1, &ptr2, &len2, 0);
	if(hr!=DS_OK){
		if(hr==DSERR_BUFFERLOST){
			hr=restoreBuffer();
			if(SUCCEEDED(hr)) goto lstart;
		}
	}
	else{
		memcpy(ptr1, data, len1);
		if(len2) memcpy(ptr2, ((char*)data)+len1, len2);
		hr= buffer->Unlock(ptr1, len1, ptr2, len2);
		clockLock.Lock();
		writePos+=len;
		if(writePos>=bufSize){
			writePos-=bufSize;
			cycleDif--;
		}
		clockLock.Unlock();
		ASSERT(signed(writePos)>=0);
	}
	return hr;
}

int AudioFilter::getFree()
{
	clockLock.Lock();
	int result= int(curPos-writePos)+cycleDif*bufSize;
	clockLock.Unlock();
	return result;
}

int AudioFilter::getFilled()
{
	clockLock.Lock();
	int result= int(writePos-curPos)+(1-cycleDif)*bufSize;
	clockLock.Unlock();
	return result;
}

void AudioFilter::waitForEnd()
{
	if(!playing) return;
	if(!waited && ending){
		static char buf[1440];
		int Dnul, f;
		Dnul=0;
		memset(buf, (bufFormat.Format.wBitsPerSample==8) ? 128 : 0, sizeof(buf));
		while(Dnul<int(bufSize) && playing){
			getCurPos();
			f=getFree();
			if(f>sizeof(buf)){
				put(buf, sizeof(buf));
				Dnul+=sizeof(buf);
			}
			else{
				Sleep(50);
			}
		}
	}
	if(!noStop) stop();
	waited=true;
}

bool AudioFilter::isPreBufDone(int f)
{
	if(f==-1) f=getFree();
	int pb= dsPreBuffer;
	if(underrun) amin(pb, isVideo ? (isAvi ? 10000 : 1800) : 2500);
	//prebuffer can be set in options, but can't be less than 10 ms or 2048 bytes
	aminmax(pb, 10, dsBufferSize-70);
	pb=msToB(pb);
	amin(pb, 2048);
	return f <= int(bufSize-pb);
}

HRESULT AudioFilter::receive(void *data, int len)
{
	if(dsCheckUnderrun) initSchedule(); //start thread
	if(4*len > (int)bufSize){
		//split large packet
		int len1=((len>>1)/bufFormat.Format.nBlockAlign)*bufFormat.Format.nBlockAlign;
		HRESULT hr=receive(data, len1);
		if(FAILED(hr)) return hr;
		len-=len1;
		data=((char*)data)+len1;
	}
	if(!tick && isVideo){
		static int cnt;
		cnt+=len;
		if(cnt>176400){
			cnt=0;
			PostMessage(hWin, WM_TIMER, 1000, 0); //redraw position
		}
	}
	while(!m_pInput->m_bFlushing && m_State!=State_Stopped){  //exit loop when stop or seek
		for(int i=0;; i++, getCurPos()){
			int f=getFree();
			if(!playing && m_State==State_Running && isPreBufDone(f)){
				if(!i) continue; //get current position and calculate again
				//start playback when there are enough samples in the buffer
				SetEvent(playEvent);
			}
			if(f>len){
				//copy packet to audio buffer and return
				waited=false;
				return put(data, len);
			}
			if(i) break; //repeat only twice
		}
		if(!tick) PostMessage(hWin, WM_TIMER, 1000, 0); //redraw position

		//buffer is full => wait for a while
		if(useNotify) WaitForSingleObject(notifyEvent, INFINITE);
		else{
			amin(dsSleep, 1);
			for(int t=dsSleep; t>0 && !m_pInput->m_bFlushing && m_State!=State_Stopped; t-=100){
				Sleep(min(t, 100));
			}
		}
	}
	return DSERR_INVALIDCALL;
}



//=====================================================================
// AudioFilter
//=====================================================================

AudioFilter::AudioFilter(HRESULT *phr) :VisualizationFilter(phr), m_pPosition(0)
{
	Npins=1;
	if(*phr!=S_OK) return;
	delete m_pInput;
	m_pInput = new AudioInputPin(this);
	if(m_pInput){
		m_pOutput->m_pInput=m_pInput;
		m_pInput->m_pOutput=m_pOutput;
	}
	else{
		*phr=E_OUTOFMEMORY;
	}
	m_hThread=0;
	isUnderRun=false;
	beginTime=0;
	ended=false;
}

AudioFilter::~AudioFilter()
{
	SAFE_RELEASE(m_pPosition);

	//destroy advise time thread
	if(m_hThread){
		m_bAbort = true;
		SetEvent(m_pSchedule->GetEvent());
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
		delete m_pSchedule;
	}
}

HRESULT AudioFilter::CheckInputType(const CMediaType* pmt)
{
	HRESULT hr = CheckInputType1(pmt);
	if(SUCCEEDED(hr)){
		receiveLock.Lock();
		hr= createBuffer((WAVEFORMATEX *)pmt->Format());
		receiveLock.Unlock(); ///
		if(hr==DS_OK){
			hr= VisualizationFilter::CheckInputType(pmt);
		}
	}
	return hr;
}

STDMETHODIMP AudioFilter::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	if(riid == IID_IReferenceClock){
		return GetInterface((IReferenceClock *) this, ppv);
	}
	else if(riid == IID_IBasicAudio){
		return GetInterface((IBasicAudio *) this, ppv);
	}
	else if(riid == IID_IAMFilterMiscFlags){
		return GetInterface((IAMFilterMiscFlags *) this, ppv);
	}
	else if(riid == IID_IMediaPosition || riid == IID_IMediaSeeking){
		if(!m_pPosition){
			HRESULT hr = CreatePosPassThru(GetOwner(), TRUE, m_pInput, &m_pPosition);
			if(FAILED(hr)) return hr;
		}
		return m_pPosition->QueryInterface(riid, ppv);
	}
	else{
		return VisualizationFilter::NonDelegatingQueryInterface(riid, ppv);
	}
}


HRESULT AudioFilter::get_Volume(long *plVolume)
{
	CheckPointer(plVolume);
	if(!buffer) return VFW_E_WRONG_STATE;
	return buffer->GetVolume(plVolume);
}

HRESULT AudioFilter::put_Volume(long plVolume)
{
	if(!buffer) return VFW_E_WRONG_STATE;
	return buffer->SetVolume(plVolume);
}

HRESULT AudioFilter::get_Balance(long *plBalance)
{
	CheckPointer(plBalance);
	if(!buffer) return VFW_E_WRONG_STATE;
	return buffer->GetPan(plBalance);
}

HRESULT AudioFilter::put_Balance(long plBalance)
{
	if(!buffer) return VFW_E_WRONG_STATE;
	return buffer->SetPan(plBalance);
}

HRESULT AudioFilter::GetIDsOfNames(REFIID riid, OLECHAR **rgszNames, unsigned int cNames, LCID lcid, DISPID *rgDispId)
{
	return E_NOTIMPL;
}
HRESULT AudioFilter::GetTypeInfo(unsigned int iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
	return E_NOTIMPL;
}
HRESULT AudioFilter::GetTypeInfoCount(unsigned int *pctinfo)
{
	return E_NOTIMPL;
}
HRESULT AudioFilter::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, unsigned int *puArgErr)
{
	return E_NOTIMPL;
}

void AudioFilter::setBeginTime()
{
	clockLock.Lock();
	beginTime= getWriteTime();
	clockLock.Unlock();
}

STDMETHODIMP
AudioFilter::Stop()
{
	filterLock.Lock();
	if(m_State != State_Stopped){
		if(m_pInput->IsConnected()){
			m_pInput->m_bRunTimeError = FALSE;
			if(m_pInput->m_pAllocator) m_pInput->m_pAllocator->Decommit();
			else m_pInput->m_bFlushing = FALSE;
		}
		m_State = State_Stopped;
		//unblock streaming thread
		SetEvent(notifyEvent);
		//wait until streaming thread is finished
		receiveLock.Lock();
		receiveLock.Unlock();
		ended=true;
	}
	filterLock.Unlock();
	return S_OK;
}

STDMETHODIMP
AudioFilter::Pause()
{
	filterLock.Lock();

	HRESULT hr=S_OK;
	if(m_State != State_Paused){
		if(m_State == State_Stopped){
			if(m_pInput->IsConnected()){
				HRESULT hr= m_pInput->m_pAllocator->Commit();
				if(FAILED(hr)) goto lend;
			}
			if(!playing){
				flush(0);
			}
			else{
				if(!ending){
					if(!dsSeekDelay) stop();
					flush(msToB(dsSeekDelay));
				}
				setBeginTime();
			}
			ending=false;
		}
		else if(!noStop){
			stop();
		}
		m_State = State_Paused;
	}
lend:
	filterLock.Unlock();
	return hr;
}

STDMETHODIMP
AudioFilter::Run(REFERENCE_TIME tStart)
{
	HRESULT hr= VisualizationFilter::Run(tStart);
	ResetEvent(playEvent);
	SetEvent(notifyEvent);
	ended=false;
	if(!playing){
		if(state!=ID_FILE_PAUSE && !isPreBufDone(-1)){
			WaitForSingleObject(playEvent, isCD ? dsMaxPrebufCD : dsMaxPrebuf);
		}
		if(!noPlay) play();
	}
	return hr;
}

STDMETHODIMP AudioFilter::GetTime(REFERENCE_TIME *pTime)
{
	CheckPointer(pTime);
	*pTime= GetPrivateTime();
	if(*pTime<beginTime) *pTime=beginTime;
	return S_OK;
}


//=====================================================================
// CEnumMediaTypes
//=====================================================================

CEnumMediaTypes::CEnumMediaTypes(int pos) :
m_Position(pos), m_cRef(1)
{
}

STDMETHODIMP
CEnumMediaTypes::QueryInterface(REFIID riid, void **ppv)
{
	CheckPointer(ppv);
	if(riid == IID_IEnumMediaTypes || riid == IID_IUnknown){
		return GetInterface((IEnumMediaTypes *) this, ppv);
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
CEnumMediaTypes::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG)
CEnumMediaTypes::Release()
{
	ULONG cRef = InterlockedDecrement(&m_cRef);
	if(cRef == 0) delete this;
	return cRef;
}

STDMETHODIMP
CEnumMediaTypes::Clone(IEnumMediaTypes **ppEnum)
{
	CheckPointer(ppEnum);
	*ppEnum = new CEnumMediaTypes(m_Position);
	if(*ppEnum == NULL) return E_OUTOFMEMORY;
	return NOERROR;
}

STDMETHODIMP
CEnumMediaTypes::Next(ULONG cTypes, AM_MEDIA_TYPE **ppTypes, ULONG *pcFetched)
{
	CheckPointer(ppTypes);

	int cRealTypes = min(1 - m_Position, (int)cTypes);
	if(pcFetched!=NULL) *pcFetched = cRealTypes;
	else if(cTypes>1) return E_INVALIDARG;
	if(cRealTypes==0) return S_FALSE;
	while(cRealTypes){
		AM_MEDIA_TYPE *pType = (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
		ZeroMemory(pType, sizeof(AM_MEDIA_TYPE));
		pType->majortype=MEDIATYPE_Audio;
		pType->subtype=MEDIASUBTYPE_PCM;
		pType->formattype=FORMAT_None;
		WAVEFORMATEX *w= (WAVEFORMATEX *)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
		ZeroMemory(w, sizeof(WAVEFORMATEX));
		pType->pbFormat=(BYTE*)w; //Ogg Vorbis would crash if this was NULL
		m_Position++;
		*ppTypes++ = pType;
		cRealTypes--;
	}
	return NOERROR;
}

STDMETHODIMP
CEnumMediaTypes::Skip(ULONG cPins)
{
	if(cPins > ULONG(1 - m_Position)) return S_FALSE;
	m_Position += cPins;
	return NOERROR;
}

STDMETHODIMP
CEnumMediaTypes::Reset()
{
	m_Position = 0;
	return S_OK;
}


//=====================================================================
// AudioInputPin
//=====================================================================

AudioInputPin::AudioInputPin(AudioFilter *pFilter) : VisualizationInputPin(pFilter)
{
	dsFilter= pFilter;
}

STDMETHODIMP
AudioInputPin::EndOfStream()
{
	m_pFilter->receiveLock.Lock();
	HRESULT hr = CheckStreaming();
	if(hr==S_OK){
		dsFilter->ended=true;
		AudioFilter::ending=true;
		IMediaEventSink *e;
		if(m_pFilter->m_pGraph->QueryInterface(IID_IMediaEventSink, (void**)&e)==S_OK){
			e->Notify(EC_COMPLETE, 0, 0);
			e->Release();
		}
		if(isVideo){
			noStop++;
			AudioFilter::waitForEnd();
			noStop--;
			AudioFilter::ending=false;
		}
	}
	m_pFilter->receiveLock.Unlock();
	return hr;
}

STDMETHODIMP
AudioInputPin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
	*ppEnum= new CEnumMediaTypes(0);
	if(!*ppEnum) return E_OUTOFMEMORY;
	return S_OK;
}

STDMETHODIMP
AudioInputPin::Receive(IMediaSample *pSample)
{
	m_pFilter->receiveLock.Lock();

	HRESULT hr = Receive1(pSample);
	if(hr==S_OK){
		AM_SAMPLE2_PROPERTIES * const pProps = SampleProps();
		if(pProps->dwStreamId != AM_STREAM_MEDIA){
			hr=S_FALSE;
		}
		else{
			// call the visualization plugin
			hr=m_pFilter->Transform(pSample);
			if(SUCCEEDED(hr)){
				// send the sample to the audio renderer
				BYTE *pWave;
				pSample->GetPointer(&pWave);

				LONGLONG start, end;
				if(SUCCEEDED(pSample->GetTime(&start, &end))){
					REFERENCE_TIME r= dsFilter->getWriteTime();
					LONGLONG dl= (start+m_pFilter->m_tStart - r)*dsFilter->bufFormat.Format.nAvgBytesPerSec/UNITS;

					if(dl>10 && dl<1000000){ ///
						int d=(int)dl;
						if(dsFilter->bufFormat.Format.nBlockAlign){
							d-= d % dsFilter->bufFormat.Format.nBlockAlign;
						}
						dsFilter->clockLock.Lock();
						dsFilter->offsetBuf+=d;
						dsFilter->clockLock.Unlock();
						/*
						const int Dbuf=1024;
						void *buf=_alloca(Dbuf);
						memset(buf,0,Dbuf);
						while(d>0){
						int n;
						if(d>Dbuf) n=Dbuf;
						else n=(int)d;
						if(dsFilter->receive(buf,n)!=DS_OK) break;
						d-=n;
						}
						*/
					}
				}

				hr= dsFilter->receive(pWave, m_pFilter->LastMediaSampleSize);
				hr= (hr==DS_OK) ? S_OK : S_FALSE;
				/*
				static int lastUnderrun;
				if(AudioFilter::underrun!=lastUnderrun){
				lastUnderrun=AudioFilter::underrun;
				if(lastUnderrun && m_Connected){
				IQualityControl *pQC;
				if(m_Connected->QueryInterface(IID_IQualityControl, (void**)&pQC)==S_OK){
				Quality q;
				q.Type=Famine;
				q.Late=10000000;
				LONGLONG e;
				pSample->GetMediaTime(&q.TimeStamp,&e);
				q.Proportion=1000;
				pQC->Notify(m_pFilter, q);
				pQC->Release();
				}
				}
				}
				*/
			}
			else{
				hr=S_OK;
			}
		}
	}
	m_pFilter->receiveLock.Unlock();
	return hr;
}

STDMETHODIMP
AudioInputPin::ReceiveCanBlock()
{
	return S_OK;
}

STDMETHODIMP
AudioInputPin::BeginFlush()
{
	m_pLock->Lock();
	ASSERT(!m_bFlushing);
	m_bFlushing = TRUE;
	SetEvent(AudioFilter::notifyEvent);
	m_pLock->Unlock();
	return S_OK;
}

STDMETHODIMP
AudioInputPin::EndFlush()
{
	m_pLock->Lock();
	ASSERT(m_bFlushing);
	if(!dsFilter->ended){ //Ogg Vorbis sends flush at the end of stream
		dsFilter->flush(0);///
	}
	else{
		dsFilter->setBeginTime();
	}
	m_bFlushing = FALSE;
	m_bRunTimeError = FALSE;
	m_pLock->Unlock();
	return S_OK;
}

//------------------------------------------------------------------
LRESULT CALLBACK AudioFilterProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int utilization, sampleSize;
	struct TaudioProp{ int old; int *value; int id; };
	static TaudioProp intOpts[]={
		{0, &AudioFilter::locksDone, IDC_DS_LOCKS},
		{0, &sampleSize, IDC_DS_SAMPLE_SIZE},
		{0, &utilization, IDC_DS_BUFFERED},
		{0, &AudioFilter::underrun, IDC_DS_UNDERRUN},
	};
	int i;
	WAVEFORMATEX *w;
	RECT rcw;
	TCHAR buf[128];

	switch(message){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 33);
			for(i=0; i<sizeA(intOpts); i++){
				intOpts[i].old=-1;
			}
			AudioFilter::formatChanged=true;
			SetTimer(hWnd, 140, audioPropUpdate, 0);
			return TRUE;
		case WM_DESTROY:
			KillTimer(hWnd, 140);
			break;

		case WM_TIMER:
			if(AudioFilter::bufFormat.Format.nAvgBytesPerSec){
				utilization= (int)(1000*LONGLONG(AudioFilter::getFilled())/AudioFilter::bufFormat.Format.nAvgBytesPerSec);
			}
			if(visualFilter){
				sampleSize=visualFilter->LastMediaSampleSize;
			}
			for(i=0; i<sizeA(intOpts); i++){
				if(*intOpts[i].value!=intOpts[i].old){
					SetDlgItemInt(hWnd, intOpts[i].id, intOpts[i].old= *intOpts[i].value, TRUE);
				}
			}
			if(AudioFilter::formatChanged){
				AudioFilter::formatChanged=false;
				w= &AudioFilter::bufFormat.Format;
				if(w->nChannels){
					int bits = w->wBitsPerSample;
					if(w->wFormatTag == WAVE_FORMAT_EXTENSIBLE && w->cbSize >= 22){
						bits = ((WAVEFORMATEXTENSIBLE*)w)->Samples.wValidBitsPerSample;
					}
					_stprintf(buf, lng(677, _T("%d Hz, %d bit, %s\n")),
					w->nSamplesPerSec, bits,
					w->nChannels>1 ? _T("stereo") : _T("mono"));
					SetDlgItemText(hWnd, IDC_DS_FORMAT, buf);
				}
			}
			break;
		case WM_MOVE:
			GetWindowRect(hWnd, &rcw);
			audioDialogX= rcw.left;
			audioDialogY= rcw.top;
			break;
		case WM_CLOSE:
			audioDialog=0;
			DestroyWindow(hWnd);
			return TRUE;
	}
	return FALSE;
}

//------------------------------------------------------------------

void parseEqualizerFreq()
{
	int f;
	TCHAR *s, *e;

lstart:
	Nequalizer=0;
	for(s=eqFreqStr; *s;){
		f=_tcstol(s, &e, 10);
		if(e>s){
			s=e;
			if(Nequalizer<Mequalizer){
				aminmax(f, 80, 16000);
				eqParam[Nequalizer++].freq=(float)f;
			}
		}
		else{
			s++;
		}
	}
	if(!Nequalizer){
		_tcscpy(eqFreqStr, _T("80,150,250,500,1000,2000,4000,8000,16000"));
		goto lstart;
	}
}

void setEqualizerSliders()
{
	for(int i=0; i<Nequalizer; i++){
		SendDlgItemMessage(equalizerWnd, 100+i, TBM_SETPOS, TRUE, (int)floor(eqParam[i].value*(eqSliderH-1)/(-30)+0.5));
	}
}

LRESULT CALLBACK EqualizerProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int i;
	RECT rcw;
	static bool err;

	switch(message){
		case WM_VSCROLL:
			if((LOWORD(wParam)==TB_ENDTRACK || LOWORD(wParam)==TB_THUMBTRACK)){
				i=GetDlgCtrlID((HWND)lParam)-100;
				if(i>=0 && i<Nequalizer){
					eqParam[i].value= SendMessage((HWND)lParam, TBM_GETPOS, 0, 0)*(-30)/(float)(eqSliderH-1);
					if(eqParam[i].index<0){
						AudioFilter::initEqualizer(true);
					}
					else{
						AudioFilter::setEqualizer();
					}
					if(!AudioFilter::buffer8 && (AudioFilter::buffer || !audioFilter) && !err){
						err=true;
						HWND o=dlgWin;
						dlgWin=hWnd;
						if(audioFilter){
							if(!dsEq) msglng(869, "You have to check \"Enable equalizer\" in the options");
							else msglng(783, "This function is not available.\nTry to install DirectX 9.");
						}
						else{
							msglng(781, "You have to choose built-in audio renderer if you want equalizer");
						}
						dlgWin=o;
					}
				}
			}
			break;
		case WM_COMMAND:
			if(LOWORD(wParam)==500){
				for(i=0; i<Nequalizer; i++) eqParam[i].value=0;
				setEqualizerSliders();
				AudioFilter::setEqualizer();
			}
			break;
		case WM_MOVE:
			GetWindowRect(hWnd, &rcw);
			eqX= rcw.left;
			eqY= rcw.top;
			break;
		case WM_CLOSE:
			err=false;
			equalizerWnd=0;
			eqVisible=0;
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void showEqualizer()
{
	int i, f, eqLabelH;
	TCHAR buf[12], *s;

	if(!equalizerWnd){
		eqLabelH= HIWORD(GetDialogBaseUnits());
		equalizerWnd= CreateWindowEx(WS_EX_TOOLWINDOW, EQUALIZERCLASSNAME,
			lng(1297, _T("Equalizer")), WS_OVERLAPPED|WS_BORDER|WS_SYSMENU,
			eqX, eqY, Nequalizer*eqDx+98+eqSliderX, 6+eqSliderH+eqSliderY+eqLabelH+GetSystemMetrics(SM_CYSMCAPTION), hWin, 0, inst, 0);
		for(i=0; i<Nequalizer; i++){
			HWND w=CreateWindow(TRACKBAR_CLASS, _T(""),
				TBS_VERT|TBS_BOTH|TBS_NOTICKS|TBS_TOOLTIPS|WS_CHILD|WS_VISIBLE,
				i*eqDx+eqSliderX, eqSliderY, eqDx, eqSliderH, equalizerWnd, (HMENU)(100+i), inst, 0);
			SendMessage(w, TBM_SETRANGE, TRUE, MAKELONG(-eqSliderH/2, eqSliderH/2));
			f=(int)eqParam[i].freq;
			_itot(f, buf, 10);
			if(f>999){
				s=_tcschr(buf, 0);
				s[-3]='k';
				s[-2]=0;
			}
			CreateWindow(_T("STATIC"), buf, SS_CENTER|WS_CHILD|WS_VISIBLE,
				i*eqDx, eqSliderY+eqSliderH, eqDx, eqLabelH, equalizerWnd, (HMENU)(200+i), inst, 0);
			CreateWindow(_T("BUTTON"), lng(540, _T("Reset")), WS_CHILD|WS_VISIBLE,
				Nequalizer*eqDx+eqSliderX+1, 10, 90, eqLabelH+4, equalizerWnd, (HMENU)500, inst, 0);
		}
		setEqualizerSliders();
		ShowWindow(equalizerWnd, SW_SHOW);
		eqVisible=1;
	}
	else{
		if(GetForegroundWindow()==equalizerWnd){
			PostMessage(equalizerWnd, WM_CLOSE, 0, 0);
		}
		else{
			SetForegroundWindow(equalizerWnd);
		}
	}
}

void AudioFilter::OnUnderRun()
{
	if(isDVD && g_bMenuOn){
		char buf[1440];
		memset(buf, (bufFormat.Format.wBitsPerSample==8) ? 128 : 0, sizeof(buf));
		for(int Dnul=0; Dnul<50000; Dnul+=sizeof(buf)){
			put(buf, sizeof(buf));
		}
	}
	else if(GetTickCount()-flushTime>500){
		isUnderRun=true;
		underrun++;
		stop();
		writeTime=GetTickCount();
	}
}
