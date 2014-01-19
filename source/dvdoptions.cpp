/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#include "hdr.h"
#include "tinyplay.h"
#include "dvd.h"


void SetDVDPlaybackOptions()
{
	// Disable Line21 (closed captioning) by default
	if(pLine21Dec) pLine21Dec->SetServiceState(AM_L21_CCSTATE_Off);

	// Don't reset DVD on stop.  This prevents the DVD from entering
	// DOMAIN_Stop when we stop playback or during resolution modes changes
	pDvdControl->SetOption(DVD_ResetOnStop, FALSE);

	// Ignore parental control
	pDvdControl->SetOption(DVD_NotifyParentalLevelChange, FALSE);

	// Use HMSF timecode format (instead of binary coded decimal)
	pDvdControl->SetOption(DVD_HMSF_TimeCodeEvents, TRUE);
}

void invalidUOPS(TCHAR *action)
{
	msg("Changing %s is not valid at this time (prohibited by DVD)",
		action);
}

//play the selected title
void SetTitle(int nTitle)
{
	HRESULT hr = pDvdControl->PlayTitle(nTitle, DVD_CMD_FLAG_Block, NULL);
	if(hr==VFW_E_DVD_OPERATION_INHIBITED) invalidUOPS(_T("Title"));
}

//play the selected chapter
void SetChapter(int nChapter)
{
	HRESULT hr = pDvdControl->PlayChapter(nChapter, DVD_CMD_FLAG_Block, NULL);
	if(hr==VFW_E_DVD_OPERATION_INHIBITED) invalidUOPS(_T("Chapter"));
}

void ClearSubmenu(HMENU hMenu)
{
	int n = GetMenuItemCount(hMenu);
	for(int i=0; i<n; i++)
		DeleteMenu(hMenu, 0, MF_BYPOSITION);
}

void createSubMenu(HMENU menu, char *text, int id, int num)
{
	ClearSubmenu(menu);
	for(int i=1; i<=num; i++){
		char s[16];
		sprintf(s, "%s %d", text, i);
		AppendMenuA(menu, MF_STRING, id+i, s);
	}
}

void checkSubMenu(int start, int current, int num)
{
	for(int i=0; i<=num; i++){
		CheckMenuItem(menuDVD, start + i,
			i==current ? MF_CHECKED : MF_UNCHECKED);
	}
}

void GetTitleInfo()
{
	DVD_DISC_SIDE discSide;
	ULONG dummy;

	// Read the number of titles available on this disc
	if(SUCCEEDED(pDvdInfo->GetDVDVolumeInfo(
		&dummy, &dummy, &discSide, (ULONG*)&g_ulNumTitles)))
	{
		createSubMenu(ghTitleMenu, "Title", ID_DVD_TITLE_BASE, g_ulNumTitles);
		CheckMenuItem(ghTitleMenu, 0, MF_CHECKED | MF_BYPOSITION);
	}
}

void UpdateCurrentTitle()
{
	checkSubMenu(ID_DVD_TITLE_BASE, g_ulCurTitle, g_ulNumTitles);
}

void createChapterMenu()
{
	ULONG ulNumChapters;
	DVD_PLAYBACK_LOCATION2 loc;

	if(SUCCEEDED(pDvdInfo->GetCurrentLocation(&loc))){
		if(SUCCEEDED(pDvdInfo->GetNumberOfChapters(loc.TitleNum, &ulNumChapters))){
			createSubMenu(ghChapterMenu, "Chapter", ID_DVD_CHAPTER_BASE, ulNumChapters);
			checkSubMenu(ID_DVD_CHAPTER_BASE, loc.ChapterNum, ulNumChapters);
		}
	}
}

// append a language item to a menu
void lcidItem(LCID lcid, HMENU menu, UINT item)
{
	TCHAR s[128];

	if(!lcid){
		_tcscpy(s, _T("(?)"));
	}
	else if(!GetLocaleInfo(lcid, LOCALE_SLANGUAGE, s, sizeA(s))){
		_stprintf(s, _T("0x%x"), lcid);
	}
	AppendMenu(menu, MF_STRING, item, s);
}

void EnableSubpicture()
{
	CheckMenuItem(menuDVD, ID_DVD_SHOWSUBPICTURE,
		g_bDisplaySubpicture ? MF_CHECKED : MF_UNCHECKED);
}

void GetSubpictureInfo()
{
	ULONG ulStreamsAvailable, ulCurrentStream;
	BOOL bIsDisabled;

	// Read the number of subpicture streams available
	if(SUCCEEDED(pDvdInfo->GetCurrentSubpicture(
		&ulStreamsAvailable, &ulCurrentStream, &bIsDisabled)))
	{
		// Update the on/off menu item
		g_bDisplaySubpicture = !bIsDisabled;
		EnableSubpicture();

		ClearSubmenu(ghSubpictureMenu);
		// Add an entry to the menu for each available subpicture stream
		for(ULONG i=0; i < ulStreamsAvailable; i++){
			LCID lcid;
			if(SUCCEEDED(pDvdInfo->GetSubpictureLanguage(i, &lcid)))
				lcidItem(lcid, ghSubpictureMenu, ID_DVD_SUBPICTURE_BASE + i);
		}
	}
	UpdateSubpictureInfo();
}

void UpdateSubpictureInfo()
{
	ULONG ulStreamsAvailable, ulCurrentStream;
	BOOL bIsDisabled;

	// Read the number of subpicture streams and the current state
	if(SUCCEEDED(pDvdInfo->GetCurrentSubpicture(&ulStreamsAvailable,
		&ulCurrentStream, &bIsDisabled)))
	{
		// Update the on/off menu item check
		g_bDisplaySubpicture = !bIsDisabled;
		CheckMenuItem(menuDVD, ID_DVD_SHOWSUBPICTURE,
			(g_bDisplaySubpicture) ? MF_CHECKED : MF_UNCHECKED);

		// Update the current subpicture language selection
		checkSubMenu(ID_DVD_SUBPICTURE_BASE, ulCurrentStream, ulStreamsAvailable);
	}
}

// Set the subpicture stream to the requested value
void SetSubpictureStream(int nStream)
{
	switch(pDvdControl->SelectSubpictureStream(nStream, DVD_CMD_FLAG_None, NULL)){
		case VFW_E_DVD_OPERATION_INHIBITED:
			invalidUOPS(_T("Subpicture Stream"));
			break;
		case VFW_E_DVD_INVALIDDOMAIN:
			msg("Can't change the subpicture stream in this domain.");
			break;
		case VFW_E_DVD_STREAM_DISABLED:
			msg("The selected stream (#%d) is currently disabled.", nStream+1);
			break;
	}
}

void GetAudioInfo()
{
	ULONG ulStreamsAvailable, ulCurrentStream;

	// Read the number of audio streams available
	if(SUCCEEDED(pDvdInfo->GetCurrentAudio(&ulStreamsAvailable, &ulCurrentStream)))
	{
		ClearSubmenu(ghAudioMenu);

		// Add an entry to the menu for each available audio stream
		for(ULONG i=0; i < ulStreamsAvailable; i++){
			LCID lcid;
			if(SUCCEEDED(pDvdInfo->GetAudioLanguage(i, &lcid)))
				lcidItem(lcid, ghAudioMenu, ID_DVD_AUDIO_BASE + i);
		}
	}
	UpdateAudioInfo();
}

// Update the current audio language selection
void UpdateAudioInfo()
{
	ULONG ulStreamsAvailable, ulCurrentStream;

	if(SUCCEEDED(pDvdInfo->GetCurrentAudio(&ulStreamsAvailable, &ulCurrentStream))){
		checkSubMenu(ID_DVD_AUDIO_BASE, ulCurrentStream, ulStreamsAvailable);
	}
}

void SetAudioStream(int nStream)
{
	HRESULT hr = pDvdControl->SelectAudioStream(nStream, DVD_CMD_FLAG_None, NULL);
	if(hr==VFW_E_DVD_OPERATION_INHIBITED) invalidUOPS(_T("Audio Stream"));
}

void EnableAngleMenu(BOOL bEnable)
{
	EnableMenuItem(menuDVD, MENU_Angle,
		(bEnable ? MF_ENABLED : MF_GRAYED) | MF_BYPOSITION);
	DrawMenuBar(hWin);
}

void GetAngleInfo()
{
	ULONG ulAnglesAvailable, ulCurrentAngle;

	// Read the number of angles available
	if(FAILED(pDvdInfo->GetCurrentAngle(&ulAnglesAvailable, &ulCurrentAngle))
		|| ulAnglesAvailable < 2){
		// An angle count of 1 means that the DVD is not in an 
		// angle block, so disable angle selection.
		EnableAngleMenu(FALSE);
	}
	else{
		// Enable angle selection with this menu
		EnableAngleMenu(TRUE);
		createSubMenu(ghAngleMenu, "Angle", ID_DVD_ANGLE_BASE, ulAnglesAvailable);
	}
	UpdateAngleInfo();
}

// Update the current angle selection
void UpdateAngleInfo()
{
	ULONG ulAnglesAvailable, ulCurrentAngle;

	if(SUCCEEDED(pDvdInfo->GetCurrentAngle(&ulAnglesAvailable, &ulCurrentAngle))){
		checkSubMenu(ID_DVD_ANGLE_BASE, ulCurrentAngle, ulAnglesAvailable);
	}
}

void SetAngle(int nAngle)
{
	HRESULT hr = pDvdControl->SelectAngle(nAngle, DVD_CMD_FLAG_None, NULL);
	if(hr==VFW_E_DVD_OPERATION_INHIBITED) invalidUOPS(_T("Angle"));
}

void GetMenuLanguageInfo()
{
	ULONG ulLanguagesAvailable;

	// Read the number of available menu languages
	if(FAILED(pDvdInfo->GetMenuLanguages(NULL, 100, &ulLanguagesAvailable)))
		return;

	// Allocate a language array large enough to hold the language list
	g_pLanguageList = new LCID[ulLanguagesAvailable];

	// Now fill the language array with the menu languages
	if(SUCCEEDED(pDvdInfo->GetMenuLanguages(
		g_pLanguageList, ulLanguagesAvailable, &ulLanguagesAvailable)))
	{
		ClearSubmenu(ghMenuLanguageMenu);

		// Add an entry to the menu for each available menu language
		for(ULONG i=0; i < ulLanguagesAvailable; i++){
			lcidItem(g_pLanguageList[i], ghMenuLanguageMenu, ID_DVD_MENULANG_BASE + i);
		}
		CheckMenuItem(ghMenuLanguageMenu, 0, MF_CHECKED | MF_BYPOSITION);
	}
}

void SetMenuLanguage(int nLanguageIndex)
{
	//changing menu language is only valid in the DVD_DOMAIN_Stop
	pDvdControl->SetOption(DVD_ResetOnStop, TRUE);
	if(SUCCEEDED(pDvdControl->Stop())){
		pDvdControl->SelectDefaultMenuLanguage(g_pLanguageList[nLanguageIndex]);
		//show root menu
		dvdMenu(DVD_MENU_Root);
	}
	pDvdControl->SetOption(DVD_ResetOnStop, FALSE);
}
