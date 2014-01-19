/*
	(C) 2004-2005  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "tinyplay.h"
#include "sound.h"
#include "winamp.h"

TCHAR visualDLL[64]=_T("spectrum");

int Nvisuals;
int visualWidth, visualHeight;
bool isGrabbingWAV=false;
BYTE *pBuffer;                //sound buffer
int nBuf;                     //size of pBuffer (bytes)
int nSamples;                 //size of pBuffer (samples)
BYTE *pIndex;                 //pointer after the last sample
HMODULE visualLib;
void *visualData;
HWND visualDialog;
HANDLE paintThread;
DWORD paintThreadId;
HBITMAP paintBitmap;
HDC paintDC;
CRITICAL_SECTION critSect;
HANDLE wavFile=INVALID_HANDLE_VALUE;
DWORD wavFmtSize;

static HGDIOBJ oldBitmap;
static RECT visSnap1, visSnap2, visSnap3;

typedef void* (*TvisInit)(HWND videoWnd);
typedef void(*TvisSetSize)(void *data, int w, int h, HDC dc);
typedef void(*TvisSetFormat)(void *data, WAVEFORMATEX *format);
typedef void(*TvisDraw)(void *data, BYTE *start, BYTE *index, BYTE *end);
typedef BOOL(*TvisPropertyPage)(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
typedef void(*TvisFree)(void *data);

TvisInit visInit;
TvisSetSize visSetSize;
TvisSetFormat visSetFormat;
TvisDraw visDraw;
TvisPropertyPage visPropertyPage;
TvisFree visFree;

const int threadPriorCnst[]={
	THREAD_PRIORITY_IDLE, THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL,
	THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL
};

//------------------------------------------------------------------

void VisualizationFilter::draw()
{
	int start, d;
	REFERENCE_TIME delta, rt;

	//calculate the current position in the buffer
	d= format.Format.wBitsPerSample*format.Format.nChannels/8;
	if(!d){
		ShowWindow(visualWnd, SW_HIDE);
		return;
	}
	if(cdAudioFilter){
		ripPos();
		rt=position;
	}
	else if(waAudioFilter){
		rt=Winamp::streamTime();
	}
	else{
		StreamTime(rt);
	}
	visualPosLock.Lock();
	delta = EndSample - rt;
	start = (int)((pIndex-pBuffer)/d);
	start -= int(delta * format.Format.nSamplesPerSec / UNITS);
	if(start<0){
		start += nSamples;
		if(start<0) start= (pIndex-pBuffer)/d;
	}
	visualPosLock.Unlock();
	if(start>=nSamples) start=0;
	//call the plugin
	visDraw(visualData, pBuffer, pBuffer+start*d, pBuffer+nBuf);
}

// Check that we can support a given proposed type
HRESULT VisualizationFilter::CheckInputType(const CMediaType* pmt)
{
	HRESULT hr = CheckInputType1(pmt);
	if(SUCCEEDED(hr)){
		WAVEFORMATEX *pwf = (WAVEFORMATEX *)pmt->Format();
		memcpy(&format, pwf, min(sizeof(WAVEFORMATEX)+pwf->cbSize, sizeof(format)));
		allocBuffer(plugin);
	}
	return hr;
}

HRESULT VisualizationFilter::CheckInputType1(const CMediaType* pmt)
{
	CheckPointer(pmt);

	// Reject non-PCM Audio type
	WAVEFORMATEX *pwf = (WAVEFORMATEX *)pmt->Format();
	if(pmt->majortype != MEDIATYPE_Audio ||
		 pmt->formattype != FORMAT_WaveFormatEx ||
		 !pwf ||
		 pwf->wFormatTag != WAVE_FORMAT_PCM &&
		 pwf->wFormatTag != WAVE_FORMAT_IEEE_FLOAT &&
		 (pwf->wFormatTag != WAVE_FORMAT_EXTENSIBLE || pwf->cbSize<22 ||
		 ((WAVEFORMATEXTENSIBLE *)pwf)->SubFormat !=  MEDIASUBTYPE_PCM &&
		 ((WAVEFORMATEXTENSIBLE *)pwf)->SubFormat !=  MEDIASUBTYPE_IEEE_FLOAT)
		 ){
		return E_INVALIDARG;
	}
	return S_OK;
}

void VisualizationFilter::allocBuffer(void *d)
{
	if(d){
		EnterCriticalSection(&critSect);
		//allocate buffer
		nSamples = max(dsBufferSize/1000+1, 2)*format.Format.nSamplesPerSec;
		int len= nSamples*format.Format.nChannels*format.Format.wBitsPerSample/8;
		if(len!=nBuf){
			delete[] pBuffer;
			pIndex = pBuffer = new BYTE[nBuf=len];
		}
		//inform plugin
		visSetFormat(d, (WAVEFORMATEX*)&format);
		plugin=d;
		LeaveCriticalSection(&critSect);
	}
}

void createWAVfile(WAVEFORMATEXTENSIBLE *fmt)
{
	TCHAR *s, name[MAX_PATH], name2[MAX_PATH*2];
	lstrcpyn(name, playlist[currentFile].file, sizeA(name)-8);
	s=name+lstrlen(name)-4;
	if(s<name || *s!='.') s+=4;
	if(!_tcscmp(s, _T(".wav"))) return; //don't copy wav to wav
	lstrcpy(s, _T("-out.wav"));
	wavFile=CreateFile(name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if(wavFile==INVALID_HANDLE_VALUE){
		getExeDir(name2, cutPath(name));
		wavFile=CreateFile(name2, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	}
	char head[68];
	WORD fmtSize = fmt->Format.cbSize;
	if(fmtSize > 0) fmtSize += 2;
	fmtSize += 16;
	memcpy(head, "RIFF    WAVEfmt ", 16);
	*((DWORD*)(head+16)) = fmtSize;
	memcpy(head+20, fmt, fmtSize);
	memcpy(head+20+fmtSize, "data", 4);
	wavFmtSize=fmtSize;
	DWORD w;
	WriteFile(wavFile, head, 28+fmtSize, &w, 0);
}

void VisualizationFilter::transformWave(BYTE *pWave)
{
	BYTE *p, *e;
	SHORT *s;
	LONG *l;
	float *f;
	int incr, m;

	if(soundConversion==ID_SOUND_MUTE){
		memset(pWave, (format.Format.wBitsPerSample==8) ? 128 : 0, LastMediaSampleSize);
		return;
	}
	if(soundConversion==ID_SOUND_WAV){ //save to WAV
		if(wavFile==INVALID_HANDLE_VALUE){
			createWAVfile(&format);
		}
		DWORD w;
		WriteFile(wavFile, pWave, LastMediaSampleSize, &w, 0);
		return;
	}
	if(format.Format.nChannels<2) return;
	e=pWave+LastMediaSampleSize;
	incr = (format.Format.wBitsPerSample>>3)*format.Format.nChannels;

	for(p=pWave; p<e; p+=incr){
		switch(soundConversion){
			case ID_SOUND_MONO:
				switch(format.Format.wBitsPerSample){
					case 8:
						p[0]=p[1]=(BYTE)((p[0]+p[1])>>1);
						break;
					case 16:
						s=(SHORT*)p;
						s[0]=s[1]=(SHORT)((s[0]+s[1])>>1);
						break;
					case 24:
						m=(((((p[2]+p[5])<<8)+(p[1]+p[4]))<<8)+(p[0]+p[3]))>>1;
						p[0]=p[3]=(BYTE)m;
						p[1]=p[4]=(BYTE)(m>>8);
						p[2]=p[5]=(BYTE)(m>>16);
						break;
					case 32:
						if(format.Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
							format.Format.wFormatTag==WAVE_FORMAT_EXTENSIBLE && format.SubFormat == MEDIASUBTYPE_IEEE_FLOAT){
							f=(float*)p;
							f[0]=f[1]=(f[0]+f[1])/2;
						}
						else{
							l=(LONG*)p;
							l[0]=l[1]=(LONG)((l[0]+(LONGLONG)l[1])>>1);
						}
						break;
				}
				break;
			case ID_SOUND_LEFT:
				switch(format.Format.wBitsPerSample){
					case 8:
						p[1]=p[0];
						break;
					case 16:
						s=(SHORT*)p;
						s[1]=s[0];
						break;
					case 24:
						p[3]=p[0]; p[4]=p[1]; p[5]=p[2];
						break;
					case 32:
						l=(LONG*)p;
						l[1]=l[0];
						break;
				}
				break;
			case ID_SOUND_RIGHT:
				switch(format.Format.wBitsPerSample){
					case 8:
						p[0]=p[1];
						break;
					case 16:
						s=(SHORT*)p;
						s[0]=s[1];
						break;
					case 24:
						p[0]=p[3]; p[1]=p[4]; p[2]=p[5];
						break;
					case 32:
						l=(LONG*)p;
						l[0]=l[1];
						break;
				}
				break;
		}
	}
}

// Here's the next block of data from the stream
HRESULT VisualizationFilter::Transform(IMediaSample *pSample)
{
	BYTE *pWave;
	pSample->GetPointer(&pWave);
	REFERENCE_TIME tStart, tEnd;
	pSample->GetTime(&tStart, &tEnd);
	LastMediaSampleSize = pSample->GetActualDataLength();
	return Transform(pWave, tEnd);
}

HRESULT VisualizationFilter::Transform(BYTE *pWave, REFERENCE_TIME tEnd)
{
	if(soundConversion!=ID_SOUND_STEREO){
		transformWave(pWave);
	}
	if(isVisual && (!isVideo || !fullscreen || winOnFull || multiMonitors()) && plugin){
		if(pBuffer){
			int i= int(pIndex-pBuffer);
			int o= i+LastMediaSampleSize-nBuf;
			if(o>0){
				if(o>nBuf) o=nBuf;
				memcpy(pIndex, pWave, nBuf-i);
				memcpy(pBuffer, pWave+nBuf-i, o);
			}
			else{
				memcpy(pIndex, pWave, LastMediaSampleSize);
			}
			visualPosLock.Lock();
			if(o>0){
				pIndex= pBuffer + o;
			}
			else{
				pIndex += LastMediaSampleSize;
			}
			EndSample=tEnd;
			visualPosLock.Unlock();
		}
	}
	else{
		if(soundConversion==ID_SOUND_WAV && !isVisual){
			EndSample=tEnd;
			isGrabbingWAV=true;
			AudioFilter::skip(LastMediaSampleSize);
			return E_FAIL;
		}
	}
	if(isGrabbingWAV){
		isGrabbingWAV=false;
		if(state==ID_FILE_PLAY){
			AudioFilter::play();
			SetEvent(AudioFilter::notifyEvent);
		}
	}
	return S_OK;
}

/* Return information about this filter */
STDMETHODIMP
VisualizationFilter::QueryFilterInfo(FILTER_INFO *pInfo)
{
	CheckPointer(pInfo);

	lstrcpyW(pInfo->achName, VISUAL_FILTER_NAMEW);
	pInfo->pGraph = m_pGraph;
	if(m_pGraph)  m_pGraph->AddRef();
	return NOERROR;
}


//------------------------------------------------------------------
void delBitmap()
{
	if(paintDC){
		DeleteObject(SelectObject(paintDC, oldBitmap));
		DeleteDC(paintDC);
		paintDC=0;
	}
}

void onResize(int w, int h)
{
	if(w!=visualWidth || h!=visualHeight){
		visualWidth=w;
		visualHeight=h;
		delBitmap();
		HDC hdc = GetDC(visualWnd);
		paintDC = CreateCompatibleDC(hdc);
		paintBitmap= CreateCompatibleBitmap(hdc, w, h);
		oldBitmap = SelectObject(paintDC, paintBitmap);
		ReleaseDC(visualWnd, hdc);
	}
	EnterCriticalSection(&critSect);
	if(visualData) visSetSize(visualData, w, h, paintDC);
	LeaveCriticalSection(&critSect);
}

void onVisualMoved()
{
	if(!IsZoomed(visualWnd) && !IsIconic(visualWnd)){
		RECT rc;
		GetWindowRect(visualWnd, &rc);
		visualTop= rc.top;
		visualLeft= rc.left;
		visualW= rc.right-rc.left;
		visualH= rc.bottom-rc.top;
	}
}

LRESULT CALLBACK visualWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message){
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmm = (LPMINMAXINFO)lParam;
			lpmm->ptMinTrackSize.x = 20;
			lpmm->ptMinTrackSize.y = 20;
		}
			break;
		case WM_EXITSIZEMOVE:
			onVisualMoved();
			SetRectEmpty(&visSnap1);
			SetRectEmpty(&visSnap2);
			SetRectEmpty(&visSnap3);
			break;
		case WM_SIZE:
			onResize(LOWORD(lParam), HIWORD(lParam));
			InvalidateRect(hWnd, 0, FALSE);
			break;
		case WM_MOVING:
			snapWnd1((RECT*)lParam, &visSnap1);
			snapWnd2((RECT*)lParam, videoWnd, &visSnap2);
			snapWnd2((RECT*)lParam, hWin, &visSnap3);
			return TRUE;

		case WM_MOUSEMOVE:
			//this message is sent only in fullscreen
			SetCursor(0);
			mouseMoveFull();
			break;

		case WM_NCHITTEST:
		{
			LRESULT ht= DefWindowProc(hWnd, message, wParam, lParam);
			if(ht==HTCLIENT && !fullscreen) ht=HTCAPTION;
			return ht;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			if(ps.rcPaint.bottom || ps.rcPaint.right){
				EnterCriticalSection(&critSect);
				if(visualData && visualFilter){
					visualFilter->draw();
				}
				LeaveCriticalSection(&critSect);
				BitBlt(ps.hdc, 0, 0, visualWidth, visualHeight, paintDC, 0, 0, SRCCOPY);
			}
			EndPaint(hWnd, &ps);
		}
			break;
		case WM_TIMER:
			if(state==ID_FILE_PLAY && (!isVideo || !fullscreen || winOnFull || multiMonitors())){
				InvalidateRect(hWnd, 0, FALSE);
			}
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_APPCOMMAND:
			PostMessage(hWin, message, wParam, lParam);
			break;
		case WM_NCLBUTTONDBLCLK:
			visualProperty(1);
			break;
		case WM_RBUTTONUP:
		case WM_NCRBUTTONUP:
			nextVisual();
			hideWinOnFull();
			break;

		case WM_DESTROY:
			delBitmap();
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL CALLBACK propertyPageProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int id=0;

	if(uMsg==WM_INITDIALOG){
		SetWindowLong(hDlg, (DWORD)GWL_USERDATA, (LONG)visualData);
		lParam=(LPARAM)&id;
	}
	BOOL result = visPropertyPage(hDlg, uMsg, wParam, lParam);
	if(uMsg==WM_INITDIALOG){
		setDlgTexts(hDlg, id);
	}
	if(uMsg==WM_MOVE){
		RECT rcw;
		GetWindowRect(hDlg, &rcw);
		visualDialogX= rcw.left;
		visualDialogY= rcw.top;
	}
	if(uMsg==WM_CLOSE){
		visualDialog=0;
		DestroyWindow(hDlg);
		result=TRUE;
	}
	return result;
}


void loadVisualDlg()
{
	HWND w=CreateDialog(visualLib, MAKEINTRESOURCE(101), 0, (DLGPROC)propertyPageProc);;
	if(!w) return;
	SetWindowPos(w, 0, visualDialogX, visualDialogY, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
	ShowWindow(w, SW_SHOW);
	if(visualDialog) DestroyWindow(visualDialog);
	visualDialog= w;
}

void loadAudioDlg()
{
	changeDialog(audioDialog, audioDialogX, audioDialogY, MAKEINTRESOURCE(IDD_AUDIO_FILTER), (DLGPROC)AudioFilterProc);
	ShowWindow(audioDialog, SW_SHOW);
}

static HANDLE startEvent;


DWORD WINAPI paintProc(LPVOID p)
{
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.style= CS_DBLCLKS;
	wc.lpfnWndProc = visualWndProc;
	wc.hInstance = inst;
	wc.lpszClassName = VISUALCLASSNAME;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = 0;
	RegisterClass(&wc);
	visualWnd = CreateWindowEx(WS_EX_TOOLWINDOW,
		VISUALCLASSNAME, _T(""),
		WS_THICKFRAME|WS_POPUP,
		visualLeft, visualTop, visualW, visualH, 0, 0, inst, 0);
	SetEvent(startEvent);

	MSG mesg;
	while(GetMessage(&mesg, NULL, 0, 0)==TRUE){
		if(mesg.message == WM_VISUAL_PROPERTY){
			if(visualData && visPropertyPage){
				loadVisualDlg();
				if(mesg.wParam) SetForegroundWindow(visualDialog);
			}
		}
		else if(mesg.message == WM_AUDIO_PROPERTY){
			loadAudioDlg();
			if(mesg.wParam) SetForegroundWindow(audioDialog);
		}
		else if(!TranslateAccelerator(hWin, haccel, &mesg)){
			if(!visualDialog || !IsDialogMessage(visualDialog, &mesg)){
				TranslateMessage(&mesg);
				DispatchMessage(&mesg);
			}
		}
	}
	return 0;
}

//show property page dialog
void visualProperty(WPARAM activ)
{
	PostThreadMessage(paintThreadId, WM_VISUAL_PROPERTY, activ, 0);
}

void audioProperty(WPARAM activ)
{
	PostThreadMessage(paintThreadId, WM_AUDIO_PROPERTY, activ, 0);
}

//delete plugin and unload DLL
void freeVisual()
{
	if(visualLib){
		if(visFree){
			//close config dialog
			if(visualDialog) SendMessage(visualDialog, WM_CLOSE, 0, 0);
			//delete plugin
			EnterCriticalSection(&critSect);
			if(visualFilter) visualFilter->plugin=0;
			visFree(visualData);
			visualData=0;
			LeaveCriticalSection(&critSect);
		}
		FreeLibrary(visualLib);
		visualLib=0;
	}
}

int getVisualNum()
{
	for(int j=0; j<Nvisuals; j++){
		if(!lstrcmpi(visualNames[j], visualDLL)) return j;
	}
	return -1;
}

void checkVisualMenu()
{
	int i, j;

	j=getVisualNum();
	for(i=0; i<Nvisuals; i++){
		checkMenuItem(ID_VISUALS+i, i==j);
	}
}

//load DLL and create plugin
void addVisual()
{
	static const int Mbuf=128;
	TCHAR *buf=(TCHAR*)_alloca(2*Mbuf);

	if(!paintThread) return;
	freeVisual();
	lstrcpyn(buf, visualDLL, Mbuf-5);
	lstrcat(buf, _T(".dll"));
	HMODULE lib= visualLib= LoadLibrary(buf);
	if(lib){
		visInit= (TvisInit)GetProcAddress(lib, "visInit");
		visSetSize= (TvisSetSize)GetProcAddress(lib, "visSetSize");
		visSetFormat= (TvisSetFormat)GetProcAddress(lib, "visSetFormat");
		visDraw= (TvisDraw)GetProcAddress(lib, "visDraw");
		visPropertyPage= (TvisPropertyPage)GetProcAddress(lib, "visPropertyPage");
		visFree= (TvisFree)GetProcAddress(lib, "visFree");
		if(!visInit || !visSetSize || !visSetFormat || !visDraw || !visFree){
			msglng(785, "Visualization DLL does not export required functions");
			return;
		}
		void *v= visInit(visualWnd);
		if(v){
			visSetSize(v, visualWidth, visualHeight, paintDC);
			visualData=v;
			if(visualFilter) visualFilter->allocBuffer(v);
			checkVisualMenu();
		}
	}
}

void nextVisual()
{
	int j= getVisualNum()+1;
	if(j==Nvisuals) j=0;
	PostMessage(hWin, WM_COMMAND, j+ID_VISUALS, 0);
}

void visualMenu()
{
	HMENU m=GetMenu(hWin);
	for(int j=0; j<Nvisuals; j++){
		InsertMenu(m, 39999, MF_BYCOMMAND,
			ID_VISUALS+j, visualNames[j]);
	}
	RemoveMenu(m, 39999, MF_BYCOMMAND);
	checkVisualMenu();
}

void findDLL()
{
	HANDLE h;
	WIN32_FIND_DATA &fd=*(WIN32_FIND_DATA*)_alloca(sizeof(WIN32_FIND_DATA)+512);
	TCHAR *buf=(TCHAR*)(&fd+1);

	getExeDir(buf, _T("*.dll"));
	h = FindFirstFile(buf, &fd);
	if(h!=INVALID_HANDLE_VALUE){
		Nvisuals=0;
		do{
			if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
				HMODULE lib=LoadLibrary(fd.cFileName);
				if(GetProcAddress(lib, "visInit")){
					int len= lstrlen(fd.cFileName)-4;
					if(len>0){
						TCHAR *s= new TCHAR[len+1];
						memcpy(s, fd.cFileName, len*sizeof(TCHAR));
						s[len]='\0';
						visualNames[Nvisuals++]=s;
					}
				}
				FreeLibrary(lib);
			}
		} while(FindNextFile(h, &fd) && Nvisuals<MAXVISUALS);
		FindClose(h);
	}
}

void visualSetPriority()
{
	aminmax(visualPriority, 0, sizeA(threadPriorCnst)-1);
	SetThreadPriority(paintThread, threadPriorCnst[visualPriority]);
}

//create filter, working thread and visualization window
void visualInit()
{
	InitializeCriticalSection(&critSect);
	startEvent= CreateEvent(0, TRUE, FALSE, 0);
	paintThread = CreateThread(0, 0, paintProc, 0, 0, &paintThreadId);
	WaitForSingleObject(startEvent, INFINITE);
	CloseHandle(startEvent);
	visualSetPriority();
	addVisual();
}

//delete filter, working thread and visualization window
void visualShut()
{
	freeVisual();
	if(paintThread){
		DestroyWindow(visualWnd);
		PostThreadMessage(paintThreadId, WM_QUIT, 0, 0);
		WaitForSingleObject(paintThread, INFINITE);
		CloseHandle(paintThread);
	}
	DeleteCriticalSection(&critSect);
	delete[] pBuffer;
}

