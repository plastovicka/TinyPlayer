/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#include "hdr.h"
#include "tinyplay.h"
#include "dvd.h"

int g_ulCurTitle, g_ulNumTitles, noChangeTitle;
bool g_bStillOn, g_bMenuOn;
TCHAR *dvdPath;
ULONG dvdPathLen;

IDvdControl2 *pDvdControl;
IDvdInfo2 *pDvdInfo;
IAMLine21Decoder *pLine21Dec;
LCID *g_pLanguageList;
BOOL g_bDisplayCC, g_bDisplaySubpicture;

HMENU menuDVD, ghTitleMenu, ghChapterMenu, ghAngleMenu,
ghAudioMenu, ghSubpictureMenu, ghMenuLanguageMenu;
//------------------------------------------------------------------

void ResetRate()
{
	setRate(1);
}

void dvdMenu(DVD_MENU_ID which)
{
	pDvdControl->ShowMenu(which, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
	hideWinOnFull();
	invalidateTime();
}

void dvdCnvTime(LONGLONG *duration, DVD_HMSF_TIMECODE t, ULONG flag)
{
	int f=UNITS/25;
	if(flag&DVD_TC_FLAG_DropFrame) f=UNITS*100/2997;
	else if(flag&DVD_TC_FLAG_30fps) f=UNITS/30;
	*duration= LONGLONG((t.bHours*60+t.bMinutes)*60+t.bSeconds)*UNITS+t.bFrames*f;
}

bool dvdDuration(LONGLONG *duration)
{
	DVD_HMSF_TIMECODE t;
	ULONG f;
	if(!pDvdInfo || FAILED(pDvdInfo->GetTotalTitleTime(&t, &f))) return false;
	dvdCnvTime(duration, t, f);
	return true;
}

void dvdFatal(char *err)
{
	msg("%hs");
	PostMessage(hWin, WM_COMMAND, ID_FILE_STOP, 0);
}


LRESULT OnDvdEvent(LONG lEvent, LONG lParam1, LONG lParam2)
{
	switch(lEvent){

		case EC_DVD_CURRENT_HMSF_TIME:
		{
			dvdCnvTime(&position, *(DVD_HMSF_TIMECODE *)&lParam1, lParam2);
			// If we have reached the beginning of the movie, perhaps through
			// seeking backward, then reset the rate to 1.0
			if(lParam1==0) ResetRate();
			if(!tick) PostMessage(hWin, WM_TIMER, 1000, 0);
			//update current title length
			Titem *item= &playlist[currentFile];
			if(!item->length){
				dvdDuration(&item->length);
				invalidateItem(currentFile);
			}
		}
			break;

		case EC_DVD_TITLE_CHANGE:
			g_ulCurTitle = lParam1;
			ResetRate();
			// Indicate the change of title
			UpdateCurrentTitle();
			GetAudioInfo();
			dvdHiliteTitle();
			break;

		case EC_DVD_NO_FP_PGC:
			PostMessage(hWin, WM_COMMAND, 904, 0);
			break;

		case EC_DVD_SUBPICTURE_STREAM_CHANGE:
			UpdateSubpictureInfo();
			break;

		case EC_DVD_AUDIO_STREAM_CHANGE:
			UpdateAudioInfo();
			break;

		case EC_DVD_ANGLE_CHANGE:
			UpdateAngleInfo();
			break;

		case EC_DVD_ANGLES_AVAILABLE:
			GetAngleInfo();
			break;

		case EC_DVD_STILL_ON:
			// if there is a still without buttons, we can call StillOff
			if(TRUE == lParam1) g_bStillOn = true;
			break;

		case EC_DVD_STILL_OFF:
			g_bStillOn = false;
			break;

		case EC_DVD_DISC_EJECTED:
			PostMessage(hWin, WM_COMMAND, ID_FILE_STOP, 0);
			break;

		case EC_DVD_DOMAIN_CHANGE:
			switch(lParam1){

				case DVD_DOMAIN_FirstPlay:
					//DVD started playing outside of the main title
					GetTitleInfo();
					GetAngleInfo();
					GetAudioInfo();
					GetMenuLanguageInfo();
					GetSubpictureInfo();
					PostMessage(hWin, WM_COMMAND, 902, 0);
					break;

				case DVD_DOMAIN_VideoManagerMenu:
				case DVD_DOMAIN_VideoTitleSetMenu:
					g_bMenuOn= true; //menu is visible
					setMouse();
					dvdHiliteTitle();
					break;

				case DVD_DOMAIN_Title:
					g_bMenuOn= false; //we are no longer in a menu
					setMouse();
					dvdHiliteTitle();
					break;
			}
			break;

		case EC_DVD_ERROR:
			switch(lParam1){
				case DVD_ERROR_Unexpected:
					dvdFatal("Unexpected error (possibly incorrectly authored content)");
					break;
				case DVD_ERROR_CopyProtectFail:
					dvdFatal("Key exchange for DVD copy protection failed.");
					break;
				case DVD_ERROR_InvalidDVD1_0Disc:
					dvdFatal("This DVD-Video disc is incorrectly authored for v1.0  of the spec.");
					break;
				case DVD_ERROR_InvalidDiscRegion:
					dvdFatal(
						"This DVD-Video disc cannot be played, because it is not"
						"\nauthored to play in the current system region."
						"\nThe region mismatch may be fixed by changing the"
						"\nsystem region.");
					break;
				case DVD_ERROR_MacrovisionFail:
					dvdFatal(
						"This DVD-Video content is protected by Macrovision."
						"\nThe system does not satisfy Macrovision requirement.");
					break;
				case DVD_ERROR_IncompatibleSystemAndDecoderRegions:
					dvdFatal(
						"No DVD-Video disc can be played on this system, because "
						"\nthe system region does not match the decoder region.");
					break;
				case DVD_ERROR_IncompatibleDiscAndDecoderRegions:
					dvdFatal(
						"This DVD-Video disc cannot be played on this system, because it is"
						"\nnot authored to be played in the installed decoder's region.");
					break;
			}
			break;

		case EC_DVD_WARNING:
			switch(lParam1){
				case DVD_WARNING_Open:
					msg("A file on the DVD disc could not be opened.");
					break;
				case DVD_WARNING_Seek:
					msg("Could not move to a different part of a file on the DVD disc.");
					break;
				case DVD_WARNING_Read:
					msg("Could not read part of a file on the DVD disc.");
					break;
			}
			break;
	}
	return 0;
}

int dvdCommand(WPARAM wParam)
{
	DWORD dwFlags = DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush;

	if(!pDvdControl) return 0;

	switch(wParam){

		case ID_DVD_NEXTCHAPTER:
			pDvdControl->PlayNextChapter(dwFlags, NULL);
			break;
		case ID_DVD_PREVIOUSCHAPTER:
			pDvdControl->PlayPrevChapter(dwFlags, NULL);
			break;
		case ID_DVD_REPLAYCHAPTER:
			pDvdControl->ReplayChapter(dwFlags, NULL);
			break;

		case ID_DVD_MENU_TITLE:
			dvdMenu(DVD_MENU_Title);
			ResetRate();
			break;
		case ID_DVD_MENU_ROOT:
			dvdMenu(DVD_MENU_Root);
			ResetRate();
			break;
		case ID_DVD_MENU_SUBPICTURE:
			dvdMenu(DVD_MENU_Subpicture);
			break;
		case ID_DVD_MENU_AUDIO:
			dvdMenu(DVD_MENU_Audio);
			break;
		case ID_DVD_MENU_ANGLE:
			dvdMenu(DVD_MENU_Angle);
			break;
		case ID_DVD_MENU_CHAPTER:
			dvdMenu(DVD_MENU_Chapter);
			break;
		case ID_DVD_MENU_RESUME:
			pDvdControl->Resume(dwFlags, NULL);
			break;

		default:
			return 0;
	}
	return 1;
}


bool isIFO(Titem *item)
{
	return !_tcsicmp(item->ext, _T("ifo"));
}

bool dvdItem(Titem *item)
{
	return isIFO(item) && !_tcsnicmp(item->file, dvdPath, dvdPathLen);
}

void dvdHiliteTitle()
{
	for(int i=0; i<playlist.len; i++){
		Titem *item= &playlist[i];
		if(dvdItem(item)){
			int t=getCDtrack(item);
			if(t==(g_bMenuOn ? 0 : g_ulCurTitle)){
				invalidateItem(currentFile);
				invalidateItem(i);
				currentFile=i;
				ensureVisible();
				return;
			}
		}
	}
}

//add track m to the end of playlist
void addTitle(int m)
{
	TCHAR buf[256], *s;

	if(m>=0 && m<=g_ulNumTitles) {
		lstrcpyn(buf, dvdPath, sizeA(buf)-16);
		s=_tcschr(buf, 0);
		if(s[-1]!='\\') *s++='\\';
		if(m==0){
			_tcscpy(s, dvdFirstPlayName);
		}
		else{
			_tcscpy(s, _T("Title00.ifo"));
			s[5]= (TCHAR)(m/10+'0');
			s[6]= (TCHAR)(m%10+'0');
		}
		addFile1(buf);
	}
}

//update playlist (when CD was changed)
void updateDVD()
{
	int s, d, top, m;

	if(!isDVD) return;
	top = SendMessage(listbox, LB_GETTOPINDEX, 0, 0);
	d=0;
	for(s=0; s<playlist.len; s++){
		Titem *item= &playlist[s];
		if(dvdItem(item) || !_tcsicmp(item->file, dvdFirstPlayName)){
			//delete item
			if(s<top) top--;
			item->free();
			continue;
		}
		//move item
		if(s==currentFile) currentFile=d;
		playlist[d++]=playlist[s];
	}
	playlist.setLen(d);

	numAdded=0;
	for(m=0; m<=g_ulNumTitles; m++){
		addTitle(m);
	}
	initList();
	SendMessage(listbox, LB_SETTOPINDEX, top+(m>>1), 0);
	dvdHiliteTitle();
}

bool dvdRate(double rate)
{
	HRESULT hr;
	if(rate>0) hr = pDvdControl->PlayForwards(rate, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
	else hr = pDvdControl->PlayBackwards(-rate, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
	if(FAILED(hr)) return false;
	return true;
}

HRESULT dvdRender(IGraphBuilder *_pGB, TCHAR *fileName)
{
	IBaseFilter *pBF;
	if(FAILED(pDvdControl->QueryInterface(IID_IBaseFilter, (void **)&pBF))){
		msg("IBaseFilter interface not found");
		return E_FAIL;
	}
	//set path
	cpStr(dvdPath, fileName);
	TCHAR *s=cutPath(dvdPath);
	if(s>dvdPath){
		s[-1]=0;
		dvdPathLen= _tcslen(dvdPath);
		convertT2W(dvdPath, w);
		pDvdControl->SetDVDDirectory(w);
	}
	_pGB->AddFilter(pBF, L"DVD Navigator");
	//render all pins
	IPin *pins[3];
	ULONG c;
	IEnumPins *pEnumPins;
	pBF->EnumPins(&pEnumPins);
	pBF->Release();
	pEnumPins->Next(sizeA(pins), pins, &c);
	pEnumPins->Release();
	int n=0;
	for(ULONG i=0; i<c; i++){
		if(SUCCEEDED(_pGB->Render(pins[i]))) n++;
		pins[i]->Release();
	}
	//check if there is a disc
	ULONGLONG id;
	HRESULT h= pDvdInfo->GetDiscID(0, &id);
	if(FAILED(h)) return h;
	//get path
	if(s==dvdPath){
		WCHAR buf[MAX_PATH];
		if(pDvdInfo->GetDVDDirectory(buf, sizeA(buf), &dvdPathLen)==S_OK){
			convertW2T(buf, t);
			cpStr(dvdPath, t);
		}
		else{
			s[0]=0;
		}
		dvdPathLen= _tcslen(dvdPath);
	}
	if(!n) return VFW_E_DVD_DECNOTENOUGH;
	return 0;
}

bool dvdSeek(LONGLONG pos)
{
	DVD_HMSF_TIMECODE t;
	ULONG flag;

	pDvdInfo->GetTotalTitleTime(&t, &flag);
	int f=UNITS/25;
	if(flag&DVD_TC_FLAG_DropFrame) f=UNITS*100/2997;
	else if(flag&DVD_TC_FLAG_30fps) f=UNITS/30;

	t.bHours= BYTE(pos/(3600*(LONGLONG)UNITS));
	t.bMinutes= BYTE((pos/(60*UNITS))%60);
	t.bSeconds= BYTE((pos/UNITS)%60);
	t.bFrames= BYTE((pos%UNITS)/f);
	return pDvdControl->PlayAtTime(&t, DVD_CMD_FLAG_Flush, 0)!=S_OK;
}


void dvdClose()
{
	//free DVD interfaces
	SAFE_RELEASE(pLine21Dec);
	SAFE_RELEASE(pDvdControl);
	SAFE_RELEASE(pDvdInfo);
	//delete the Menu Language LCID array
	delete[] g_pLanguageList;
	//clear global flags
	g_bDisplayCC = g_bDisplaySubpicture = 0;
	g_ulCurTitle = 0;
	g_bDisplaySubpicture = 0;
	isDVD=false;
	RemoveMenu(GetMenu(hWin), MENU_DVD, MF_BYPOSITION);
	DrawMenuBar(hWin);
}

int CheckSubmenuMessage(int wParam)
{
	if(wParam >= ID_DVD_SUBPICTURE_BASE && wParam <= ID_DVD_SUBPICTURE_MAX){
		SetSubpictureStream(wParam - ID_DVD_SUBPICTURE_BASE);
	}
	else if(wParam >= ID_DVD_AUDIO_BASE && wParam <= ID_DVD_AUDIO_MAX){
		SetAudioStream(wParam - ID_DVD_AUDIO_BASE);
	}
	else if(wParam >= ID_DVD_ANGLE_BASE && wParam <= ID_DVD_ANGLE_MAX){
		SetAngle(wParam - ID_DVD_ANGLE_BASE);
	}
	else if(wParam >= ID_DVD_TITLE_BASE && wParam <= ID_DVD_TITLE_MAX){
		SetTitle(wParam - ID_DVD_TITLE_BASE);
	}
	else if(wParam >= ID_DVD_CHAPTER_BASE && wParam <= ID_DVD_CHAPTER_MAX){
		SetChapter(wParam - ID_DVD_CHAPTER_BASE);
	}
	else if(wParam >= ID_DVD_MENULANG_BASE && wParam <= ID_DVD_MENULANG_MAX){
		SetMenuLanguage(wParam - ID_DVD_MENULANG_BASE);
	}
	else return 0;
	return 1;
}

LRESULT CALLBACK dvdWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	POINT pt;

	if(!g_bMenuOn) return 0;

	switch(message){

		case WM_MOUSEMOVE:
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			if(pDDXM) ClientToScreen(hWnd, &pt);
			pDvdControl->SelectAtPosition(pt);
			return 0;

		case WM_LBUTTONDOWN:
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			if(pDDXM) ClientToScreen(hWnd, &pt);
			if(pDvdControl->ActivateAtPosition(pt)!=S_OK) return 0;
			break;

		case WM_KEYUP:
			switch(wParam){
				case VK_RETURN:
					pDvdControl->ActivateButton();
					break;
				case VK_LEFT:
					pDvdControl->SelectRelativeButton(DVD_Relative_Left);
					break;
				case VK_RIGHT:
					pDvdControl->SelectRelativeButton(DVD_Relative_Right);
					break;
				case VK_UP:
					pDvdControl->SelectRelativeButton(DVD_Relative_Upper);
					break;
				case VK_DOWN:
					pDvdControl->SelectRelativeButton(DVD_Relative_Lower);
					break;
				default:
					return 0;
			}
			break;
		default:
			return 0;
	}
	return 1;
}

LRESULT CALLBACK dvdWndMainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message){
		case WM_INITMENUPOPUP:
			if((HMENU)wParam == ghChapterMenu){
				createChapterMenu();
				return 1;
			}
			break;

		case WM_COMMAND:
			if(dvdCommand(wParam)) return 1;
			return CheckSubmenuMessage((int)wParam);
	}
	return 0;
}

void dvdRestore()
{
	DVD_PLAYBACK_LOCATION2 o;
	if(SUCCEEDED(pDvdInfo->GetCurrentLocation(&o))){
		int st=state;
		CloseClip();
		noChangeTitle++;
		beginPlay();
		noChangeTitle--;
		pDvdControl->PlayAtTimeInTitle(o.TitleNum, &o.TimeCode, DVD_CMD_FLAG_Flush, 0);
		if(st==ID_FILE_PAUSE) pause();
	}
}
