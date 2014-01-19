/*
	(C) 2004-2008  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include <mpconfig.h>

#include "draw.h"
#include "tinyplay.h"
#include "assocext.h"

COLORREF colorKeyRGB;  //overlay color key (RGB)
LPDIRECTDRAW pDDObject;       //DirectDraw object
LPDIRECTDRAWSURFACE pPrimary; //DirectDraw primary surface
IDDrawExclModeVideoCallback *pOverlayCallback;  //overlay callback handler
IDDrawExclModeVideo *pDDXM;   //video interface if Overlay Mixer is used
HMODULE ddrawDll;      //draw.dll handle
DDCOLORCONTROL colorControl;
POINT monitorOffset;

CSubtitles subtitles;  //subtitles object
LOGFONT subtFont;      //subtitles font

struct TcolorTrackbars {
	int id;
	SHORT min, max;
	LONG norm;
	DWORD flag;
	LONG DDCOLORCONTROL::* offset;
}
colorTrackbars[]={
	{IDC_BRIGHTNESS, 0, 10000, 750, DDCOLOR_BRIGHTNESS, &DDCOLORCONTROL::lBrightness},
	{IDC_CONTRAST, 0, 20000, 10000, DDCOLOR_CONTRAST, &DDCOLORCONTROL::lContrast},
	{IDC_SATURATION, 0, 20000, 10000, DDCOLOR_SATURATION, &DDCOLORCONTROL::lSaturation},
	{IDC_HUE, -180, 180, 0, DDCOLOR_HUE, &DDCOLORCONTROL::lHue},
	{IDC_GAMMA, 0, 500, 1, DDCOLOR_GAMMA, &DDCOLORCONTROL::lGamma},
};

//------------------------------------------------------------------

/*

{frame}{frame}txt

hh:mm:ss:txt

[hh:mm:ss]txt

[hh:mm:ss]
txt
[hh:mm:ss]

num
hh:mm:ss,iii --> hh:mm:ss,iii
txt
txt

hh:mm:ss.ii,hh:mm:ss.ii
txt

-->> frame
txt
txt

*/

LONGLONG scanTimeA(char *&ptr)
{
	long t, m, b;
	char *s, *e;

	s=ptr;
	while(*s==' ') s++;
	if(*s=='[') s++;
	b=UNITS;
	t=strtol(s, &s, 10); //hour
	if(*s==':'){
		s++;
		t=t*60+strtol(s, &s, 10); //min
		if(*s==':'){
			s++;
			t=t*60+strtol(s, &s, 10); //sec
			if(*s=='.' || *s==','){
				s++;
				m=strtol(s, &e, 10); //millisec
				while(s<e){
					t*=10;
					b/=10;
					s++;
				}
				t+=m;
			}
		}
	}
	else{
		b=1; t=-1;
	}
	if(*s==']') s++;
	while(*s==' ') s++;
	ptr=s;
	return t*(LONGLONG)b;
}

LONGLONG rdFrame(char *&s)
{
	char *e;
	LONGLONG f= strtol(s, &e, 10);
	if(e==s) return -1;
	s=e;
	LONGLONG t;
	if(pMS && SUCCEEDED(pMS->ConvertTimeFormat(&t, &TIME_FORMAT_MEDIA_TIME, f, &TIME_FORMAT_FRAME))){
		return t;
	}
	return (LONGLONG)(f*UNITS/(frameRate ? frameRate : DEFAULTFRAME));
}

void CSubtitles::next()
{
	char *s, *s0;
	LONGLONG start1, stop1;
	int isEOL;

	if(!file) return;
	lines=0;
	start=stop=-1;
	s=ptr;
	isEOL=1;
	for(;;){
		start1=stop1=-1;
		for(;;){
			for(;;){
				if(*s=='\r'){ if(s[1]!='\n') isEOL++; }
				else if(*s=='\n') isEOL++;
				else if(*s!=' ' && *s!='\t') break;
				s++;
			}
			if(isEOL) break;
			if(!*s){
				start1= 0x4000000000000000;
				break;
			}
			T[lines].txt=s;
			int d=0;
			for(;;){
				if(*s=='\r' || *s=='\n') break;
				if(*s=='|'){ d=1; break; }
				if(*s=='[' && !_strnicmp(s, "[br]", 4)){ d=4; break; }
				s++;
			}
			T[lines].len= s-T[lines].txt;
			if(lines<MsubtLines && start>=0) lines++;
			s+=d;
		}
		s0=s;
		if(*s=='{' && s[1]>='0' && s[1]<='9'){
			s++;
			start1=rdFrame(s);
			if(*s=='}') s++;
			if(*s=='{'){
				s++;
				stop1=rdFrame(s);
				if(*s=='}') s++;
			}
		}
		else if(!strncmp(s, "-->>", 4)){
			s+=4;
			start1=rdFrame(s);
		}
		else if(*s>='0' && *s<='9' || *s=='[' && s[1]>='0' && s[1]<='9'){
			start1=scanTimeA(s);
			if(start1<0 && isEOL==1) s=s0;
			if(*s==':'){
				s++;
			}
			else if(*s==','){
				s++;
				stop1=scanTimeA(s);
			}
			else if(*s=='-' && s[1]=='-' && s[2]=='>'){
				s+=3;
				stop1=scanTimeA(s);
			}
		}
		if(start1>=0){
			if(start>=0){
				if(stop<0) stop=start1;
				break;
			}
			start=start1;
			stop=stop1;
		}
		isEOL=0;
	}
	if(stop>start+maxDelay) stop=start+maxDelay;
	ptr=s0;
}

void CSubtitles::timer()
{
	if(!file) return;
	pos=position;
	if(scale!=frameRate){
		pos= (LONGLONG)(pos/(frameRate ? frameRate : DEFAULTFRAME)*scale);
	}
	pos+=delta;
	while(pos>=stop){
		clear();
		//read next subtitle 
		next();
	}
	if(!on && pos>=start){
		HDC dc=GetDC(videoWnd);
		draw(dc);
		ReleaseDC(videoWnd, dc);
	}
}

void CSubtitles::clear()
{
	//clear previous subtitle 
	if(on){
		HDC dc=GetDC(videoWnd);
		HBRUSH br= CreateSolidBrush(colorKeyRGB);
		for(int i=0; i<lines; i++){
			RECT *rt= &T[i].rc;
			if(!fullscreen){
				FillRect(dc, rt, br);
			}
			else{
				RECT rd, ri;
				SetRect(&ri, videoFrameLeft, videoFrameTop,
					videoFrameLeft+videoWidth, videoFrameTop+videoHeight);
				BOOL t= IntersectRect(&rd, &ri, rt);
				//part outside the video is filled with black color
				if(!EqualRect(&rd, rt)) FillRect(dc, rt, (HBRUSH)GetStockObject(BLACK_BRUSH));
				//part inside the video filled with key color
				if(t) FillRect(dc, &rd, br);
			}
		}
		DeleteObject(br);
		ReleaseDC(videoWnd, dc);
		on=false;
	}
}

void CSubtitles::draw(HDC dc)
{
	int i;
	HGDIOBJ oldPen=0, oldBrush=0, oldFont;
	LONG oldFontSize;
	HBRUSH br;
	int w, h, y;
	SIZE sz[MsubtLines];
	RECT rcw;

	if(pos<start || pos>=stop || !lines || disabled) return;
	on=true;
	//create pen, brush
	SetBkMode(dc, subtOpaque && !subtOutLine ? OPAQUE : TRANSPARENT);
	SetBkColor(dc, colors[clBkSubt]);
	if(subtOutLine){
		oldPen=SelectObject(dc, CreatePen(PS_SOLID, subtOutLine, colors[clSubtOut]));
		oldBrush=SelectObject(dc, CreateSolidBrush(colors[clSubtFill]));
	}
	else{
		SetTextColor(dc, colors[clSubtFill]);
	}
	oldFontSize=subtFont.lfHeight;
	GetClientRect(videoWnd, &rcw);
	oldFont=GetCurrentObject(dc, OBJ_FONT);
	for(;;){
		SelectObject(dc, CreateFontIndirect(&subtFont));
		//calculate text size
		w=h=0;
		for(i=0; i<lines; i++){
			SIZE *s=&sz[i];
			GetTextExtentPoint32A(dc, T[i].txt, T[i].len, s);
			if(s->cx > w) w = s->cx;
			h+= s->cy;
		}
		if(w<=rcw.right && h<=rcw.bottom || subtFont.lfHeight<=MINFONT) break;
		subtFont.lfHeight--;
		DeleteObject(SelectObject(dc, oldFont));
	}
	//draw subtitle
	y=rcw.bottom-max(h, (fullscreen ? subtPosFull : subtPosWin));
	for(i=0; i<lines; i++){
		if(subtOutLine){
			BeginPath(dc);
		}
		Tline *t= &T[i];
		RECT *rc= &t->rc;
		rc->left= (rcw.right-sz[i].cx)>>1;
		rc->top= y;
		y+= sz[i].cy;
		rc->right= rc->left+sz[i].cx+subtOutLine;
		rc->bottom= rc->top+sz[i].cy+subtOutLine;
		if(subtOpaque && subtOutLine){
			br= CreateSolidBrush(colors[clBkSubt]);
			FillRect(dc, rc, br);
			DeleteObject(br);
		}
		TextOutA(dc, rc->left, rc->top, t->txt, t->len);
		if(subtOutLine){
			rc->left -= subtOutLine;
			rc->top -= subtOutLine;
			EndPath(dc);
			StrokeAndFillPath(dc);
		}
	}
	if(subtOutLine){
		DeleteObject(SelectObject(dc, oldPen));
		DeleteObject(SelectObject(dc, oldBrush));
	}
	DeleteObject(SelectObject(dc, oldFont));
	subtFont.lfHeight=oldFontSize;
}

//------------------------------------------------------------------

void CSubtitles::calcMaxDelay()
{
	maxDelay= UNITS*(LONGLONG)subtMaxDelay;
	if(scale!=frameRate){
		if(!scale) scale= frameRate;
		maxDelay= (LONGLONG)(maxDelay/(frameRate ? frameRate : DEFAULTFRAME)*scale);
	}
}

void CSubtitles::reset()
{
	ptr=file;
	clear();
	calcMaxDelay();
	next();
}

void setSubtMenu(TCHAR *fn)
{
	ModifyMenu(GetSubMenu(GetMenu(hWin), MENU_SUBTITLES), ID_SUBTITLES,
		MF_BYCOMMAND, ID_SUBTITLES, cutPath(fn));
}

void CSubtitles::load(TCHAR *fn)
{
	HANDLE f=CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if(f!=INVALID_HANDLE_VALUE){
		DWORD len=GetFileSize(f, 0);
		if(len<10000000){
			setSubtMenu(fn);
			lines=0;
			delta=0;
			scale=0;
			delete[] file;
			file= new char[len+16];
			DWORD r;
			ReadFile(f, file, len, &r, 0);
			if(r==len){
				file[len]='\n';
				file[len+1]=0;
				reset();
				AddOvMToGraph();
			}
			if(!lines){
				delete[] file;
				file=0;
			}
		}
		CloseHandle(f);
	}
}

void CSubtitles::close()
{
	clear();
	delete[] file; file=0;
	setSubtMenu(_T("-"));
}

void findSubtitles()
{
	HANDLE h;
	WIN32_FIND_DATA fd;
	TCHAR *e, *name, *file;
	TCHAR buf[MAX_PATH], subt1[MAX_PATH];
	int len, cmpLen, i, k, cntAvi, cntSub;

	name= playlist[currentFile].name;
	e=cutExt(name);
	if(e) cmpLen=int(e-name);
	else cmpLen=lstrlen(name);
	//first look in a current folder, then in subtitles folder
	for(k=0; k<2; k++){
		//first compare whole file name, then prefix
		for(i=1; i>=0; i--){
			if(subtitles.file) break;
			if(k){
				file= subtFolder;
				if(!*file) continue;
				len= lstrlen(subtFolder);
				if(file[len-1]!='\\') len++;
			}
			else{
				file= playlist[currentFile].file;
				len= int(name-file);
			}
			cntAvi=cntSub=0;
			//find all subtitle files in the folder
			lstrcpyn(buf, file, len);
			lstrcpy(buf+len-1, _T("\\*.*"));
			h = FindFirstFile(buf, &fd);
			if(h!=INVALID_HANDLE_VALUE){
				do{
					if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
						//calculate count of video files and count of subtitle files
						if(k==0 && cntAvi<=1 && cntSub<=1){
							e=cutExt(fd.cFileName);
							if(e){
								e++;
								if(scanExt(extensions, 0, e) &&
									!scanExt(audioExt, 0, e)){
									cntAvi++;
								}
								else if(scanExt(subtExt, 0, e)){
									if(cntSub==0) lstrcpy(subt1, fd.cFileName);
									cntSub++;
								}
							}
						}
						if(!_tcsnicmp(fd.cFileName, name, cmpLen+i)){
							//compare extension
							e=cutExt(fd.cFileName);
							if(e && scanExt(subtExt, 0, e+1)){
								//open subtitles
								lstrcpy(buf+len, fd.cFileName);
								subtitles.load(buf);
								if(subtitles.file) break; //success
							}
						}
					}
				} while(FindNextFile(h, &fd));
				FindClose(h);
				if(!subtitles.file && cntAvi<=1 && cntSub==1){
					//there is only 1 video and 1 subtitle
					lstrcpy(buf+len, subt1);
					subtitles.load(buf);
				}
			}
		}
	}
}

//------------------------------------------------------------------
HMONITOR(WINAPI* monitorFromRect)(LPCRECT, DWORD);
BOOL(WINAPI* getMonitorInfo)(HMONITOR, LPMONITORINFO);

bool multiMonitors()
{
	bool result = GetSystemMetrics(SM_CMONITORS) > 1;
	if(result && !monitorFromRect){
		HMODULE user32 = GetModuleHandle(_T("USER32"));
		monitorFromRect = (HMONITOR(WINAPI*)(LPCRECT, DWORD)) GetProcAddress(user32, "MonitorFromRect");
		getMonitorInfo = (BOOL(WINAPI*)(HMONITOR, LPMONITORINFO)) GetProcAddress(user32, "GetMonitorInfoA");
	}
	return result;
}

static GUID ddGuid;
static HMONITOR currentMonitor;

static BOOL WINAPI findDDGuidProc(GUID *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID result, HMONITOR hm)
{
	if(hm == currentMonitor){
		MONITORINFO mi;
		mi.cbSize=sizeof(MONITORINFO);
		if(getMonitorInfo(hm, &mi)){
			//monitorOffset is needed for pDDXM->SetDrawParameters
			monitorOffset.x = mi.rcMonitor.left;
			monitorOffset.y = mi.rcMonitor.top;
			//copy GUID to global variable
			ddGuid=*lpGUID;
			*(GUID**)result=&ddGuid;
			return FALSE;
		}
	}
	return TRUE;
}


GUID* findDDGuid()
{
	GUID* result = NULL;
	monitorOffset.x = monitorOffset.y = 0;

	if(multiMonitors()){
		//find HMONITOR where is the video window
		RECT rc;
		SetRect(&rc, videoLeft, videoTop, videoLeft + videoWidth, videoTop + videoHeight);
		currentMonitor = monitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
		//find monitor GUID
		TdirectDrawEnumerateEx directDrawEnumerateEx= (TdirectDrawEnumerateEx)GetProcAddress(ddrawDll, "DirectDrawEnumerateExA");
		if(directDrawEnumerateEx){
			directDrawEnumerateEx(findDDGuidProc, &result, DDENUM_ATTACHEDSECONDARYDEVICES);
		}
	}
	return result;
}


// create DirectDraw and primary surface
HRESULT createDirectDraw()
{
	if(pPrimary) return S_OK;
	HRESULT hr=DDERR_NODIRECTDRAWHW;
	if(!ddrawDll) ddrawDll= LoadLibrary(_T("ddraw.dll"));
	if(ddrawDll){
		TddrawCreate directDrawCreate= (TddrawCreate)GetProcAddress(ddrawDll, "DirectDrawCreate");
		if(directDrawCreate){
			hr= directDrawCreate(findDDGuid(), &pDDObject, NULL);
			if(hr==DD_OK){
				hr = pDDObject->SetCooperativeLevel(videoWnd, DDSCL_NORMAL);
				if(SUCCEEDED(hr)){
					// Create the primary surface 
					DDSURFACEDESC ddsd;
					ZeroMemory(&ddsd, sizeof(ddsd));
					ddsd.dwSize = sizeof(ddsd);
					ddsd.dwFlags = DDSD_CAPS;
					ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
					hr = pDDObject->CreateSurface(&ddsd, &pPrimary, NULL);
				}
				else{
					RELEASE(pDDObject);
				}
			}
		}
	}
	return hr;
}

//------------------------------------------------------------------

// connect the decoded video stream through the OverlayMixer.
HRESULT connectFilter(IBaseFilter *pOvM, WCHAR *toWhom)
{
	IBaseFilter *pVR;
	IEnumPins *pEnumPins;
	IPin *pPinVR, *pPinConnectedTo, *pPinOvM;
	PIN_DIRECTION pd;
	HRESULT result;

	// find Video Renderer filter
	result= pGB->FindFilterByName(toWhom, &pVR);
	if(FAILED(result)) return result;
	// get the video renderer's (only) pin
	pVR->EnumPins(&pEnumPins);
	pEnumPins->Next(1, &pPinVR, 0);
	pEnumPins->Release();

	// disconnect VR from upstream filter and put OvMixer in between them
	pPinVR->ConnectedTo(&pPinConnectedTo);
	pGB->Disconnect(pPinVR);
	pGB->Disconnect(pPinConnectedTo);

	// now connect the upstream filter's out pin to OvM's in pin 
	pOvM->EnumPins(&pEnumPins);
	result=E_FAIL;
	while(pEnumPins->Next(1, &pPinOvM, 0)==S_OK){
		pPinOvM->QueryDirection(&pd);
		// OvM's in pin <- Upstream filter's pin
		if(pd==PINDIR_INPUT){
			if(SUCCEEDED(pGB->Connect(pPinConnectedTo, pPinOvM))){
				result=S_OK;
				//get surface color controls
				if(SUCCEEDED(pPinOvM->QueryInterface(IID_IMixerPinConfig2, (void **)&pColor))){
					colorControl.dwSize = sizeof(DDCOLORCONTROL);
					if(SUCCEEDED(pColor->GetOverlaySurfaceColorControls(&colorControl))){
						for(TcolorTrackbars *t = colorTrackbars; t < endA(colorTrackbars); t++){
							bool b = (colorControl.dwFlags & t->flag)!=0;
							if(b) SendDlgItemMessage(DDColorWnd, t->id, TBM_SETPOS, TRUE, colorControl.*t->offset);
							EnableWindow(GetDlgItem(DDColorWnd, t->id), b);
						}
					}
				}
				pPinOvM->Release();
				break;
			}
		}
		pPinOvM->Release();
	}
	pEnumPins->Release();
	if(result==S_OK){
		// remove the Video Renderer from graph
		pGB->RemoveFilter(pVR);
	}
	else{
		pGB->Connect(pPinConnectedTo, pPinVR);
	}
	pPinConnectedTo->Release();
	pPinVR->Release();
	pVR->Release();
	return result;
}

void convertColorKey(DWORD dwColor)
{
	COLORREF old;
	HDC hDC;
	HRESULT hr;
	DDSURFACEDESC ddsd;

	if(pPrimary->GetDC(&hDC)==DD_OK){
		// get previous pixel color
		old=GetPixel(hDC, 0, 0);
		pPrimary->ReleaseDC(hDC);
		// set pixel to key color
		ddsd.dwSize = sizeof(ddsd);
		while((hr=pPrimary->Lock(0, &ddsd, 0, 0))==DDERR_WASSTILLDRAWING);
		if(hr==DD_OK){
			*(DWORD*)ddsd.lpSurface = dwColor;
			pPrimary->Unlock(0);
		}
		// get colorKeyRGB and restote old the pixel 
		if(pPrimary->GetDC(&hDC)==DD_OK){
			colorKeyRGB= GetPixel(hDC, 0, 0);
			SetPixel(hDC, 0, 0, old);
			pPrimary->ReleaseDC(hDC);
		}
	}
}

// set the color key of the first input pin of the OverlayMixer.
void setColorKey(IBaseFilter *pOvM)
{
	IEnumPins *pEnumPins;
	IPin *pPin;
	IMixerPinConfig *pMPC;
	PIN_DIRECTION pd;
	HRESULT hr;
	COLORREF c;

	pOvM->EnumPins(&pEnumPins);

	while(pEnumPins->Next(1, &pPin, NULL)==S_OK){
		pPin->QueryDirection(&pd);
		if(pd == PINDIR_INPUT){
			hr = pPin->QueryInterface(IID_IMixerPinConfig, (LPVOID *)&pMPC);
			if(SUCCEEDED(hr)){
				HDC dc=GetDC(0);
				c=GetNearestColor(dc, colorKeyRGB);
				if(c!=CLR_INVALID) colorKeyRGB=c;
				ReleaseDC(0, dc);
				COLORKEY ck;
				ck.KeyType=CK_RGB;
				ck.LowColorValue=ck.HighColorValue=colorKeyRGB;
				pMPC->SetColorKey(&ck);
				// set mode to stretch - that way we don't fight the overlay
				// mixer about the exact way to fix the aspect ratio
				pMPC->SetAspectRatioMode(AM_ARMODE_STRETCHED);
				pMPC->Release();
				pPin->Release();
				break;
			}
		}
		pPin->Release();
	}
	pEnumPins->Release();
}


// create DirectDraw, OverlayMixer and add it to the graph
// There are some AVI files which decide to rather go 
// through the Color Space Converter, and don't use OverlayMixer.
HRESULT AddOvMToGraph1()
{
	IBaseFilter *pOM;
	HRESULT hr;

	if(pDDXM || !pMC) return S_FALSE;
	pMC->Stop();
	hr=createDirectDraw();
	if(SUCCEEDED(hr)){
		hr= CoCreateInstance(CLSID_OverlayMixer, NULL, CLSCTX_INPROC, IID_IBaseFilter, (LPVOID *)&pOM);
		if(SUCCEEDED(hr)){
			hr = pGB->AddFilter(pOM, L"Overlay Mixer");
			if(SUCCEEDED(hr)){
				// set the DDraw params now
				hr= pOM->QueryInterface(IID_IDDrawExclModeVideo, (LPVOID *)&pDDXM);
				if(SUCCEEDED(hr)){
					hr = pDDXM->SetDDrawObject(pDDObject);
					if(SUCCEEDED(hr)){
						hr = pDDXM->SetDDrawSurface(pPrimary);
						if(FAILED(hr)){
							pDDXM->SetDDrawObject(NULL);
						}
						else{
							//connect Overlay Mixer to a video decoder
							hr=connectFilter(pOM, L"Video Renderer");
							if(SUCCEEDED(hr)){
								pOverlayCallback = (IDDrawExclModeVideoCallback *) new COverlayCallback();
								pDDXM->SetCallbackInterface(pOverlayCallback, 0);
								//set the color key 
								setColorKey(pOM);
								SAFE_RELEASE(pVW);
								onVideoMoved();
							}
						}
					}
				}
				if(FAILED(hr)){
					// remove Overlay Mixer from the graph
					pGB->RemoveFilter(pOM);
					SAFE_RELEASE(pDDXM);
				}
			}
			pOM->Release();
		}
	}
	pMC->Pause();
	if(state==ID_FILE_PLAY) pMC->Run();
	return hr;
}

HRESULT AddOvMToGraph()
{
	if(useOverlayMixer==2) return S_FALSE;
	if(useOverlayMixer==0){
		if(notMpgOverlay){
			TCHAR *s= playlist[currentFile].name;
			if(tolower(_tcschr(s, 0)[-1])=='g') return S_FALSE;
		}
		else{
			if(!subtitles.file) return S_FALSE;
		}
	}
	return AddOvMToGraph1();
}

//------------------------------------------------------------------

COverlayCallback::COverlayCallback()
{
	AddRef(); // increment ref count for itself
}


HRESULT COverlayCallback::OnUpdateOverlay(BOOL  bBefore, DWORD dwFlags,
	BOOL  bOldVisible, const RECT *prcSrcOld, const RECT *prcDestOld, BOOL  bNewVisible,
	const RECT *prcSrcNew, const RECT *prcDestNew)
{
	return S_OK;
}


HRESULT COverlayCallback::OnUpdateColorKey(COLORKEY const *pKey,
	DWORD dwColor)
{
	convertColorKey(dwColor);
	return S_OK;
}


HRESULT COverlayCallback::OnUpdateSize(DWORD dwWidth, DWORD dwHeight,
	DWORD dwARWidth, DWORD dwARHeight)
{
	if(origW!=(int)dwWidth || origH!=(int)dwHeight){
		origW=dwWidth;
		origH=dwHeight;
		PostMessage(hWin, WM_COMMAND, 903, 0);
	}
	return S_OK;
}

//------------------------------------------------------------------
struct TgrabInfo {
	TCHAR *fileName;
	int result;
};

int saveBMP(TCHAR *fn, BYTE *bits, int width, int height, int len)
{
	BITMAPINFOHEADER bmi;
	BITMAPFILEHEADER hdr;
	HANDLE f;
	DWORD d;

	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biWidth = width;
	bmi.biHeight = height;
	bmi.biPlanes = 1;
	bmi.biBitCount = 24;
	bmi.biCompression = BI_RGB;
	bmi.biSizeImage = len;
	bmi.biClrImportant = 0;
	bmi.biClrUsed=0;
	f= CreateFile(fn, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if(f==INVALID_HANDLE_VALUE) return 1;
	hdr.bfType = 0x4d42;  //'BM'
	hdr.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bmi.biSizeImage;
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
	WriteFile(f, &hdr, sizeof(BITMAPFILEHEADER), &d, 0);
	WriteFile(f, &bmi, sizeof(BITMAPINFOHEADER), &d, 0);
	WriteFile(f, bits, bmi.biSizeImage, &d, 0);
	CloseHandle(f);
	return d!=bmi.biSizeImage;
}

/*

Y= (0.299*R + 0.587*G + 0.114*B)
U= 0.493(B-Y) = -0.147407*R - 0.289391*G + 0.436798*B
V= 0.877(R-Y) = 0.614777*R - 0.514799*G - 0.099978*B

R= Y+1.140250855*V
G= Y-0.3939307027*U-0.580809209*V
B= Y+2.028397566*U
*/

BYTE cc(double k)
{
	if(k>255) return 255;
	if(k<0) return 0;
	return BYTE(k);
}

static HRESULT CALLBACK enumOverlay(LPDIRECTDRAWSURFACE surface,
	 LPDDSURFACEDESC desc, LPVOID param)
{
	BYTE Y0, Y1;
	int U, V;
	int len;
	BYTE *A, *A0=0;

	len=(3*(desc->dwWidth+1)&-4)*desc->dwHeight;
	DDSURFACEDESC ddsd;
	ddsd.dwSize=sizeof(DDSURFACEDESC);
	if(surface->Lock(0, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_NOSYSLOCK|DDLOCK_WAIT, 0)==DD_OK){
		A=A0=new BYTE[len];
		BYTE *p= (BYTE*)ddsd.lpSurface + (desc->dwHeight-1)*ddsd.lPitch;
		for(unsigned y=0; y<desc->dwHeight; y++){
			BYTE *pn= p-ddsd.lPitch;
			for(unsigned x=0; x<desc->dwWidth; x+=2){
				switch(ddsd.ddpfPixelFormat.dwFourCC){
					case MAKEFOURCC('Y', 'U', 'Y', 'V'):
					case MAKEFOURCC('Y', 'U', 'Y', '2'):
						Y0=*p++;
						U= *p++;
						Y1=*p++;
						V= *p++;
						break;
					case MAKEFOURCC('U', 'Y', 'V', 'Y'):
						U= *p++;
						Y0=*p++;
						V= *p++;
						Y1=*p++;
						break;
					case MAKEFOURCC('Y', 'V', 'Y', 'U'):
						Y0=*p++;
						V= *p++;
						Y1=*p++;
						U= *p++;
						break;
					default:
						delete[] A0;
						A0=0;
						goto l1;
				}
				U-=128;
				V-=128;
				*A++=cc(Y0+2.028397566*U);
				*A++=cc(Y0-0.3939307027*U-0.580809209*V);
				*A++=cc(Y0+1.140250855*V);
				*A++=cc(Y1+2.028397566*U);
				*A++=cc(Y1-0.3939307027*U-0.580809209*V);
				*A++=cc(Y1+1.140250855*V);
			}
			p=pn;
			while(int(A-A0)&3) *A++=0;
		}
	l1:
		surface->Unlock(0);
	}
	if(A0){
		((TgrabInfo*)param)->result= saveBMP(
			((TgrabInfo*)param)->fileName, A0, desc->dwWidth, desc->dwHeight, len);
		delete[] A0;
		return DDENUMRET_CANCEL;
	}
	return DDENUMRET_OK;
}

//get a video frame from the Overlay surface
int grabOverlay(TCHAR *name)
{
	LPDIRECTDRAWSURFACE oldPrimary= pPrimary;
	TgrabInfo info;
	info.fileName=name;
	info.result=2;
	if(createDirectDraw()==S_OK){
		pPrimary->EnumOverlayZOrders(DDENUMOVERLAYZ_FRONTTOBACK, &info, enumOverlay);
		if(!oldPrimary){
			RELEASE(pPrimary);
			SAFE_RELEASE(pDDObject);
		}
	}
	return info.result;
}

//use MediaDet to grab a video frame
int grabMediaDet(TCHAR *name)
{
	IMediaDet *pDet;
	HRESULT hr;
	int result=2;

	if(FAILED(CoCreateInstance(CLSID_MediaDet, 0, CLSCTX_INPROC_SERVER, __uuidof(IMediaDet), (void**)&pDet))){
		result=3;
	}
	else{
		convertT2W(playlist[currentFile].file, wfileName);
		if(SUCCEEDED(pDet->put_Filename(wfileName))){
			for(long i=0; i<99; i++){
				GUID major_type;
				if(FAILED(pDet->put_CurrentStream(i))) break;
				if(FAILED(pDet->get_StreamType(&major_type))) break;
				if(major_type == MEDIATYPE_Video){
					convertT2W(name, wname);
					BSTR bname=SysAllocString(wname);
					hr= pDet->WriteBitmapBits(position/(double)UNITS, origW, origH, bname);
					SysFreeString(bname);
					if(hr==S_OK) result=0;
					else if(hr==E_ACCESSDENIED || hr==STG_E_ACCESSDENIED) result=1;
					break;
				}
			}
		}
		pDet->Release();
	}
	return result;
}

int getFrameNumber()
{
	LONGLONG frame;
	GUID guidOriginalFormat;
	static int cnt;

	if(!getPosition()) return cnt++;
	pMS->GetTimeFormat(&guidOriginalFormat);
	pMS->SetTimeFormat(&TIME_FORMAT_FRAME);
	if(FAILED(pMS->GetCurrentPosition(&frame))){
		frame= position/100000;
	}
	pMS->SetTimeFormat(&guidOriginalFormat);
	return (int)frame;
}

//save the current frame to BMP file
void grab()
{
	if(pMC) pMC->Pause();
	TCHAR name[MAX_PATH], *s;
	lstrcpyn(name, playlist[currentFile].file, sizeA(name)-16);
	s=cutExt(name);
	if(!s) s=name+lstrlen(name);
	_stprintf(s, _T("(%d).bmp"), getFrameNumber());
	int err, err1;
	err=err1=grabMediaDet(name);
	if(err) err=grabOverlay(name);
	if(err==1){
		TCHAR name2[MAX_PATH*2];
		getExeDir(name2, cutPath(name));
		err=err1=grabMediaDet(name2);
		if(err) err=grabOverlay(name2);
	}
	if(err){
		if(err==1) msglng(733, "Cannot create file %s", name);
		else if(err1==3) msglng(783, "This function is not available.\nTry to install DirectX 9.");
		else msglng(784, "Failed to capture a frame");
	}
	if(state==ID_FILE_PLAY) pMC->Run();
}

//------------------------------------------------------------------

void restoreOvM()
{
	IBaseFilter *pOvM;
	if(SUCCEEDED(pGB->FindFilterByName(L"Overlay Mixer", &pOvM))){
		pMC->Stop();
		IPin *pin;
		ULONG c;
		IEnumPins *pEnumPins;
		pOvM->EnumPins(&pEnumPins);
		pEnumPins->Next(1, &pin, &c);
		pEnumPins->Release();
		IPin *pin2;
		pin->ConnectedTo(&pin2);
		pin->Release();
		//delete Overlay Mixer
		pGB->RemoveFilter(pOvM);
		pOvM->Release();
		SAFE_RELEASE(pOverlayCallback);
		RELEASE(pDDXM);
		//delete surface that is lost
		RELEASE(pPrimary);
		RELEASE(pDDObject);
		//create video renderer
		pGB->Render(pin2);
		pin2->Release();
		//create new Overlay Mixer
		AddOvMToGraph();
		sizeChanged();
		if(state==ID_FILE_PLAY) pMC->Run();
	}
}

//------------------------------------------------------------------

void showDDColor()
{
	ShowWindow(DDColorWnd, SW_SHOW);
	SetActiveWindow(DDColorWnd);
}

void setColorControls()
{
	if(isVideo){
		if(!pColor) AddOvMToGraph1();
		if(!pColor || FAILED(pColor->SetOverlaySurfaceColorControls(&colorControl))){
			static DWORD lastErr;
			DWORD t = GetTickCount();
			if(t>lastErr+9000) msg(!pColor ? "Cannot connect to Overlay Mixer" : "Cannot set color controls");
			lastErr=t;
		}
	}
}

LRESULT CALLBACK DDColorProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TcolorTrackbars *t;

	switch(message){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 36);
			for(t = colorTrackbars; t < endA(colorTrackbars); t++){
				SendDlgItemMessage(hWnd, t->id, TBM_SETRANGE, TRUE, MAKELONG(t->min, t->max));
			}
			SendMessage(hWnd, WM_COMMAND, IDC_DDCOLOR_RESET, 1);
			return TRUE;

		case WM_HSCROLL:
			if(LOWORD(wParam)==TB_ENDTRACK || LOWORD(wParam)==TB_THUMBTRACK){
				for(t = colorTrackbars; t < endA(colorTrackbars); t++){
					if((HWND)lParam == GetDlgItem(hWnd, t->id)){
						colorControl.dwFlags = t->flag;
						colorControl.*t->offset = SendDlgItemMessage(hWnd, t->id, TBM_GETPOS, 0, 0);
						setColorControls();
						break;
					}
				}
			}
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam)){
				case IDC_DDCOLOR_RESET:
					colorControl.dwFlags = 0;
					for(t = colorTrackbars; t < endA(colorTrackbars); t++){
						SendDlgItemMessage(hWnd, t->id, TBM_SETPOS, TRUE, t->norm);
						colorControl.dwFlags |= t->flag;
						colorControl.*t->offset = t->norm;
					}
					if(lParam != 1) setColorControls();
					break;
				case IDCANCEL:
					ShowWindow(hWnd, SW_HIDE);
					break;
			}
	}
	return FALSE;
}
