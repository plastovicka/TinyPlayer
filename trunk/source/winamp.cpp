/*
	(C) 2005-2011  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "tinyplay.h"
#include "sound.h"
#include "winamp.h"

extern REFERENCE_TIME refTimeDiff;
AudioFilter *waAudioFilter;
In_Module *Winamp::in;
int Winamp::lock;
TCHAR Winamp::menuName[128];
static bool waBufOk;
static Darray<In_Module*> plugins;

//---------------------------------------------------------------------------

int waOpen(int samplerate, int numchannels, int bitspersamp, int /*bufferlenms*/, int /*prebufferms*/)
{
	//create audio buffer
	WAVEFORMATEX fmt;
	fmt.cbSize=0;
	fmt.nBlockAlign=(WORD)(numchannels*bitspersamp/8);
	fmt.nAvgBytesPerSec=numchannels*samplerate*bitspersamp/8;
	fmt.nChannels=(WORD)numchannels;
	fmt.nSamplesPerSec=samplerate;
	fmt.wBitsPerSample=(WORD)bitspersamp;
	fmt.wFormatTag=WAVE_FORMAT_PCM;
	AudioFilter *ds= waAudioFilter;
	if(ds->createBuffer(&fmt)!=DS_OK) return -1;
	ds->format.Format=fmt;

	if(!pBA) ds->QueryInterface(IID_IBasicAudio, (void **)&pBA);
	ds->Pause();
	REFERENCE_TIME r;
	ds->GetTime(&r);
	refTimeDiff= -r;
	ds->m_pInput->m_bFlushing=FALSE;
	waBufOk=true;
	return 1000;
}

// This function is non-blocking in Winamp, 
// but blocking in TinyPlayer !!!
int waWrite(char *buf, int len)
{
	waAudioFilter->LastMediaSampleSize= len;
	if(SUCCEEDED(waAudioFilter->Transform((BYTE*)buf, waAudioFilter->getWriteTime()+refTimeDiff))){
		//write samples to DirectSound buffer
		if(waAudioFilter->receive(buf, len)!=DS_OK) return 1;
	}
	return 0;
}

int waCanWrite()
{
	return 1000000;
}

int waPause(int pause)
{
	int result= !waAudioFilter->playing;
	if(pause) waAudioFilter->Pause();
	else waAudioFilter->Run(0);
	return result;
}

void waFlush(int)
{
	//NOTE: audio buffer was already flushed in seek
	waAudioFilter->m_pInput->m_bFlushing=FALSE;
}

int waDsp_dosamples(short int *, int numsamples, int, int, int)
{
	return numsamples;
}

int dummy()
{
	return 0;
}

Out_Module out={
	OUT_VER, "tinyplay", 79671, 0, 0,
	(void(*)(HWND))dummy, (void(*)(HWND))dummy,
	(void(*)())dummy, (void(*)())dummy,
	waOpen, (void(*)())dummy, waWrite, waCanWrite,
	(int(*)())dummy, waPause,
	(void(*)(int))dummy, (void(*)(int))dummy,
	waFlush, (int(*)())dummy, (int(*)())dummy
};

//---------------------------------------------------------------------------

//compare current file extension with plugin's extensions
int waCmpExt(char *ext, char *s)
{
	char *e;
	bool b;

	if(s && *s){
		for(;;){
			if(*s==';') s++;
			if(*s==0){
				s=strchr(s+1, 0)+1;
				if(*s==0) break;
			}
			e=ext;
			b=true;
			while(*s && *s!=';'){
				if(tolower(*s)!=tolower(*e)) b=false;
				else e++;
				s++;
			}
			if(b && *e==0) return 1;
		}
	}
	return 0;
}

//action: 0=play, 1=getInfo
bool Winamp::open(Titem *item, int action)
{
	int i, k;
	In_Module *in;
	char *ext;
	AudioFilter *ds=0;
	HRESULT hr;
	char *title=(char*)_alloca(512);

	//load all plugins into memory
	loadPlugins();
	//create AudioFilter
	if(action==0){
		hr=S_OK;
		ds = new AudioFilter(&hr);
		if(hr!=S_OK){
			delete ds;
			return false;
		}
		ds->AddRef();
	}
	//file extension
	convertT2A(item->file, fnA);
	for(ext= strchr(fnA, 0);; ext--){
		if(*ext=='.'){
			ext++;
			break;
		}
		if(ext==fnA){
			ext="";
			break;
		}
	}
	for(k=action; k<2; k++){
		for(i=0; i<plugins.len; i++){
			in=plugins[i];
			if(k ? waCmpExt(ext, in->FileExtensions) : in->IsOurFile(fnA)){
				if(action==0){ //play
					Winamp::in=in;
					waAudioFilter=ds;
					waBufOk=false;
					if(!in->Play(fnA)){
						if(in->UsesOutputPlug){
							//many plugins create audio buffer in Play()
							//mp3 plugin creates buffer later, so we have to wait
							int cnt=500;
							while(!waBufOk && --cnt>0){
								Sleep(20);
							}
						}
						return true;
					}
					waAudioFilter=0;
				}
				else{ //get duration
					int d=0;
					in->GetFileInfo(fnA, title, &d);
					if(d>0){
						item->length=LONGLONG(d)*10000;
						return true;
					}
				}
			}
		}
	}
	SAFE_RELEASE(ds);
	return false;
}

void Winamp::loadPlugins()
{
	if(plugins.len) return;
	HANDLE h;
	HMODULE dll;
	TCHAR *filePart;
	HKEY key;
	DWORD dwDummy, sz;
	UINT len;
	WIN32_FIND_DATA &fd=*(WIN32_FIND_DATA*)_alloca(sizeof(WIN32_FIND_DATA)+sizeof(VS_FIXEDFILEINFO)+256*sizeof(TCHAR));
	VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO*)(&fd+1);
	TCHAR *buf=(TCHAR*)(verInfo+1);

	//winamp installation folder
	_tcscpy(buf, _T("C:\\Program Files\\WINAMP"));
	if(RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Winamp"), 0, KEY_QUERY_VALUE, &key)==ERROR_SUCCESS){
		DWORD d=480;
		RegQueryValueEx(key, 0, 0, 0, (BYTE *)buf, &d);
		RegCloseKey(key);
	}
	//winamp version
	filePart = _tcschr(buf, 0);
	_tcscpy(filePart, _T("\\winamp.exe"));
	sz = GetFileVersionInfoSize(buf, &dwDummy);
	if(sz){
		DWORD major=0;
		BYTE *ver = new BYTE[sz];
		if(GetFileVersionInfo(buf, 0, sz, ver)){
			if(VerQueryValue(ver, _T("\\"), (void**)&verInfo, &len)){
				major=verInfo->dwFileVersionMS;
			}
		}
		delete[] ver;
		if(major > 0x50001) return; //version newer than 5.1 are not supported
	}
	//plugin folder
	_tcscpy(filePart, _T("\\Plugins\\"));
	filePart=_tcschr(filePart, 0);
	//find only INPUT plugins
	_tcscpy(filePart, _T("in_*.dll"));
	h = FindFirstFile(buf, &fd);
	if(h!=INVALID_HANDLE_VALUE){
		do{
			_tcscpy(filePart, fd.cFileName);
			dll=LoadLibrary(buf);
			if(dll){
				typedef In_Module*(*TwaDllFunc)();
				TwaDllFunc f= (TwaDllFunc)GetProcAddress(dll, "winampGetInModule2");
				if(f){
					in=f();
					if(in){
						in->outMod=&out;
						in->hMainWindow=hWin;
						in->hDllInstance=dll;
						in->dsp_dosamples=waDsp_dosamples;
						in->SAVSAInit=(void(*)(int, int))dummy;
						in->SAVSADeInit=(void(*)())dummy;
						in->SAAddPCMData=(void(*)(void*, int, int, int))dummy;
						in->SAGetMode=(int(*)())dummy;
						in->SAAdd=(void(*)(void*, int, int))dummy;
						in->VSAAddPCMData=(void(*)(void*, int, int, int))dummy;
						in->VSAGetMode=(int(*)(int*, int*))dummy;
						in->VSAAdd=(void(*)(void*, int))dummy;
						in->VSASetInfo=(void(*)(int, int))dummy;
						in->dsp_isactive=dummy;
						in->SetInfo=(void(*)(int, int, int, int))dummy;
						in->Init();
						*plugins++= in;
						continue;
					}
				}
				FreeLibrary(dll);
			}
		} while(FindNextFile(h, &fd));
		FindClose(h);
	}
}

void Winamp::unloadPlugins()
{
	if(lock || waAudioFilter) return;
	for(int i=0; i<plugins.len; i++){
		In_Module *in= plugins[i];
		in->Quit();
		FreeLibrary(in->hDllInstance);
	}
	plugins.reset();
}

//---------------------------------------------------------------------------

bool Winamp::run()
{
	in->UnPause();
	return true;
}

bool Winamp::stop()
{
	//unblock working thread
	waAudioFilter->m_pInput->m_bFlushing=TRUE;
	SetEvent(waAudioFilter->notifyEvent);
	//stop the plugin
	in->Stop();
	return true;
}

bool Winamp::pause()
{
	in->Pause();
	return true;
}

bool Winamp::seek(LONGLONG pos)
{
	if(!in->is_seekable) return true;
	waAudioFilter->m_pInput->m_bFlushing=TRUE;
	SetEvent(waAudioFilter->notifyEvent);
	waAudioFilter->flush(0);
	waAudioFilter->setBeginTime();
	refTimeDiff= pos - waAudioFilter->beginTime;
	in->SetOutputTime(int(pos/10000));
	return false;
}

REFERENCE_TIME Winamp::streamTime()
{
	REFERENCE_TIME pos;
	if(!waAudioFilter || FAILED(waAudioFilter->GetTime(&pos))) return 0;
	return pos+refTimeDiff;
}

bool Winamp::getDuration(LONGLONG *duration)
{
	int d= in->GetLength();
	if(d<=0) return false;
	*duration= LONGLONG(d)*10000;
	return true;
}

void waNext()
{
	if(waAudioFilter){
		AudioFilter::ending=true;
		nextClip();
	}
}

