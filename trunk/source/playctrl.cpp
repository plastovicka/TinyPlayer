/*
	(C) 2004-2011  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include <initguid.h>
#include <qnetwork.h>
#include <dvdmedia.h>
#include "tinyplay.h"
#include "sound.h"
#include "draw.h"
#include "winamp.h"
#include "dvd.h"

const int fullDiv=10; //fullscreen stretch tolerance 
long volume=1;  //DirectX volume before fadeout, 1 means NULL
bool isMPEG2, isAvi;
bool isCD, isDVD;
int audioStream;      //current audio stream number
BOOL screenSaver, screenSaver1;
double seekCorrection;
LONGLONG posCorrection;
HANDLE infoThread;

//---------------------------------------------------------------------------
struct Tdword
{
	Tdword *nxt, *prv;
	DWORD i;

	Tdword(){ nxt=prv=this; }
	~Tdword(){ prv->nxt= nxt; nxt->prv= prv; }
	void append(Tdword *item);
	int isEmpty(){ return nxt==this; }
};

void Tdword::append(Tdword *item)
{
	prv->nxt= item;
	item->prv= prv;
	prv= item;
	item->nxt= this;
}

#define forl2(T,l) for(T *item=(T*)(l).nxt;\
	(Tdword*)item!=&(l); item=(T*)item->nxt)

Tdword propertyThreadId;

//------------------------------------------------------------------
void playBtn(int cmd)
{
	if(state==cmd) return;
	SendMessage(toolbar, TB_CHECKBUTTON, state, MAKELONG(FALSE, 0));
	SendMessage(toolbar, TB_CHECKBUTTON, cmd, MAKELONG(TRUE, 0));
	state=cmd;
}

bool processMessages()
{
	MSG mesg;
	while(PeekMessage(&mesg, 0, 0, 0, PM_REMOVE)){
		if(mesg.hwnd==videoWnd ||
			!TranslateAccelerator(hWin, haccel, &mesg)){
			if(mesg.message==WM_QUIT){
				PostQuitMessage(0);
				return true;
			}
			TranslateMessage(&mesg);
			DispatchMessage(&mesg);
		}
	}
	return false;
}

void rdAVI(TCHAR *fn)
{
	HANDLE f=CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if(f!=INVALID_HANDLE_VALUE){
		DWORD buf[32];
		DWORD r;
		ReadFile(f, buf, sizeof(buf), &r, 0);
		if(buf[2]==MAKEFOURCC('A', 'V', 'I', ' ') && r>=36
				&& buf[6]==MAKEFOURCC('a', 'v', 'i', 'h')){
			DWORD f= buf[8];
			if(f>1000 && f<5000000) frameRate=1000000/(double)f;
			isAvi=true;
		}
		else if(buf[0]==MAKEFOURCC('O', 'g', 'g', 'S')){
			char *s=((char*)buf)+28;
			if(!strncmp(s, "\1vorbis", 7)) s+=58;
			if(!strncmp(s, "\1video\0\0\0", 9)){
				LONGLONG d= *(LONGLONG*)(s+17);
				if(d>10000 && d<100000000) frameRate= UNITS/(double)d;
			}
		}
		CloseHandle(f);
	}
}

//------------------------------------------------------------------
//returns fullscreen window rectangle and sets scrW,scrH
void getScreen(RECT *rc, RECT *rcWork, int videoLeft, int videoTop, int videoWidth, int videoHeight)
{
	int vx, vy;
	bool ok=false;

	vx=vy=0;
	if(multiMonitors()){
		RECT rcd;
		SetRect(&rcd, videoLeft, videoTop, videoLeft + videoWidth, videoTop + videoHeight);
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		if(getMonitorInfo(monitorFromRect(&rcd, MONITOR_DEFAULTTONEAREST), &mi)){
			vx = mi.rcMonitor.left;
			vy = mi.rcMonitor.top;
			scrW = mi.rcMonitor.right-vx;
			scrH = mi.rcMonitor.bottom-vy;
			if(rcWork) CopyRect(rcWork, &mi.rcWork);
			ok=true;
		}
	}
	if(!ok && rcWork) SystemParametersInfo(SPI_GETWORKAREA, 0, rcWork, 0);
	SetRect(rc, vx, vy, scrW, scrH);
}

void resizeVideo(int newWidth)
{
	int dh, dw;
	RECT rcd, rcs;

	if(isVideo){
		//get screen rectangle
		getScreen(&rcs, &rcd, videoLeft, videoTop, videoWidth, videoHeight);
		//calculate new video size according to ratio
		if(newWidth<0) newWidth = min(scrW, scrH*ratio1/ratio2);
		videoWidth= newWidth;
		aminmax(videoWidth, minW, 2*scrW);
		videoHeight = videoWidth * ratio2 / ratio1;
		if(videoHeight>2*scrH){
			videoHeight=2*scrH;
			videoWidth = videoHeight * ratio1 / ratio2;
		}
		if(videoHeight<minH){
			videoHeight=minH;
			videoWidth = videoHeight * ratio1 / ratio2;
		}
		resizing++;
		LONG style=GetWindowLong(videoWnd, GWL_STYLE);

		if(fullscreen){
			SetWindowLong(videoWnd, GWL_STYLE, style&=~WS_THICKFRAME);
			//stretch to full size
			dh=abs(scrH-videoHeight);
			dw=abs(scrW-videoWidth);
			if(dw==1 || !forceOrigRatio && dh<2 && dw<scrW/fullDiv){
				videoWidth=scrW;
			}
			if(dh==1 || !forceOrigRatio && dw<2 && dh<scrH/fullDiv){
				videoHeight=scrH;
			}
			//set top-left corner client coordinates (usually [0,0])
			videoFrameLeft=(scrW-videoWidth)/2;
			videoFrameTop=(scrH-videoHeight)/2;

			SetWindowPos(videoWnd, HWND_TOPMOST, rcs.left, rcs.top, rcs.right, rcs.bottom, SWP_NOCOPYBITS);
			InvalidateRect(videoWnd, 0, TRUE);
			winOnFull=false;
			if(!pDDXM){///
				pVW->SetWindowPosition(0, 0, videoWidth, videoHeight);
			}
		}
		else{
			SetWindowLong(videoWnd, GWL_STYLE, style|=WS_THICKFRAME);
			int borderW, borderH, border2W, border2H;
			borderW= GetSystemMetrics(SM_CXSIZEFRAME)-1;
			border2W=2*borderW;
			borderH= GetSystemMetrics(SM_CYSIZEFRAME)-1;
			border2H=2*borderH;

			//move up or left if the window is outside working area
			rcd.bottom -= videoHeight+border2H;
			rcd.top -= borderH;
			int top=videoTop;
			if(top > rcd.bottom){
				top= rcd.bottom;
				if(top<rcd.top) top=rcd.top;
			}
			rcd.right -= videoWidth+border2W;
			rcd.left -= borderW;
			int left=videoLeft;
			if(left > rcd.right){
				left= rcd.right;
				if(left<rcd.left) left=rcd.left;
			}

			if(!pDDXM){///
				pVW->put_Width(videoWidth);
				pVW->put_Height(videoHeight);
			}
			videoFrameLeft=videoFrameTop=0;
			SetWindowPos(videoWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE);
			SetWindowPos(videoWnd, HWND_NOTOPMOST, left, top,
				videoWidth + border2W, videoHeight + border2H, SWP_NOACTIVATE);
			videoW=videoWidth;
		}
		if(!pDDXM){
			//if(!fullscreen) pVW->put_FullScreenMode(fullscreen ? OATRUE : OAFALSE);
			pVW->SetWindowPosition(videoFrameLeft, videoFrameTop, videoWidth, videoHeight);
			//if(fullscreen) pVW->put_FullScreenMode(fullscreen ? OATRUE : OAFALSE);
		}
		else{
			onVideoMoved();
		}
		if(subtitles.on){
			InvalidateRect(videoWnd, 0, FALSE);
		}
		resizing--;
	}
	checkSizeMenu();
}

void resizeVideo(int nMultiplier, int nDivider)
{
	resizeVideo(origW  * nMultiplier / nDivider);
}

void sizeChanged()
{
	if(isVideo){
		resizeVideo(fullscreen ? -1 : videoW);
	}
}

bool onVideoMoved()
{
	if(pDDXM){
		RECT rc;
		POINT p;
		p.x=p.y=0;
		ClientToScreen(videoWnd, &p);
		rc.left= p.x + videoFrameLeft;
		rc.right= rc.left+videoWidth;
		rc.top= p.y + videoFrameTop;
		rc.bottom= rc.top+videoHeight;
		OffsetRect(&rc, -monitorOffset.x, -monitorOffset.y);
		pDDXM->SetDrawParameters(NULL, &rc);
		return multiMonitors() && (rc.right<=0 || rc.bottom<=0 || rc.left>=scrW || rc.top>=scrH);
	}
	return false;
}

void blackSides(HDC dc)
{
	if(!fullscreen) return;
	RECT rc;
	rc.bottom=scrH;
	rc.right=scrW;
	rc.left=rc.top=0;
	HBRUSH br= (HBRUSH)GetStockObject(BLACK_BRUSH);
	if(videoHeight<scrH){
		rc.bottom=videoFrameTop;
		FillRect(dc, &rc, br);
		rc.top=rc.bottom+videoHeight;
		rc.bottom=scrH;
		FillRect(dc, &rc, br);
	}
	if(videoWidth<scrW){
		rc.top=videoFrameTop;
		rc.bottom=rc.top+videoHeight;
		rc.right=videoFrameLeft;
		FillRect(dc, &rc, br);
		rc.left=rc.right+videoWidth;
		rc.right=scrW;
		FillRect(dc, &rc, br);
	}
}

void showVisual()
{
	if(isVisual && visualData && visualFilter){
		LONG style=GetWindowLong(visualWnd, GWL_STYLE);
		if(fullscreen && !isVideo){
			SetWindowLong(visualWnd, GWL_STYLE, style&=~WS_THICKFRAME);
			RECT rc;
			getScreen(&rc, 0, visualLeft, visualTop, visualW, visualH);
			SetWindowPos(visualWnd, HWND_TOPMOST, rc.left, rc.top, rc.right, rc.bottom, SWP_SHOWWINDOW|SWP_ASYNCWINDOWPOS);
			winOnFull=false;
		}
		else{
			SetWindowLong(visualWnd, GWL_STYLE, style|=WS_THICKFRAME);
			SetWindowPos(visualWnd, HWND_NOTOPMOST, visualLeft, visualTop, visualW, visualH, SWP_SHOWWINDOW|SWP_NOACTIVATE|SWP_ASYNCWINDOWPOS);
		}
	}
	else{
		ShowWindow(visualWnd, SW_HIDE);
	}
	checkSizeMenu();
}

void setFullScreen(int b)
{
	fullscreen=b;
	showVisual();
	sizeChanged();
	InvalidateRect(videoWnd, 0, TRUE);
	setMouse();
}

void ratio(int rx, int ry)
{
	ratio1=rx; ratio2=ry;
	sizeChanged();
}

//------------------------------------------------------------------
bool getPosition()
{
	if(isGrabbingWAV && visualFilter && !audioFilter){
		position= visualFilter->EndSample;
		return true;
	}
	if(isCD){
		return cdPos();
	}
	if(waAudioFilter){
		position= Winamp::streamTime();
		return true;
	}
	if(isDVD) return true;
	if(!pMS || !SUCCEEDED(pMS->GetCurrentPosition(&position))) return false;
	position -= posCorrection;
	return true;
}

void fadeIn(int b)
{
	if(pBA){
		if(volume==1) pBA->get_Volume(&volume);
		if(b && state!=ID_FILE_STOP){
			for(long i=-8000; i<-100 && i<volume; i>>=1){
				pBA->put_Volume(i);
				Sleep(80);
			}
		}
		pBA->put_Volume(volume);
		volume=1;
	}
}

void fadeOut(int b)
{
	if(pBA){
		if(volume==1) pBA->get_Volume(&volume);
		if(b){
			for(long i=min(-1000, volume); i>-5000; i<<=1){
				pBA->put_Volume(i);
				Sleep(60);
			}
		}
		pBA->put_Volume(-10000);
	}
}

bool seek(LONGLONG pos)
{
	bool b, err;
	LONGLONG pos2;

	b= smoothSeek && state==ID_FILE_PLAY;
	if(pos<=0){
		pos= isMPEG2 && pDDXM ? 400000 : 0;///
	}
	if(b) fadeOut(true);
	else noStop++;
	if(cdAudioFilter){
		err= ripSeek((LONG)(pos/133333));
	}
	else if(isCD){
		err= cdPlay(MCI_MAKE_TMSF(0, pos/(60*UNITS),
			(pos/UNITS)%60, (pos%UNITS)/(UNITS/75)))!=0;
	}
	else if(waAudioFilter){
		err= Winamp::seek(pos);
	}
	else if(isDVD){
		err= dvdSeek(pos);
	}
	else{
		err=true;
		if(pMS){
			DWORD cap = AM_SEEKING_CanSeekAbsolute;
			if(pMS->CheckCapabilities(&cap)==S_OK){
				pos2=pos;
				if(seekCorrection){
					pos2=(LONGLONG)(pos*seekCorrection);
					posCorrection=pos2-pos;
				}
				err= FAILED(pMS->SetPositions(&pos2,
					AM_SEEKING_AbsolutePositioning, 0, AM_SEEKING_NoPositioning));
			}
		}
	}
	if(b) fadeIn(true);
	else noStop--;
	if(err) return false;
	position=pos;
	repaintTime(0);
	subtitles.reset();
	return true;
}

void seekRel(int sec)
{
	if(getPosition()){
		seek(position+LONGLONG(sec)*UNITS);
	}
}

void restart()
{
	if(!seek(0) && pMC){
		pMC->Stop();
		pMC->Run();
	}
}

BOOL CALLBACK enumWin(HWND hWnd, LPARAM)
{
	DWORD id, pid;

	id=GetWindowThreadProcessId(hWnd, &pid);
	forl2(Tdword, propertyThreadId){
		if(item->i==id){
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		}
	}
	return TRUE;
}

//------------------------------------------------------------------
void mediaInfo(IBaseFilter *pFilter, void *)
{
	IEnumPins *pEnumPins;
	IPin *pPin;
	PIN_DIRECTION pd;
	DWORD x, y;
	AM_MEDIA_TYPE type;

	pFilter->EnumPins(&pEnumPins);
	while(pEnumPins->Next(1, &pPin, NULL)==S_OK){
		pPin->QueryDirection(&pd);
		if(pd == PINDIR_OUTPUT){
			if(SUCCEEDED(pPin->ConnectionMediaType(&type))){
				if(type.formattype == FORMAT_MPEG2_VIDEO){
					MPEG2VIDEOINFO *info = (MPEG2VIDEOINFO *)type.pbFormat;
					x=info->hdr.dwPictAspectRatioX;
					y=info->hdr.dwPictAspectRatioY;
					if(x && y){
						ratio1=x;
						ratio2=y;
					}
				}
				FreeMediaType(type);
			}
		}
		pPin->Release();
	}
	pEnumPins->Release();
}

// enumerate all filters in the graph
void EnumFilters(void(*f)(IBaseFilter*, void*), void *param)
{
	IEnumFilters *pEnum;
	IBaseFilter *pFilter;

	if(pGB && SUCCEEDED(pGB->EnumFilters(&pEnum))){
		while(pEnum->Next(1, &pFilter, 0) == S_OK){
			f(pFilter, param);
			pFilter->Release();
		}
		pEnum->Release();
	}
}

void releaseGB(IGraphBuilder *&g)
{
	SAFE_RELEASE(g);
}

void stopCD()
{
	if(mciPlaying){
		mciPlaying=false;
		MCI_GENERIC_PARMS mgp;
		mciSendCommand(cdDevice, MCI_STOP, 0, (WPARAM)&mgp);
	}
}

void CloseClip1(bool stop)
{
	if(!stop) noStop++;
	//restore volume
	if(volume!=1){
		if(pMC) pMC->Pause();
		fadeIn(0);
	}
	//stop all filters
	if(pMC) pMC->Stop();
	if(waAudioFilter){
		Winamp::stop();
		RELEASE(waAudioFilter);
	}
	ripShut();
	if(!stop) noStop--;
	// hide video window
	if(pVW){
		pVW->put_Visible(OAFALSE);
		pVW->put_Owner(NULL);
	}
	if(pDDXM) pDDXM->SetCallbackInterface(NULL, 0);
	if(pME) pME->SetNotifyWindow((OAHWND)NULL, 0, 0);
	KillTimer(visualWnd, 1);
	// close all property pages
	if(!propertyThreadId.isEmpty()){
		EnumWindows((WNDENUMPROC)enumWin, 0);
	}
	// Release and zero DirectShow interfaces
	SAFE_RELEASE(pColor);
	SAFE_RELEASE(pDDXM);
	SAFE_RELEASE(pBV);
	if(pBA){
		RELEASE(pBA);
	}
	SAFE_RELEASE(pVW);
	SAFE_RELEASE(pME);
	SAFE_RELEASE(pMS);
	SAFE_RELEASE(pMP);
	SAFE_RELEASE(pMC);
	EnterCriticalSection(&critSect);
	SAFE_RELEASE(visualFilter);
	LeaveCriticalSection(&critSect);
	releaseGB(pGB);
	if(isVideo){
		subtitles.close();
		SAFE_RELEASE(pOverlayCallback);
		SAFE_RELEASE(pPrimary);
		SAFE_RELEASE(pDDObject);
	}
	if(wavFile!=INVALID_HANDLE_VALUE){
		DWORD d, w;
		d=GetFileSize(wavFile, 0)-8;
		SetFilePointer(wavFile, 4, 0, FILE_BEGIN);
		WriteFile(wavFile, &d, 4, &w, 0);
		d-=20+wavFmtSize;
		SetFilePointer(wavFile, 24+wavFmtSize, 0, FILE_BEGIN);
		WriteFile(wavFile, &d, 4, &w, 0);
		CloseHandle(wavFile);
		wavFile=INVALID_HANDLE_VALUE;
	}
	if(isDVD) dvdClose();
	isVideo= false;
	isCD= false;
	isGrabbingWAV=false;
	playBtn(ID_FILE_STOP);
}

HRESULT renderFile(IGraphBuilder *pGB, TCHAR *fileName)
{
	if(!audioFilter) enumAudioRenderers(2, pGB);
	HRESULT hr=S_OK;
	VisualizationFilter *visFilter= audioFilter ? new AudioFilter(&hr) : new VisualizationFilter(&hr);
	if(hr!=S_OK){
		delete visFilter;
		visFilter=0;
	}
	else{
		pGB->AddFilter(visFilter, VISUAL_FILTER_NAMEW);
	}
	if(!audioFilter) AudioFilter::stop();

	if(isDVD){
		hr= dvdRender(pGB, fileName);
	}
	else{
		convertT2W(fileName, wFile);
		hr= pGB->RenderFile(wFile, 0);
	}
	if(visFilter && !visFilter->m_pInput->IsConnected()){
		pGB->RemoveFilter(visFilter);
	}
	return hr;
}

void beginPlay()
{
	Titem *item= &playlist[currentFile];
	TCHAR *fn= item->file;
	LONGLONG duration;
	HRESULT hr;
	int t;
	IGraphBuilder *oldGB=0;
	IMediaControl *oldMC=0;
	int oldState=state;
	bool oldIsVideo=isVideo;

	t=getCDtrack();
	if(isDVD && dvdItem(item)){
		if(t==0){
			SendMessage(hWin, WM_COMMAND, ID_DVD_MENU_ROOT, 0);
		}
		else{
			SetTitle(t);
		}
		return;
	}
	if(crossfade && !isVideo && !AudioFilter::playing){
		oldGB=pGB;
		oldMC=pMC;
		pGB=0;
		pMC=0;
	}
	if(state!=ID_FILE_STOP){
		CloseClip1(false);
	}
	else{
		SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &screenSaver1, 0);
		screenSaver=screenSaver1;
		playbackRate= defaultSpeed/1000.0;
	}
	isMPEG2=false;
	isAvi=false;
	isVideo=false;
	position=0;
	seekCorrection=0;
	posCorrection=0;
	audioStream=0;
	if(isCDA(item)){
		isCD=true;
		if(!cdNext || cdNext!=getCDtrack()){
			MCIERROR e, e1;
			e= e1= openCD(fn[0]);
			if(!e){
				if(cdda){
					stopCD();
					e= ripStart();
				}
				else{
					AudioFilter::waitForEnd();
					e= cdPlay(0);
					cdStartTime= GetTickCount();
				}
			}
			if(e){
				state=ID_FILE_PAUSE;
				CloseClip();
				if(oldMC){ oldMC->Stop(); oldMC->Release(); }
				if(e1){
					if(e1==MCIERR_INVALID_FILE){
						msglng(866, "%c: is not a CD drive letter", fn[0]);
					}
					else{
						cdErr(e1);
					}
				}
				releaseGB(oldGB);
				return;
			}
		}
	}
	else{
		closeCD();
		if(fn==gaplessFile && pNextGB){
			pGB=pNextGB;
			pNextGB=0;
			gaplessFile=0;
		}
		else{
			//create FilterGraph and render the file
			if(isIFO(item)){
				if(FAILED(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
					IID_IGraphBuilder, (void **)&pGB))){
					msg("DirectX FilterGraph interface not found");
					return;
				}
				if(FAILED(CoCreateInstance(CLSID_DVDNavigator, NULL, CLSCTX_INPROC_SERVER,
					IID_IDvdControl2, (void **)&pDvdControl))){
					RELEASE(pGB);
					msg("DVD Navigator interface not found");
					return;
				}
				isDVD=true;
				InsertMenu(GetMenu(hWin), MENU_DVD, MF_BYPOSITION|MF_POPUP, (UINT)menuDVD, lng(412, _T("&DVD")));
				DrawMenuBar(hWin);
				pDvdControl->QueryInterface(IID_IDvdInfo2, (void **)&pDvdInfo);
				pGB->QueryInterface(IID_IMediaEventEx, (void **)&pME);
				hr=renderFile(pGB, fn);
				if(SUCCEEDED(hr)){
					SetDVDPlaybackOptions();
					SetFocus(videoWnd);
					if(t && !noChangeTitle) PostMessage(hWin, WM_COMMAND, t+ID_DVD_TITLE_BASE, 0);
				}
			}
			else{
				if(FAILED(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
					IID_IGraphBuilder, (void **)&pGB))){
					msg("DirectShow FilterGraph interface not found");
					return;
				}
				hr=renderFile(pGB, fn);
			}
			if(FAILED(hr)){
				if(Winamp::open(item, 0)){
					releaseGB(pGB);
				}
				else{
					state=ID_FILE_PAUSE;
					CloseClip();
					if(oldMC){ oldMC->Stop(); oldMC->Release(); }
					releaseGB(oldGB);
					if(hr==ERROR_FILE_NOT_FOUND || hr==ERROR_PATH_NOT_FOUND ||
						hr==ERROR_BAD_NETPATH || hr==VFW_E_NOT_FOUND){
						msglng(739, "File not found: %s", fn);
					}
					else if(hr==VFW_E_NO_AUDIO_HARDWARE){
						msglng(789, "Audio device failed (is it used by another application ?)");
					}
					else if(hr==VFW_E_DVD_INVALID_DISC && !_tcscmp(fn, dvdFirstPlayName)){
						msglng(868, "There is no DVD in the drive");
					}
					else{
						msglng(780, "Cannot play %s", fn);
					}
					return;
				}
			}
		}
		if(pGB){
			if(!pME) pGB->QueryInterface(IID_IMediaEventEx, (void **)&pME);
			pGB->QueryInterface(IID_IMediaControl, (void **)&pMC);
			pGB->QueryInterface(IID_IMediaPosition, (void **)&pMP);
			pGB->QueryInterface(IID_IMediaSeeking, (void **)&pMS);
			pGB->QueryInterface(IID_IBasicAudio, (void **)&pBA);
			pGB->QueryInterface(IID_IVideoWindow, (void **)&pVW);
			pGB->QueryInterface(IID_IBasicVideo, (void **)&pBV);
			if(pVW && pBV){
				long lVisible;
				if(SUCCEEDED(pVW->get_Visible(&lVisible))){
					if(pBV->GetVideoSize(&origW, &origH)!= E_NOINTERFACE){
						isVideo = true;
						ratio1=origW;
						ratio2=origH;
						if(origW==480 && origH==576){ //MPEG-2
							ratio1=768;
						}
						EnumFilters(mediaInfo, 0);
						frameRate=0;
						REFTIME t;
						pBV->get_AvgTimePerFrame(&t);
						if(t) frameRate= 1/t;
						else rdAVI(fn);
						if(subtitles.file) subtitles.reset();
						if(subtAutoLoad) findSubtitles();
						AddOvMToGraph();
					}
				}
			}
			// Have the graph signal events via window messages
			pME->SetNotifyWindow((OAHWND)hWin, WM_GRAPHNOTIFY, 0);
		}
	}

	if(isVideo){
		if(oldState==ID_FILE_STOP || oldIsVideo==false) fullscreen=startFullScreen;
		if(pVW){
			pVW->put_Owner((OAHWND)videoWnd);
			pVW->put_WindowStyle(WS_CHILD);
		}
		sizeChanged();
		if(pVW){
			pVW->put_MessageDrain((OAHWND)videoWnd);
			setMouse();
		}
		Sleep(50); //MPEG-4 need this, but I don't know why
	}
	else{
		if(oldState==ID_FILE_STOP || oldIsVideo==true) fullscreen=visualStartFullScreen;
		if(GetActiveWindow()==videoWnd) SetActiveWindow(hWin);
		ShowWindow(videoWnd, SW_HIDE);
	}
	if(setThreadExecutionState){
		setThreadExecutionState(isVideo ? ES_DISPLAY_REQUIRED|ES_SYSTEM_REQUIRED|ES_CONTINUOUS : ES_SYSTEM_REQUIRED|ES_CONTINUOUS);
	}
	int screenSaver2= isVideo ? FALSE : audioSaver;
	if(screenSaver1!=screenSaver2){
		SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, screenSaver1=screenSaver2, 0, 0);
	}

	double r=playbackRate;
	playbackRate=1;
	setRate(r);

	IBaseFilter *visFilter=0;
	if(pGB && SUCCEEDED(pGB->FindFilterByName(VISUAL_FILTER_NAMEW, &visFilter))){
	}
	else if(cdAudioFilter){
		cdAudioFilter->AddRef();
		visFilter= cdAudioFilter;
	}
	else if(waAudioFilter){
		waAudioFilter->AddRef();
		visFilter= waAudioFilter;
	}
	else{
		AudioFilter::waitForEnd();
	}

	// run the graph to play the media file
	if(pMC) pMC->Run();
	if(waAudioFilter) waAudioFilter->Run(0);
	if(volume!=1) fadeIn(0);
	playBtn(ID_FILE_PLAY);
	stopAfterCurrent=false;
	checkSoundMenu();
	if(crossfade){
		if(oldMC){ oldMC->Stop(); oldMC->Release(); }
		releaseGB(oldGB);
	}
	//previous song must be stopped before calling allocBuffer,
	//  because pBuffer is a global array
	if(visFilter){
		visualFilter= (VisualizationFilter*)visFilter;
		visualFilter->allocBuffer(visualData);
	}
	enumFilters();
	if(isVideo){
		SetCursor(0); //hide hour glass at startup
		ShowWindow(videoWnd, SW_SHOW);
	}
	t=tick;
	if(isCD && !cdda && !t) t=240;
	if(t) SetTimer(hWin, 1000, t, NULL);

	//get length and bitrate
	item->getSize();
	if(pMS && SUCCEEDED(pMS->GetDuration(&duration)) ||
		isDVD && dvdDuration(&duration) ||
		waAudioFilter && Winamp::getDuration(&duration)){
		bool m=false;
		if(!item->length) m=mp3Duration(item);
		if(!item->length || item->length>duration-10*UNITS
			&& item->length<duration+10*UNITS && !m){
			item->length=duration;
		}
		else{
			seekCorrection=double(duration)/item->length;
		}
		item->calcBitrate();
	}

	if(isVisual && visualData){
		//don't show audio data from an empty buffer 
		processMessages();
		Sleep(400);
		SetTimer(visualWnd, 1, visualTimer, 0);
	}
	showVisual();
	if(isCD){
		writeini1(); //prevent harddisk from going to sleep mode
	}
}

//read length and bitrate of the item
int getInfo(Titem *item)
{
	int result=0;
	if(!item->length && !isCDA(item) && !isIFO(item)){
		//get length
		if(mp3Duration(item)){
			result=1;
		}
		else{
			IGraphBuilder *pGB;
			IMediaSeeking *pMS;
			if(SUCCEEDED(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
				IID_IGraphBuilder, (void **)&pGB))){
				convertT2W(item->file, wFile);
				if(SUCCEEDED(pGB->RenderFile(wFile, 0))){
					pGB->QueryInterface(IID_IMediaSeeking, (void **)&pMS);
					if(pMS){
						if(SUCCEEDED(pMS->GetDuration(&item->length))){
							result=1;
						}
						pMS->Release();
					}
				}
				releaseGB(pGB);
			}
			if(!result){
				Winamp::lock++;
				result= Winamp::open(item, 1);
				Winamp::lock--;
			}
		}
		if(!item->length) result=0;
		if(result){
			item->getSize();
			item->calcBitrate();
		}
	}
	return result;
}

DWORD __stdcall infoProc(void* p)
{
	int i, j, last, top, inval;
	HWND tray;
	Titem *item, tmpItem;

	EnterCriticalSection(&infoCritSect);
	i=0;
	top=0;
	last=playlist.len-1;
	inval=-1;
	//disable task bar updates
	tray=FindWindow(_T("Shell_TrayWnd"), 0);
	LockWindowUpdate(tray);

	for(;;){
		if(i>=playlist.len){
			i=0;
			if(!playlist.len) break;
			if(last>=playlist.len) last=playlist.len-1;
		}
		//copy playlist item to temporary item
		item=&playlist[i];
		tmpItem.init(item->file, 0);
		tmpItem.length=item->length;
		tmpItem.size=item->size;
		LeaveCriticalSection(&infoCritSect);
		if(inval>=0){
			invalidateItem(inval);
			inval=-1;
		}
		if(getInfo(&tmpItem)){
			j=SendMessage(listbox, LB_GETTOPINDEX, 0, 0);
			EnterCriticalSection(&infoCritSect);
			//copy info to playlist item
			if(i<playlist.len && item==&playlist[i] &&
				!_tcscmp(tmpItem.file, item->file)){
				item->free();
				memcpy(item, &tmpItem, sizeof(Titem));
				inval=i;
			}
			else{
				tmpItem.free();
			}
			//move to the first visible item
			if(j!=top && j>=0){
				top=j;
				i=j-1;
				if(i<0) i=playlist.len-1;
			}
			last=i;
		}
		else{
			EnterCriticalSection(&infoCritSect);
			tmpItem.free();
			if(i==last) break;
		}
		i++;
	}
	LockWindowUpdate(0);
	CloseHandle(infoThread);
	infoThread=0;
	LeaveCriticalSection(&infoCritSect);
	return 0;
}

//read lengths of all items
void allInfo()
{
	if(!infoThread){
		DWORD id;
		infoThread=CreateThread(0, 0, infoProc, 0, 0, &id);
		SetThreadPriority(infoThread, THREAD_PRIORITY_LOWEST);
	}
}

//------------------------------------------------------------------
void PlayClip()
{
	if(state == ID_FILE_STOP){
		gotoFile(currentFile);
	}
	else{
		if(cdAudioFilter){
			if(FAILED(cdAudioFilter->Run(0))) return;
		}
		else if(isCD){
			getPosition();
			if(!seek(position)) return;
		}
		else if(waAudioFilter){
			if(!Winamp::run()) return;
		}
		else{
			if(!pMC || FAILED(pMC->Run())) return;
		}
		fadeIn(smoothPause);
		playBtn(ID_FILE_PLAY);
	}
}

bool PauseClip()
{
	fadeOut(smoothPause);
	if(cdAudioFilter){
		return SUCCEEDED(cdAudioFilter->Pause());
	}
	if(isCD){
		return !mciSendCommand(cdDevice, MCI_PAUSE, 0, 0);
	}
	if(waAudioFilter){
		return Winamp::pause();
	}
	return pMC && SUCCEEDED(pMC->Pause());
}

void pause()
{
	if(state == ID_FILE_PAUSE){
		PlayClip();
	}
	else if(state == ID_FILE_STOP){
		noPlay++;
		PlayClip();
		if(cdAudioFilter && SUCCEEDED(cdAudioFilter->Stop()) ||
			waAudioFilter && Winamp::pause() ||
			pMC && SUCCEEDED(pMC->Stop())){
			playBtn(ID_FILE_PAUSE);
		}
		noPlay--;
	}
	else if(PauseClip()){
		playBtn(ID_FILE_PAUSE);
	}
}

void StepFrame()
{
	IVideoFrameStep *pFS;

	if(!isVideo){
		pause();
		return;
	}
	if(state != ID_FILE_PAUSE) pause();
	if(pGB){
		if(FAILED(pGB->QueryInterface(__uuidof(IVideoFrameStep), (PVOID *)&pFS))){
			msglng(783, "This function is not available.\nTry to install DirectX 9.");
		}
		else{
			pFS->Step(1, NULL);
			pFS->Release();
		}
	}
}

void CloseClip2()
{
	releaseGB(pNextGB);
	closeCD();
	position=0;
	KillTimer(hWin, 1000);
	HWND a= GetActiveWindow();
	if(a==videoWnd || a==visualWnd) SetActiveWindow(hWin);
	ShowWindow(videoWnd, SW_HIDE);
	ShowWindow(visualWnd, SW_HIDE);
	winOnFull=false;
	invalidateTime();
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, screenSaver, 0, 0);
	if(setThreadExecutionState) setThreadExecutionState(ES_CONTINUOUS);
	AudioFilter::shut();
	Winamp::unloadPlugins();
	if(ddrawDll){
		FreeLibrary(ddrawDll);
		ddrawDll=0;
	}
}


void CloseClip()
{
	if(state==ID_FILE_STOP) return;
	CloseClip1(true);
	CloseClip2();
}

void CloseClipEnd()
{
	if(state==ID_FILE_STOP) return;
	CloseClip1(false);
	AudioFilter::waitForEnd();
	CloseClip2();
}

void nextClip()
{
	if(stopAfterCurrent || repeat==3){
		CloseClipEnd();
	}
	else if(repeat==2){
		gotoFile(currentFile);
	}
	else if(currentFile==playlist.len-1 && !repeat){
		CloseClipEnd();
	}
	else{
		gotoFile(currentFile+1);
	}
}

//------------------------------------------------------------------
void setRate(double rate)
{
	if(!rate) rate=1;
	if((!pMC && !isCD && !waAudioFilter ||
		(audioFilter ? SUCCEEDED(AudioFilter::setRate(rate)) :
		(isDVD ? dvdRate(rate) : pMP && SUCCEEDED(pMP->put_Rate(rate)))))){
		playbackRate = rate;
	}
	checkRateMenu();
}


void modifyRate(double incr)
{
	double r = playbackRate + incr;
	if(r>0) setRate(r);
}

//------------------------------------------------------------------
void HandleGraphEvent()
{
	LONG evCode, evParam1, evParam2;
	HRESULT hr;

	if(!pME) return;

	// Process all queued events
	while(SUCCEEDED(pME->GetEvent(&evCode, (LONG_PTR*)&evParam1,
			(LONG_PTR*)&evParam2, 0))){

		if(isDVD) OnDvdEvent(evCode, evParam1, evParam2);

		// Free memory associated with callback, since we're not using it
		hr = pME->FreeEventParams(evCode, evParam1, evParam2);

		if(evCode == EC_COMPLETE){
			/*Titem *item= &playlist[currentFile];
			if(position && tick
			&& (item->length > position + (tick+dsBufferSize)*10010 ||
			item->length < position)){
			item->length= position;
			item->calcBitrate();
			invalidateItem(currentFile);
			}*///
			nextClip();
			break;
		}
		if(evCode==EC_ERRORABORT){
			CloseClip();
			break;
		}
		/*if(evCode==EC_VIDEO_SIZE_CHANGED){
			long x,y;
			DWORD w,h;
			if(pDDXM){
			pDDXM->GetNativeVideoProps(&w,&h,(DWORD*)&x,(DWORD*)&y);
			}else{
			pBV->get_SourceWidth(&x);
			pBV->get_SourceHeight(&y);
			}///
			sizeChanged();
			}*/
	}
}

//------------------------------------------------------------------
DWORD WINAPI propertyProc(LPVOID param)
{
	IBaseFilter *pFilter= (IBaseFilter*)param;
	ISpecifyPropertyPages *pSpecify;

	// Discover if this filter contains a property page
	if(SUCCEEDED(pFilter->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pSpecify))){
		FILTER_INFO FilterInfo;
		if(SUCCEEDED(pFilter->QueryFilterInfo(&FilterInfo))){
			CAUUID caGUID;
			if(SUCCEEDED(pSpecify->GetPages(&caGUID))){
				Tdword *item = new Tdword;
				item->i = GetCurrentThreadId();
				propertyThreadId.append(item);
				// Display the filter's property page
				OleCreatePropertyFrame(
						0/*hWin*/, 0, 0,
						FilterInfo.achName,     // Caption for the dialog box
						1,                      // Number of filters
						(IUnknown **)&pFilter,  // Pointer to the filter 
						caGUID.cElems,          // Number of property pages
						caGUID.pElems,          // Pointer to property page CLSIDs
						0,                      // Locale identifier
						0, NULL);
				CoTaskMemFree(caGUID.pElems);
				delete item;
			}
			FilterInfo.pGraph->Release();
		}
		pSpecify->Release();
	}
	pFilter->Release();
	return 0;
}

void filterProperty(TCHAR *name)
{
	if(!_tcscmp(name, VISUAL_FILTER_NAME)){
		visualProperty(1);
	}
	else if(!_tcscmp(name, AUDIO_FILTER_NAME)){
		audioProperty(1);
	}
	else if(waAudioFilter && !_tcscmp(name, Winamp::menuName)){
		Winamp::lock++;
		Winamp::in->Config(hWin);
		Winamp::lock--;
		Winamp::unloadPlugins();
	}
	else{
		int l=lstrlen(name)-4;
		if(l>0 && !_tcscmp(name+l, _T(" ..."))) name[l]=0;
		convertT2W(name, wname);
		IBaseFilter *pFilter;
		if(pGB && pGB->FindFilterByName(wname, &pFilter)==S_OK){
			DWORD id;
			HANDLE propertyThread= CreateThread(0, 0, propertyProc, pFilter, 0, &id);
			CloseHandle(propertyThread);
		}
	}
}

void filterProperty(UINT id)
{
	TCHAR buf[128];
	MENUITEMINFO mii;
	mii.cbSize= sizeof(MENUITEMINFO);
	mii.fMask= MIIM_TYPE;
	mii.dwTypeData=buf;
	mii.cch=sizeA(buf);
	HMENU menu= GetSubMenu(GetMenu(hWin), MENU_FILTERS);
	if(GetMenuItemInfo(menu, id, FALSE, &mii)){
		filterProperty(buf);
	}
}

//------------------------------------------------------------------
static int Nfilters;

void fillFilterMenu(IBaseFilter *pFilter, void *menu)
{
	ISpecifyPropertyPages *pSpecify;
	FILTER_INFO FilterInfo;

	if(SUCCEEDED(pFilter->QueryFilterInfo(&FilterInfo))){
		if(FilterInfo.achName[1]!=':'){
			convertW2T(FilterInfo.achName, name);
			if(!_tcscmp(name, _T("Ligos MPEG Video Decoder")) && state==ID_FILE_PLAY){
				//this is required for Overlay Mixer, but I don't know why
				isMPEG2=true;
				if(pDDXM) seek(0);
			}
			if(SUCCEEDED(pFilter->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pSpecify))){
				pSpecify->Release();
				TCHAR name2[256];
				lstrcpyn(name2, name, sizeA(name2)-4);
				lstrcat(name2, _T(" ..."));
				name=name2;
			}
			InsertMenu((HMENU)menu, 0xFFFFFFFF, MF_BYPOSITION|MF_STRING, ID_FILTERS+Nfilters, name);
			Nfilters++;
		}
		FilterInfo.pGraph->Release();
	}
}

void enumFilters()
{
	HMENU menu= GetSubMenu(GetMenu(hWin), MENU_FILTERS);
	while(DeleteMenu(menu, 1, MF_BYPOSITION));
	Nfilters=0;
	if(audioFilter && visualFilter || cdAudioFilter || waAudioFilter){
		InsertMenu(menu, 1, MF_BYPOSITION|MF_STRING, ID_FILTERS, AUDIO_FILTER_NAME);
		Nfilters++;
	}
	if(cdAudioFilter || waAudioFilter){
		InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION|MF_STRING, ID_FILTERS+1, VISUAL_FILTER_NAME);
		Nfilters++;
	}
	if(waAudioFilter){
		_stprintf(Winamp::menuName, _T("%.124hs..."), Winamp::in->description);
		InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION|MF_STRING, ID_FILTERS+2, Winamp::menuName);
		Nfilters++;
	}
	EnumFilters(fillFilterMenu, (void*)menu);
	if(!Nfilters){
		InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION|MF_STRING, ID_FILTERS, _T("---"));
	}
	DeleteMenu(menu, 0, MF_BYPOSITION);
}

//------------------------------------------------------------------
void switchAudioStream1(IBaseFilter *pFilter, void *done)
{
	IAMStreamSelect *pSS;
	int i, n;

	if(SUCCEEDED(pFilter->QueryInterface(IID_IAMStreamSelect, (void **)&pSS))){
		pSS->Count((DWORD*)&n);
		if(audioStream<n){
			for(i=audioStream-1; !*(bool*)done; i--){
				if(i<0) i=n-1;
				if(i==audioStream) break;
				AM_MEDIA_TYPE *type;
				if(pSS->Info(i, &type, 0, 0, 0, 0, 0, 0)==S_OK){
					if(type->majortype==MEDIATYPE_Audio){
						pSS->Enable(i, AMSTREAMSELECTENABLE_ENABLE);
						audioStream=i;
						*(bool*)done=true;
					}
					DeleteMediaType(type);
				}
			}
		}
		pSS->Release();
	}
}

void switchAudioStream()
{
	bool done=false;
	EnumFilters(switchAudioStream1, &done);
	if(!done){
		msglng(788, "This file has only one audio stream");
	}
}

//------------------------------------------------------------------
void enumAudioRenderers(int action, void *param)
{
	VARIANT varName;
	ICreateDevEnum *pSysDevEnum;
	IEnumMoniker *pAudioEnum;
	IMoniker *pMoniker;
	IPropertyBag *pPropBag;
	int i=0;
	int audioDevId=0;

	if(SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
		 IID_ICreateDevEnum, (void **)&pSysDevEnum))){
		if(SUCCEEDED(pSysDevEnum->CreateClassEnumerator(CLSID_AudioRendererCategory,
			 &pAudioEnum, 0)) && pAudioEnum){

			while(pAudioEnum->Next(1, &pMoniker, 0) == S_OK){
				if(SUCCEEDED(pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag))){
					//read filter name
					varName.vt = VT_BSTR;
					if(SUCCEEDED(pPropBag->Read(L"FriendlyName", &varName, 0))){
						convertW2T(varName.bstrVal, name);
						switch(action){
							case 1: //add filter to the combo box
								SendMessage((HWND)param, CB_ADDSTRING, 0, (LPARAM)name);
								if(!_tcscmp(outRender, name)){
									audioDevId=i;
								}
								i++;
								break;
							case 2: //add selected filter to the DirectShow graph
								if(!_tcscmp(outRender, name)){
									IBaseFilter *outFilter;
									if(SUCCEEDED(pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&outFilter))){
										((IGraphBuilder*)param)->AddFilter(outFilter, varName.bstrVal);
										outFilter->Release();
									}
								}
								break;
						}
						SysFreeString(varName.bstrVal);
					}
					pPropBag->Release();
				}
				pMoniker->Release();
			}
			pAudioEnum->Release();
		}
		pSysDevEnum->Release();
	}
	if(action==1) SendMessage((HWND)param, CB_SETCURSEL, audioDevId, 0);
}

//------------------------------------------------------------------
void appendBstr(TCHAR *&s, BSTR b)
{
	if(b && *b){
		convertW2T(b, t);
		cpStr(s, t);
	}
	SysFreeString(b);
}

void videoInfo(IBaseFilter *filter, void *param)
{
	IAMMediaContent *m;
	TCHAR **t= (TCHAR**)param;

	if(SUCCEEDED(filter->QueryInterface(IID_IAMMediaContent, (void**)&m))){
		BSTR b;
		if(SUCCEEDED(m->get_AuthorName(&b))) appendBstr(t[T_ARTIST], b);
		if(SUCCEEDED(m->get_Title(&b))) appendBstr(t[T_TITLE], b);
		if(SUCCEEDED(m->get_Copyright(&b))) appendBstr(t[T_COPYRIGHT], b);
		if(SUCCEEDED(m->get_BaseURL(&b))) appendBstr(t[T_URL], b);
		else if(SUCCEEDED(m->get_MoreInfoURL(&b))) appendBstr(t[T_URL], b);
		if(SUCCEEDED(m->get_MoreInfoText(&b))) appendBstr(t[T_COMMENT], b);
		if(SUCCEEDED(m->get_Description(&b))) appendBstr(t[T_DESCRIPTION], b);
		if(t[T_TITLE] && t[T_DESCRIPTION] && !_tcscmp(t[T_TITLE], t[T_DESCRIPTION])){
			delStr(t[T_DESCRIPTION]);
		}
		m->Release();
	}
}

void fileInfo(int which)
{
	if(which<0) return;
	Titem *item= &playlist[which];
	if(waAudioFilter && which==currentFile){
		convertT2A(item->file, fnA);
		Winamp::lock++;
		Winamp::in->InfoBox(fnA, hWin);
		Winamp::lock--;
		Winamp::unloadPlugins();
		return;
	}
	//read tags
	TCHAR *t[MTAGS];
	memset(t, 0, sizeof(t));
	if(isCDA(item)){
		cpStr(t[T_ARTIST], item->artist);
		cpStr(t[T_TITLE], item->title);
		cpStr(t[T_ALBUM], item->album);
		cpStr(t[T_YEAR], item->year);
		cpStr(t[T_GENRE], item->genre);
		cpStr(t[T_COMMENT], item->comment);
	}
	else if(!getTags(item, t)){ //ID3,Ogg,APE,WMA
		if(which==currentFile) EnumFilters(videoInfo, t); //IAMMediaContent
	}
	//print file name, file size and length
	Darray<TCHAR> s;
	dtprintf(s, lng(670, _T("File: %s\nSize: %I64d bytes\n")),
		item->file, item->size);
	if(item->length){
		dtprintf(s, lng(673, _T("Length: %.3f seconds\n")), double(item->length)/UNITS);
	}
	//print video size
	if(which==currentFile){
		if(isVideo){
			dtprintf(s, lng(671, _T("\nOriginal video size: %d x %d\nDisplayed video size: %d x %d\n")),
				origW, origH, videoWidth, videoHeight);
			if(frameRate){
				dtprintf(s, lng(672, _T("Frame rate: %.3f\n")), frameRate);
			}
		}
	}
	dtprintf(s, _T("\n"));
	//print CD album length
	if(isCDA(item)) cdInfo(s);
	//print tags
	for(int i=0; i<MTAGS; i++){
		TCHAR *tag=t[i];
		if(tag){
			dtprintf(s, _T("%s: %s\n"), lng(1000+i, tagNames[i]), tag);
			delete[] tag;
		}
	}
	//show message box
	msg("%s", s);
}

