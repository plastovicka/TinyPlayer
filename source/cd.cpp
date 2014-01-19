/*
	(C) 2004-2005  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "tinyplay.h"
#include "cd.h"

#define MAX_FREEDB_LINE_LEN 256

enum{ COL_TRACK, COL_TITLE, COL_OFFSET, COL_LEN, COL_EXTT };


char *freedbCategories[]={
	"-", //uknown category for records which are locally edited or read from cdplayer.ini
	"blues",
	"classical",
	"country",
	"data",
	"folk",
	"jazz",
	"misc",
	"newage",
	"reggae",
	"rock",
	"soundtrack",
};

MCIDEVICEID cdDevice; //device handle
char cdLetter;        //drive letter
DWORD cdStartPos;     //0s when analog, 2s when digital
DWORD cdStartTime;    //tickCount time of playback start
int cdNext;
TCHAR mediaIdDec[16]; //CD identifier (MCI)
TCHAR cddbFolder[256];
int trackN;           //number of audio tracks
DWORD cdLen;
WNDPROC editWndProc;
HWND wndEdit;
int editItem, editCol;
const int cdFileSize=64000;
bool mciPlaying;

struct TcdStrValue {
	TCHAR *TcdInfo::*value;
	int id;
};

TcdInfo cd;

//------------------------------------------------------------------

BOOL CALLBACK multResultProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	int i, cmd;
	Darray<TmultipeResultInfo> *obj;
	static const int Mbuf=512;
	static TCHAR buf[Mbuf];
#ifndef _WIN32
	obj= (Darray<TmultipeResultInfo>*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
#else
	obj= (Darray<TmultipeResultInfo>*)GetWindowLong(hWnd, GWL_USERDATA);
#endif
	HWND list = GetDlgItem(hWnd, IDC_MULTRESULT);

	switch(mesg) {
		case WM_INITDIALOG:
			obj= (Darray<TmultipeResultInfo>*)lP;
#ifndef _WIN32
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)obj);
#else
			SetWindowLong(hWnd, GWL_USERDATA, (LONG)obj);
#endif
			setDlgTexts(hWnd, 15);
			buf[Mbuf-1]=0;
			for(i=0; i<obj->len; i++){
				_sntprintf(buf, Mbuf-1, _T("%s\t%s\t%s"),
					(*obj)[i].category, (*obj)[i].discId, (*obj)[i].description);
				SendMessage(list, LB_ADDSTRING, 0, (LPARAM)buf);
			}
			return TRUE;

		case WM_COMMAND:
			cmd=LOWORD(wP);
			switch(cmd){
				case IDC_MULTRESULT:
					if(HIWORD(wP)!=LBN_DBLCLK) break;
					//!
				case IDOK:
					EndDialog(hWnd, SendMessage(list, LB_GETCURSEL, 0, 0));
					break;
				case IDCANCEL:
					EndDialog(hWnd, -1);
					break;
			}
			break;
	}
	return FALSE;
}

int chooseFreedbResult(Darray<TmultipeResultInfo> &r)
{
	return DialogBoxParam(inst, MAKEINTRESOURCE(IDD_MULTCD), hWin, (DLGPROC)multResultProc, (LPARAM)&r);
}

//------------------------------------------------------------------

void cpStr(char *&dest, char *src)
{
	char *old=dest;
	dest = dupStr(src);
	delete[] old;
}

void cpStr(WCHAR *&dest, WCHAR *src)
{
	WCHAR *old=dest;
	dest = dupStr(src);
	delete[] old;
}

void delStr(TCHAR *&s)
{
	delete[] s;
	s=0;
}

int strn2cmp(char *s1, char *s2)
{
	return strncmp(s1, s2, strlen(s2));
}

int msf2s(DWORD t)
{
	return MCI_MSF_MINUTE(t)*60 + MCI_MSF_SECOND(t);
}

int msf2f(DWORD t)
{
	return msf2s(t)*75+MCI_MSF_FRAME(t);
}

int hexDigit(char c)
{
	if(c>='0' && c<='9') return c-'0';
	if(c>='a' && c<='z') return c-'a'+10;
	if(c>='A' && c<='Z') return c-'A'+10;
	return -1;
}

int cmphex2(char *s1, char *s2)
{
	return ((hexDigit(s1[0])-hexDigit(s2[0]))<<4) +
		hexDigit(s1[1])-hexDigit(s2[1]);
}

int cmphex2(WCHAR *t1, WCHAR *t2)
{
	char s1[2], s2[2];
	s1[0]=(char)t1[0];
	s1[1]=(char)t1[1];
	s2[0]=(char)t2[0];
	s2[1]=(char)t2[1];
	return cmphex2(s1, s2);
}


bool isRange(TCHAR *s)
{
	return _tcslen(s)==6 && (s[2]=='t' || s[2]=='T') &&
		(s[3]=='o' || s[3]=='O');
}

TCHAR *anul(TCHAR *s)
{
	return s ? s : _T("");
}
//------------------------------------------------------------------

//add line "key=value"
void cdAddLine(Darray<char> &s, char *key, TCHAR *valueT)
{
	char c;
	char *value, *valueU;
	size_t len, n, maxLen;

	if(!valueT){
		dtprintf(s, "%s=\r\n", key);
		return;
	}
	//convert to UTF-8
	len= _tcslen(valueT)+1;
	maxLen= 3*len;
	value= valueU= new char[maxLen];
#ifdef UNICODE
	WideCharToMultiByte(CP_UTF8, 0, valueT, -1, valueU, maxLen, 0, 0);
#else
	WCHAR *w= new WCHAR[len];
	MultiByteToWideChar(CP_ACP,0,valueT,-1, w,len);
	WideCharToMultiByte(CP_UTF8,0,w,-1, valueU,maxLen,0,0);
	delete[] w;
#endif

	//split long lines
	len=MAX_FREEDB_LINE_LEN-4-strlen(key);
	do{
		n=strlen(value);
		if(n>len) n=len;
		for(;;){
			c=value[n];
			if(BYTE(c & 0xC0)!=0x80 || n<=1)  break;
			//don't split one UTF-8 character into two lines
			n--;
		}
		value[n]=0;
		dtprintf(s, "%s=%s\r\n", key, value);
		value+=n;
		value[0]=c;
	} while(c);

	delete[] valueU;
}

//print record as UTF-8 string
void TcdInfo::wrToString(Darray<char> &s)
{
	int i;

	dtprintf(s, "# xmcd\r\n#\r\n# Track frame offsets:\r\n");
	for(i=1; i<=trackN2; i++){
		dtprintf(s, "# %d\r\n", Tracks[i].start);
	}
	convertT2A(discId, discIdA);
	if(!SubmittedVia) cpStr(SubmittedVia, _T("not submitted"));
	convertT2A(SubmittedVia, SubmittedViaA);
	dtprintf(s, "#\r\n# Disc length: %d seconds\r\n#\r\n# Revision: %d\r\n# Submitted via: %s\r\n#\r\nDISCID=%s\r\n",
		total, revision, SubmittedViaA, discIdA);
	TCHAR *aa= getArtistAlbum();
	cdAddLine(s, "DTITLE", aa);
	delete[] aa;
	cdAddLine(s, "DYEAR", Year);
	cdAddLine(s, "DGENRE", Genre);
	for(i=1; i<=trackN2; i++){
		char ttit[16];
		sprintf(ttit, "TTITLE%d", i-1);
		cdAddLine(s, ttit, Tracks[i].title);
	}
	cdAddLine(s, "EXTD", Extd);
	for(i=1; i<=trackN2; i++){
		char ext[16];
		sprintf(ext, "EXTT%d", i-1);
		cdAddLine(s, ext, Tracks[i].extt);
	}
	cdAddLine(s, "PLAYORDER", Order);
}

void errNoDb()
{
	msglng(862, "You have to select a folder in which is your local CD database (menu File / Options / LocalCDDB)");
}

TCHAR *TcdInfo::getCategoryFolder(TCHAR *fn)
{
	if(!Category || !cddbFolder[0]) return 0;
	_tcscpy(fn, cddbFolder);
	TCHAR *cat= _tcschr(fn, 0);
	if(cat[-1]!='\\') *cat++='\\';
	_tcscpy(cat, Category);
	TCHAR *filePart= _tcschr(cat, 0);
	*filePart++='\\';
	*filePart=0;
	return filePart;
}

int __cdecl cmdId2(const void *a, const void *b)
{
	return cmphex2((char*)a, (char*)b);
}

inline void cp2(char *d, char *s)
{
	*(WORD*)(d)=*(WORD*)(s);
}

inline void cp2(WCHAR *d, WCHAR *s)
{
	*(DWORD*)(d)=*(DWORD*)(s);
}

bool splitFile(HANDLE f, DWORD size, TCHAR *fn, TCHAR *filePart, TCHAR *fn2)
{
	char *mid, *big, key[2], *prev, *next, (*dd)[2];
	TCHAR *filePart2;
	int splitter;
	DWORD w, w0;
	HANDLE f2;
	int err=0;
	bool result=false;

	if(filePart[0]!=filePart[4] || filePart[1]!=filePart[5]){
		//read whole file into memory
		SetFilePointer(f, 0, 0, FILE_BEGIN);
		big= new char[size+2];
		big[size]=0;
		if(ReadFile(f, big, size, &w, 0) && w==size){
			SetFilePointer(f, 0, 0, FILE_BEGIN);
			//extract all discIDs from the file
			Darray<char[2]> idA;
			for(mid=big;;){
				mid= strstr(mid, "#FILENAME=");
				if(!mid) break;
				mid+=10;
				dd=idA++;
				cp2(*dd, mid);
			}
			if(idA.len>1){
				//sort IDs and get median
				qsort((char(*)[2])idA, idA.len, 2, cmdId2);
				splitter=idA.len/2;
				do{
					cp2(key, idA[splitter]);
					splitter++;
				} while(key[0]==filePart[0] && key[1]==filePart[1]);
				do{
					splitter--;
				} while(!cmphex2(idA[splitter], key));
				//create the other file
				_tcscpy(fn2, fn);
				filePart2= fn2 + (filePart-fn);
				filePart2[0]=key[0];
				filePart2[1]=key[1];
				f2=CreateFile(fn2, GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
				if(f2!=INVALID_HANDLE_VALUE){
					//write both files
					for(prev=0, mid=big;;){
						next= strstr(mid, "#FILENAME=");
						if(!next) next=big+size;
						if(prev){
							w0=DWORD(next-prev);
							WriteFile(cmphex2(mid, key)<0 ? f : f2, prev, w0, &w, 0);
							if(w0!=w) err++;
						}
						if(!*next) break;
						prev=next;
						mid=next+10;
					}
					CloseHandle(f2);
					if(err){
						//reconstruct the old file if error occured
						DeleteFile(fn2);
						SetFilePointer(f, 0, 0, FILE_BEGIN);
						WriteFile(f, big, size, &w, 0);
					}
					else{
						SetEndOfFile(f);
						_tcscpy(filePart2, filePart);
						filePart2[4]= idA[splitter][0];
						filePart2[5]= idA[splitter][1];
						result=true;
					}
				}
			}
		}
		delete[] big;
	}
	return result;
}

void TcdInfo::saveToLocalCDDB(char *contentU)
{
	TCHAR *filePart, *filePart2, *s;
	HANDLE f, h2;
	char *content;
	WCHAR *contentW;
	DWORD w;
	int d, err=0, dist, dataLen, len;
	bool isSplit=false;
	static WIN32_FIND_DATA fd2;
	static const int Mbuf=512;
	TCHAR *fn= (TCHAR*)_alloca(Mbuf*4);
	TCHAR *fn2= fn+Mbuf;

	deleteFromLocalCDDB();
	content=contentU;
	dataLen= strlen(content);
	if(CDDBenc==0){
		//convert from UTF-8 to active code page
		len= dataLen+1;
		contentW= new WCHAR[len];
		MultiByteToWideChar(CP_UTF8, 0, contentU, -1, contentW, len);
		content= new char[len];
		WideCharToMultiByte(CP_ACP, 0, contentW, -1, content, len, 0, 0);
		delete[] contentW;
		dataLen= strlen(content);
	}
	filePart=getCategoryFolder(fn);
	if(filePart){
		CreateDirectory(fn, 0);
		if(unixCDDB){
			_tcscpy(filePart, discId);
			f=CreateFile(fn, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
		}
		else{
			//find a file which has range near the discId
			_tcscpy(filePart, _T("*"));
			h2= FindFirstFile(fn, &fd2);
			cp2(filePart, discId);
			cp2(filePart+4, discId);
			filePart[2]='t'; filePart[3]='o';
			filePart[6]=0;
			if(h2!=INVALID_HANDLE_VALUE){
				dist=9999;
				do{
					if(!(fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
						s=fd2.cFileName;
						if(isRange(s)){
							d=max(cmphex2(s, discId), cmphex2(discId, s+4));
							if(d<dist){
								dist=d;
								_tcscpy(filePart, s);
							}
						}
					}
				} while(FindNextFile(h2, &fd2));
				FindClose(h2);
			}
			//append a new record to the end of file
			f=CreateFile(fn, GENERIC_WRITE|GENERIC_READ, 0, 0, OPEN_ALWAYS, 0, 0);
			SetFilePointer(f, 0, 0, FILE_END);
			char head[21];
			convertT2A(discId, discIdA);
			sprintf(head, "#FILENAME=%s\r\n", discIdA);
			WriteFile(f, head, 20, &w, 0);
			if(w!=20) err++;
		}
		WriteFile(f, content, dataLen, &w, 0);
		if((int)w!=dataLen) err++;
		DWORD size=GetFileSize(f, 0);
		if(size>cdFileSize && size<10000000 && !err){
			isSplit= splitFile(f, size, fn, filePart, fn2);
		}
		if(!CloseHandle(f)) err++;
		if(isSplit){
			MoveFile(fn, fn2);
		}
		else if(!unixCDDB){
			_tcscpy(fn2, fn);
			filePart2= fn2 + (filePart-fn);
			if(cmphex2(filePart, discId)>0){
				cp2(filePart2, discId);
				MoveFile(fn, fn2);
			}
			if(cmphex2(filePart+4, discId)<0){
				cp2(filePart2+4, discId);
				MoveFile(fn, fn2);
			}
		}
		if(err) msglng(734, "Write failed: %s", fn);
	}
	if(content!=contentU) delete[] content;
}

void TcdInfo::saveToLocalCDDB()
{
	Darray<char> s;
	wrToString(s);
	saveToLocalCDDB(s);
}

int TcdInfo::deleteFromLocalCDDB()
{
	TCHAR *filePart, *s;
	HANDLE h2, f;
	int result=0;
	DWORD size, r, w;
	char *big, *p, *e;
	static WIN32_FIND_DATA fd2;
	static const int Mbuf=512;
	TCHAR *fn= (TCHAR*)_alloca(Mbuf*2);

	modif=false;
	convertT2A(discId, discIdA);
	filePart=getCategoryFolder(fn);
	if(!filePart) return 0;
	*filePart='*';
	filePart[1]=0;
	h2= FindFirstFile(fn, &fd2);
	if(h2!=INVALID_HANDLE_VALUE){
		do{
			if(!(fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
				s=fd2.cFileName;
				if(!_tcsicmp(s, discId)){
					//unix format
					result=1;
					_tcscpy(filePart, s);
					DeleteFile(fn);
				}
				if(isRange(s) &&
					 cmphex2(s, discId)<=0 && cmphex2(s+4, discId)>=0){
					//windows format
					_tcscpy(filePart, s);
					f=CreateFile(fn, GENERIC_WRITE|GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
					if(f!=INVALID_HANDLE_VALUE){
						size= GetFileSize(f, 0);
						if(size<10000000){
							big= new char[size+8];
							big[size]=0;
							ReadFile(f, big, size, &r, 0);
							if(r==size){
								p=big;
								for(;;){
									p= strstr(p, "#FILENAME=");
									if(!p) break;
									p+=10;
									if(!_strnicmp(discIdA, p, 8)){
										//record found
										result=1;
										e= strstr(p, "#FILENAME=");
										p-=10;
										SetFilePointer(f, p-big, 0, FILE_BEGIN);
										if(e){
											WriteFile(f, e, size-(e-big), &w, 0);
										}
										SetEndOfFile(f);
										size= GetFileSize(f, 0);
										break;
									}
								}
							}
							delete[] big;
						}
						CloseHandle(f);
						if(size==0) DeleteFile(fn);
					}
				}
			}
		} while(FindNextFile(h2, &fd2));
		FindClose(h2);
	}
	return result;
}

//------------------------------------------------------------------
void appendStr(TCHAR *&d, TCHAR *s)
{
	if(!d){
		cpStr(d, s);
	}
	else{
		TCHAR *v= new TCHAR[_tcslen(d)+_tcslen(s)+1];
		_tcscpy(v, d);
		_tcscat(v, s);
		delete[] d;
		d=v;
	}
}

void TcdInfo::parseLine(char *key, char *valueE)
{
	if(!valueE[0]) return;
	convertA2W(enc, valueE, w);
	convertW2T(w, value);
	bool e=false;
	if(!strn2cmp(key, "TTITLE")) key+=6;
	if(!strn2cmp(key, "EXTT")){ key+=4; e=true; }
	if(key[0]>='0' && key[0]<='9'){
		int i= atoi(key)+1;
		if(i>0 && i<Mtrack){
			appendStr(e ? Tracks[i].extt : Tracks[i].title, value);
		}
	}
	else if(!strcmp(key, "ARTIST")){
		appendStr(Artist, value);
	}
	else if(!strcmp(key, "TITLE")){
		appendStr(Album, value);
	}
	else if(!strcmp(key, "YEAR") || !strcmp(key, "DYEAR")){
		cpStr(Year, value);
	}
	else if(!strcmp(key, "GENRE") || !strcmp(key, "DGENRE")){
		appendStr(Genre, value);
	}
	else if(!strcmp(key, "ORDER") || !strcmp(key, "PLAYORDER")){
		appendStr(Order, value);
	}
	else if(!strcmp(key, "EXTD")){
		appendStr(Extd, value);
	}
	else if(!strcmp(key, "DTITLE")){
		appendStr(Dtitle, value);
	}
	else if(!strcmp(key, "# REVISION")){
		revision= _ttoi(value);
	}
	else if(!strcmp(key, "# TRACK FRAME OFFSETS")){
		trackOffset=1;
	}
	else if(!strcmp(key, "# SUBMITTED VIA")){
		while(*value==' ') value++;
		cpStr(SubmittedVia, value);
	}
}

TCHAR *TcdInfo::getArtistAlbum()
{
	size_t len= 4;
	if(Artist) len+= _tcslen(Artist);
	if(Album) len+= _tcslen(Album);
	TCHAR *s= new TCHAR[len];
	s[0]=0;
	if(Artist){
		_tcscat(s, Artist);
		if(Album) _tcscat(s, _T(" / "));
	}
	if(Album){
		_tcscat(s, Album);
	}
	return s;
}

void TcdInfo::parse(int(*get)(void*), void *param, TCHAR *cat)
{
	int c;
	char *d;
	TmultipeResultInfo *r;
	bool reseted=false;
	static const int Mbuf=256;
	char *key= (char*)_alloca(Mbuf*2);
	char *value= key+Mbuf;

	trackOffset=0;

	for(;;){
		while(c=get(param), c=='\n' || c=='\r');
		c=toupper(c);
		if(c=='[' || c==EOF) break;
		d=key;
		while(c!='=' && c!=':' && c!='\n' && c!='\r' && c!=EOF){
			*d= (char)c;
			c=toupper(get(param));
			if(d<key+Mbuf-1) d++;
		}
		*d=0;
		if(c=='=' || c==':'){
			if(!reseted && c=='='){
				reseted=true;
				resetFields();
			}
			d=value;
			for(;;){
				c=get(param);
				if(c=='\n' || c=='\r' || c==EOF) break;
				*d= (char)c;
				if(d<value+Mbuf-1) d++;
			}
			*d=0;
			if(!strcmp(key, "#FILENAME")) break;
			parseLine(key, value);
		}
		else if(trackOffset){
			char *e;
			unsigned n=strtoul(key+1, &e, 10);
			if(e!=key+1 && key[0]=='#'){
				if(n!=Tracks[trackOffset].start){
					//match is not exact
					if(isExact) return;
				}
				if(++trackOffset>trackN2){
					trackOffset=0;
					if(!isExact) reset();
					isExact=true;
				}
			}
		}
	}
	//convert DTITLE to Artist,Album
	if(Dtitle){
		TCHAR *t, *a;
		for(t=Dtitle; (t[0]!=' ' || t[1]!='/' || t[2]!=' ') && *t; t++);
		a=Dtitle;
		if(*t){
			a=t+3;
			*t=0;
		}
		cpStr(Artist, Dtitle);
		cpStr(Album, a);
	}
	//add record to results list
	r= results++;
	r->category=0;
	cpStr(r->category, cat);
	r->discId=0;
	cpStr(r->discId, discId);
	r->description= getArtistAlbum();
}

void TcdInfo::parse(FILE *f, TCHAR *cat)
{
	parse((int(*)(void*))fgetc, (void*)f, cat);
}

int fcmp(FILE *f, TCHAR *s)
{
	int c;
	while((c=toupper(fgetc(f)))==toupper(*s)) s++;
	if(*s) return -1;
	return c;
}

void TcdInfo::readCdplayerIni()
{
	int c;
	TCHAR fn[64];

	resetResult();
	GetWindowsDirectory(fn, sizeA(fn)-14);
	_tcscat(fn, _T("\\cdplayer.ini"));
	FILE *f= _tfopen(fn, _T("rb"));
	if(!f) return;
	for(;;){
		c=fgetc(f);
		if(c==EOF) break;
		if(c=='[' && fcmp(f, mediaIdHex)==']'){
			enc=CP_ACP;
			parse(f, _T("-"));
		}
	}
	fclose(f);
	if(results.len>0) cpStr(Category, results[0].category);
}

void TcdInfo::openCDDBfile(TCHAR *fn, TCHAR *cat)
{
	FILE *f= _tfopen(fn, _T("rb"));
	if(!f) return;
	TCHAR o= _tcschr(fn, 0)[-3];
	if(o=='o' || o=='O'){
		int c;
		do{
			c=fgetc(f);
			if(c==EOF) goto le;
		} while(!(c=='#' && fcmp(f, _T("FILENAME"))=='=' &&
			(c=fcmp(f, discId), c=='\n' || c=='\r')));
	}
	enc= CDDBenc ? CP_UTF8 : CP_ACP;
	parse(f, cat);
le:
	fclose(f);
}

int getBufCh(BYTE *&s)
{
	if(!*s) return EOF;
	return *s++;
}

void TcdInfo::rdFromString(char *s, TCHAR *cat)
{
	enc= CP_UTF8;
	parse((int(*)(void*))getBufCh, &s, cat);
}

void TcdInfo::readLocalCDDB(TCHAR *mask)
{
	HANDLE h1, h2;
	static WIN32_FIND_DATA fd1, fd2;
	static const int Mbuf=512;
	TCHAR *buf= (TCHAR*)_alloca(Mbuf*2);
	TCHAR *cat, *fn, *s;

	if(!cddbFolder[0]) return;
	isExact=false;
	_tcscpy(buf, cddbFolder);
	cat= _tcschr(buf, 0);
	if(buf[0] && cat[-1]!='\\'){
		*cat++='\\';
	}
	_tcscpy(cat, mask ? mask : _T("*"));
	resetResult();
	h1= FindFirstFile(buf, &fd1);
	if(h1!=INVALID_HANDLE_VALUE){
		do{
			if((fd1.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd1.cFileName[0]!='.'){
				_tcscpy(cat, fd1.cFileName);
				s=_tcschr(cat, 0);
				*s++='\\';
				*s='*';
				s[1]=0;
				h2= FindFirstFile(buf, &fd2);
				if(h2!=INVALID_HANDLE_VALUE){
					*s++='\\';
					do{
						if(!(fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
							fn=fd2.cFileName;
							if(!_tcsicmp(fn, discId) || _tcslen(fn)==6 &&
								 cmphex2(fn, discId)<=0 && cmphex2(fn+4, discId)>=0 &&
								 (fn[2]=='t' || fn[2]=='T') && (fn[3]=='o' || fn[3]=='O')){
								_tcscpy(s, fn);
								openCDDBfile(buf, fd1.cFileName);
							}
						}
					} while(FindNextFile(h2, &fd2));
					FindClose(h2);
				}
			}
		} while(FindNextFile(h1, &fd1));
		FindClose(h1);
	}
	if(results.len==0) return;
	if(results.len>1 && !mask){
		//more results
		int which= chooseFreedbResult(results);
		if(which>=0) readLocalCDDB(results[which].category);
	}
	else{
		cpStr(Category, results[results.len-1].category);
	}
}


void TcdInfo::resetResult()
{
	for(int i=0; i<results.len; i++){
		delete[] results[i].category;
		delete[] results[i].discId;
		delete[] results[i].description;
	}
	results.reset();
}

void TcdInfo::resetFields()
{
	for(int i=1; i<Mtrack; i++){
		delStr(Tracks[i].title);
		delStr(Tracks[i].extt);
	}
	delStr(Artist);
	delStr(Album);
	delStr(Year);
	delStr(Genre);
	delStr(Extd);
	delStr(Order);
	delStr(Dtitle);
	modif=false;
}

void TcdInfo::reset()
{
	cpStr(Category, _T("-"));
	delStr(SubmittedVia);
	revision=-1;
	resetFields();
	resetResult();
}

void TcdInfo::findInfo()
{
	readLocalCDDB();
	if(!results.len){
		readCdplayerIni();
	}
	if(!results.len && autoConnect){
		noMsg++;
		getFromFreedb(true);
		noMsg--;
	}
	if(!results.len && cdTextOn){
		getCDText();
	}
}

//------------------------------------------------------------------

bool isCDA(Titem *item)
{
	return !_tcsicmp(item->ext, _T("cda"));
}

//extract track number from a file name
int getCDtrack(Titem *item)
{
	TCHAR *s= item->ext-3;
	if(*s<'0' || *s>'9') return 0;
	return (s[0]-'0')*10 + s[1]-'0';
}

//get track number of the current item
int getCDtrack()
{
	return getCDtrack(&playlist[currentFile]);
}

//get current position and convert TMSF time to 100ns time
bool cdPos()
{
	if(cdAudioFilter){
		return ripPos();
	}
	else{
		MCI_STATUS_PARMS msp;
		msp.dwItem= MCI_STATUS_POSITION;
		if(mciSendCommand(cdDevice, MCI_STATUS, MCI_STATUS_ITEM, (WPARAM)&msp)) return false;
		DWORD t= msp.dwReturn;
		DWORD s= MCI_TMSF_MINUTE(t)*60 + MCI_TMSF_SECOND(t);
		//fix the 2 seconds bug when digital playback is enabled
		if(cdStartTime){
			cdStartPos=0;
			if(GetTickCount()-cdStartTime+1000 < 1000*s) cdStartPos=2;
			cdStartTime=0;
		}
		position= UNITS*(LONGLONG)(s-cdStartPos) + UNITS/75*MCI_TMSF_FRAME(t);
		int track= MCI_TMSF_TRACK(t);
		if(track!=getCDtrack()){
			cdNext=track;
			nextClip();
			cdNext=0;
		}
		return true;
	}
}

//fill in the track length and title
void updateTrack(Titem *item, int track)
{
	if(track>0 && track<Mtrack){
		item->length= (UNITS*(LONGLONG)cd.Tracks[track].length)/75;
		item->size= 2352*cd.Tracks[track].length;
		item->bitrate= 1411;
		cpStr(item->tag, _T("CD"));
		TCHAR buf[4];
		_stprintf(buf, _T("%02d"), track);
		cpStr(item->track, buf);
		cpStr(item->title, cd.Tracks[track].title);
		cpStr(item->artist, cd.Artist);
		cpStr(item->album, cd.Album);
		cpStr(item->year, cd.Year);
		cpStr(item->genre, cd.Genre);
		cpStr(item->comment, cd.Tracks[track].extt);
	}
}


//play or seek
MCIERROR cdPlay(DWORD from)
{
	int t= getCDtrack();
	MCI_PLAY_PARMS mpp;
	mpp.dwFrom=from+t;
	mpp.dwTo= t+1;
	mpp.dwCallback= (DWORD)hWin;
	DWORD param= MCI_FROM|MCI_NOTIFY;
	if(t<trackN){
		Titem *nxt= &playlist[currentFile+1];
		if(repeat==2 || stopAfterCurrent ||
			currentFile+1>=playlist.len || !isCDA(nxt) || getCDtrack(nxt)!=t+1){
			param=MCI_FROM|MCI_TO|MCI_NOTIFY;
		}
	}
	MCIERROR result= mciSendCommand(cdDevice, MCI_PLAY, param, (WPARAM)&mpp);
	if(!result){
		mciPlaying=true;
		playBtn(ID_FILE_PLAY);
	}
	return result;
}

//close cd device
void closeCD()
{
	if(cdDevice){
		stopCD();
		MCI_GENERIC_PARMS m;
		mciSendCommand(cdDevice, MCI_CLOSE, 0, (WPARAM)&m);
		cdDevice=0;
	}
}

void TcdInfo::computeDiscID()
{
	int i, j, n, t;

	n=0;
	for(i=1; i<=trackN2; i++){
		j= Tracks[i].start/75;
		while(j>0){
			n += j%10;
			j/=10;
		}
	}
	t= total - Tracks[1].start/75;

	_stprintf(discId, _T("%08x"), (((n % 0xFF)<<24) | (t<<8) | trackN2));
}

//open CD device and get TOC
int openCD(TCHAR d)
{
	int i;
	MCIERROR e;

	if(cdDevice){
		if(d==cdLetter) return 0;
		closeCD();
	}
	trackN=0;
	cdLen=0;

	TCHAR drive[3];
	MCI_OPEN_PARMS m;
	m.dwCallback=0;
	m.lpstrDeviceType=_T("CDAudio");
	drive[0]=d;
	drive[1]=':';
	drive[2]=0;
	m.lpstrElementName=drive;
	m.lpstrAlias=0;
	e=mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE|MCI_OPEN_ELEMENT, (WPARAM)&m);
	if(e) return e;
	cdDevice= m.wDeviceID;
	cdLetter= (char)toupper(d);

	MCI_SET_PARMS mtp;
	mtp.dwTimeFormat= MCI_FORMAT_MSF;
	mciSendCommand(cdDevice, MCI_SET, MCI_SET_TIME_FORMAT, (WPARAM)&mtp);

	MCI_STATUS_PARMS msp;
	msp.dwItem= MCI_STATUS_NUMBER_OF_TRACKS;
	e=mciSendCommand(cdDevice, MCI_STATUS, MCI_STATUS_ITEM, (WPARAM)&msp);
	if(e){ closeCD(); return 2; }
	if(msp.dwReturn>=Mtrack) msp.dwReturn= Mtrack-1;
	trackN= cd.trackN2= msp.dwReturn;

	msp.dwItem= MCI_STATUS_LENGTH;
	if(!mciSendCommand(cdDevice, MCI_STATUS, MCI_STATUS_ITEM, (WPARAM)&msp)){
		cdLen=msp.dwReturn;
	}
	//get MCI track offsets of audio tracks
	bool err=false;
	for(i=1; i<=trackN; i++){
		msp.dwTrack= i;
		msp.dwItem= MCI_STATUS_LENGTH;
		if(!mciSendCommand(cdDevice, MCI_STATUS, MCI_STATUS_ITEM|MCI_TRACK, (WPARAM)&msp)){
			cd.Tracks[i].length= msf2f(msp.dwReturn);
		}
		else{
			err=true;
		}
		msp.dwItem= MCI_STATUS_POSITION;
		if(!mciSendCommand(cdDevice, MCI_STATUS, MCI_STATUS_ITEM|MCI_TRACK, (WPARAM)&msp)){
			cd.Tracks[i].start= msf2f(msp.dwReturn);
		}
		else{
			err=true;
		}
	}
	//compute lead-out position
	cd.Tracks[cd.trackN2+1].start= cd.Tracks[cd.trackN2].start + cd.Tracks[cd.trackN2].length +1;
	//data track
	if(trackN>0){
		msp.dwTrack=trackN;
		msp.dwItem=MCI_CDA_STATUS_TYPE_TRACK;
		if(!mciSendCommand(cdDevice, MCI_STATUS, MCI_STATUS_ITEM|MCI_TRACK, (WPARAM)&msp)){
			if(msp.dwReturn!=MCI_CDA_TRACK_AUDIO) trackN--;
		}
	}
	//try to get NtSCSI or ASPI track offsets (it rewrites MCI values)
	cd.getTOC();
	//get disc identifier
	cd.discId[0]=0;
	cd.total= cd.Tracks[cd.trackN2+1].start/75;
	if(!err) cd.computeDiscID();

	TCHAR id[16];
	MCI_INFO_PARMS mip;
	mip.lpstrReturn=id;
	mip.dwRetSize=16;
	mciSendCommand(cdDevice, MCI_INFO, MCI_INFO_MEDIA_IDENTITY, (WPARAM)&mip);

	mtp.dwTimeFormat= MCI_FORMAT_TMSF;
	mciSendCommand(cdDevice, MCI_SET, MCI_SET_TIME_FORMAT, (WPARAM)&mtp);

	if(_tcscmp(mediaIdDec, id)){
		//CD has been changed -> reload information
		lstrcpy(mediaIdDec, id);
		_stprintf(cd.mediaIdHex, _T("%X"), _ttoi(mediaIdDec));
		delStr(cd.inetCategory);
		cd.reset();
		for(i=trackN+1; i<=cd.trackN2; i++){
			cpStr(cd.Tracks[i].title, _T("data"));
		}
		if(trackN>0) cd.findInfo();
		PostMessage(hWin, WM_COMMAND, 900, 0);
	}
	if(trackN==0){ closeCD(); return 3; }
	return 0;
}

//add track m to the end of playlist
void addTrack(int m)
{
	TCHAR buf[24];

	if(m>0 && m<=trackN) {
		buf[0]=cdLetter;
		lstrcpy(buf+1, _T(":\\Track"));
		buf[8]= (TCHAR)(m/10+'0');
		buf[9]= (TCHAR)(m%10+'0');
		lstrcpy(buf+10, _T(".cda"));
		if(!addFile1(buf)){
			updateTrack(&playlist[playlist.len-1], m);
		}
	}
}

//update playlist (when CD was changed)
void updateCD()
{
	int s, d, top, i, m;

	top = SendMessage(listbox, LB_GETTOPINDEX, 0, 0);
	m=0;
	d=0;
	for(s=0; s<playlist.len; s++){
		Titem *item= &playlist[s];
		if(toupper(item->file[0])==cdLetter && isCDA(item)){
			i= getCDtrack(item);
			if(i>trackN){
				//delete item
				if(s<top) top--;
				item->free();
				continue;
			}
			if(i>m) m=i;
			updateTrack(item, i);
		}
		//move item
		if(s==currentFile) currentFile=d;
		playlist[d++]=playlist[s];
	}
	playlist.setLen(d);

	numAdded=0;
	while(++m<=trackN){
		addTrack(m);
	}
	initList();
	SendMessage(listbox, LB_SETTOPINDEX, top, 0);
}

void cdErr(int err)
{
	if(err==2){
		msglng(790, "There is no CD in the drive");
	}
	else if(err==3){
		msglng(865, "There is no audio track on the CD");
	}
	else{
		msglng(867, "Cannot access CD drive (error %d)", err);
	}
}

bool cdFindDrive()
{
	TCHAR d[3];
	int err=0, e;

	//find CD drive which is ready
	d[1]=':';
	d[2]=0;
	for(d[0]='A';; d[0]++){
		if(GetDriveType(d)==DRIVE_CDROM){
			e=openCD(d[0]);
			if(!e) break;
			amin(err, e);
		}
		if(d[0]=='Z'){
			cdErr(err);
			return false;
		}
	}
	return true;
}

//Play audio CD (command in the menu)
void playCD()
{
	if(cdFindDrive()){
		//add tracks to the playlist
		numAdded=0;
		if(cd.Order && cdUseOrder){
			TCHAR *s, *e;
			for(s=cd.Order; *s;){
				int n=_tcstol(s, &e, 10);
				if(e>s){
					s=e;
					addTrack(n+1);
				}
				else{
					s++;
				}
			}
		}
		if(!numAdded){
			//add all tracks
			for(int i=1; i<=trackN; i++) addTrack(i);
		}
		//play
		noSort--;
		gotoFileN();
		noSort++;
	}
}

//-------------------------------------------------------------------------
void editLabel(TcdInfo *obj, HWND trackList, int item, int col);


LRESULT CALLBACK subitemProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	if(mesg==WM_KEYDOWN){
		if(wP==VK_TAB){
			wP=VK_RETURN;
		}
	}
	if(mesg==WM_KILLFOCUS){
		if(GetKeyState(VK_TAB)<0){
			PostMessage(GetParent(GetParent(hWnd)), WM_COMMAND, IDOK, 0);
		}
		else{
			DestroyWindow(hWnd);
		}
		return 0;
	}
	if(mesg==WM_DESTROY) wndEdit=0;
	return CallWindowProc((WNDPROC)editWndProc, hWnd, mesg, wP, lP);
}


//CD info dialog procedure
BOOL CALLBACK cdProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	int i, n, k, cmd;
	Ttrack *t;
	TCHAR *s;
	HWND combo, trackList;
	HDC dc;
	LV_DISPINFO *pnmv;
	LV_COLUMN lvc;
	LVHITTESTINFO lhti;
	static const int Mbuf=1024;
	TCHAR *buf= (TCHAR*)_alloca(Mbuf*2);
	static char *colNames[]={"#", "Title", "Offset", "Len.", "Comment"};
	static int colWidth[]={25, 210, 55, 42, 250};
	static TcdStrValue strOpts[]={
		{&TcdInfo::Album, IDC_CDALBUM},
		{&TcdInfo::Artist, IDC_CDARTIST},
		{&TcdInfo::Extd, IDC_CDEXTD},
		{&TcdInfo::Year, IDC_CDYEAR},
		{&TcdInfo::Genre, IDC_CDGENRE},
		{&TcdInfo::Order, IDC_CDORDER},
	};

#ifndef _WIN32
	TcdInfo *obj= (TcdInfo*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
#else
	TcdInfo *obj= (TcdInfo*)GetWindowLong(hWnd, GWL_USERDATA);
#endif
	trackList = GetDlgItem(hWnd, IDC_CDLIST);

	switch(mesg) {
		case WM_INITDIALOG:
			dlgWin=hWnd;
			obj= (TcdInfo*)lP;
#ifndef _WIN32
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)obj);
#else
			SetWindowLong(hWnd, GWL_USERDATA, (LONG)obj);
#endif
			setDlgTexts(hWnd, 16);
			lvc.mask = LVCF_WIDTH | LVCF_SUBITEM | LVCF_TEXT;
			dc=GetDC(hWnd);
			n= GetDeviceCaps(dc, LOGPIXELSX);
			ReleaseDC(hWnd, dc);
			for(i=0; i<sizeA(colWidth); i++){
				lvc.iSubItem = i;
				lvc.cx = colWidth[i]*n/96;
				convertA2T(colNames[i], cn);
				lvc.pszText = lng(620+i, cn);
				ListView_InsertColumn(trackList, i, &lvc);
			}
			ListView_SetExtendedListViewStyle(trackList, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
			combo= GetDlgItem(hWnd, IDC_CDGENRE);
			for(i=0; i<sizeA(genres); i++){
				SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)genres[i]);
			}
			combo= GetDlgItem(hWnd, IDC_CDCATEGORY);
			for(i=0; i<sizeA(freedbCategories); i++){
				SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)freedbCategories[i]);
			}
			PostMessage(hWnd, WM_COMMAND, 993, 0);
			return TRUE;
		case WM_NOTIFY:
			switch(((LPNMHDR)lP)->code){
				case LVN_GETDISPINFO:
					pnmv= (LV_DISPINFO*)lP;
					if(pnmv->item.mask & LVIF_TEXT){
						TCHAR *s = pnmv->item.pszText;
						*s=0;
						k= pnmv->item.iItem+1;
						if(k>obj->trackN2) break;
						t = &obj->Tracks[k];
						n = pnmv->item.cchTextMax;
						switch(pnmv->item.iSubItem){
							case COL_TRACK:
								_stprintf(s, _T("%d"), k);
								break;
							case COL_TITLE:
								_tcsncpy(s, anul(t->title), n);
								break;
							case COL_OFFSET:
								_stprintf(s, _T("%d"), t->start);
								break;
							case COL_LEN:
								printTime(s, UNITS*LONGLONG(t->length)/75);
								break;
							case COL_EXTT:
								_tcsncpy(s, anul(t->extt), n);
								break;
						}
					}
					break;
				case LVN_KEYDOWN:
					if(((NMLVKEYDOWN*)lP)->wVKey==VK_F2 || ((NMLVKEYDOWN*)lP)->wVKey==VK_F4){
						editLabel(obj, trackList, ListView_GetNextItem(trackList, -1, LVNI_FOCUSED),
							((NMLVKEYDOWN*)lP)->wVKey==VK_F4 ? COL_EXTT : COL_TITLE);
					}
					break;
				case NM_CLICK:
					GetCursorPos(&lhti.pt);
					ScreenToClient(trackList, &lhti.pt);
					if(ListView_SubItemHitTest(trackList, &lhti)>=0){
						editLabel(obj, trackList, lhti.iItem, lhti.iSubItem);
					}
					break;
			}
			break;
		case WM_COMMAND:
			cmd=LOWORD(wP);
			if(cmd==IDCLOSE || cmd==IDC_CDSAVELOCAL || cmd==IDC_SUBMIT ||
				cmd==IDC_CDDELLOCAL || cmd==IDOK && !wndEdit){
				//read dialog controls
				for(i=0; i<sizeA(strOpts); i++){
					GetDlgItemText(hWnd, strOpts[i].id, buf, Mbuf);
					TCHAR *&v= obj->*strOpts[i].value;
					if(v ? _tcscmp(v, buf) : buf[0]) obj->modif=true;
					if(buf[0]) cpStr(v, buf);
					else delStr(v);
				}
				GetDlgItemText(hWnd, IDC_CDCATEGORY, buf, Mbuf);
				cpStr(obj->Category, buf);
			}
			switch(cmd){
				case IDOK:
					if(wndEdit){
						if(GetWindowText(wndEdit, buf, Mbuf)){
							t= &obj->Tracks[editItem+1];
							cpStr(editCol==4 ? t->extt : t->title, buf);
							obj->modif=true;
						}
						DestroyWindow(wndEdit);
						editItem++;
						ListView_EnsureVisible(trackList, editItem, FALSE);
						editLabel(obj, trackList, editItem, editCol);
						if(wndEdit){
							ListView_SetItemState(trackList, editItem, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
						}
						return TRUE;
					}
					//!
				case IDCANCEL:
				case IDCLOSE:
					if(wndEdit){
						SetFocus(trackList);
					}
					else{
						if(obj->modif){
							int btn= msglng1(MB_YESNOCANCEL, 861, "Do you want to save CD info to the local database ?");
							if(btn==IDCANCEL) break;
							if(btn==IDYES) obj->saveToLocalCDDB();
							obj->modif=false;
						}
						//update items
						for(i=0; i<playlist.len; i++){
							Titem *item= &playlist[i];
							if(toupper(item->file[0])==cdLetter && isCDA(item)){
								updateTrack(item, getCDtrack(item));
							}
						}
						invalidateList();
						EndDialog(hWnd, cmd);
					}
					return TRUE;
				case IDC_DEFAULT_ORDER:
					if(trackN){
						s=buf;
						for(i=0; i<playlist.len; i++){
							Titem *t= &playlist[i];
							if(isCDA(t) && toupper(t->file[0])==cdLetter){
								s+=_stprintf(s, _T("%d,"), getCDtrack(t)-1);
							}
						}
						if(s==buf){
							for(i=0; i<trackN; i++){
								s+=_stprintf(s, _T("%d,"), i);
							}
						}
						s[-1]=0;
						SetDlgItemText(hWnd, IDC_CDORDER, buf);
					}
					break;
				case IDC_CDREADLOCAL:
					if(!cddbFolder[0]){
						errNoDb();
					}
					else{
						obj->readLocalCDDB();
						if(obj->results.len==0){
							obj->readCdplayerIni();
							if(obj->results.len>0){
								msglng(798, "CD info has been found in cdplayer.ini");
								goto lrepaint;
							}
							msglng(796, "CD was not found in the local database");
						}
						else{
							goto lrepaint;
						}
					}
					break;
				case IDC_CDDELLOCAL:
					if(!obj->deleteFromLocalCDDB()){
						msglng(797, "CD not found in directory \"%s\"", obj->Category);
					}
					break;
				case IDC_CDSAVELOCAL:
					if(!cddbFolder[0]){
						errNoDb();
					}
					else{
						obj->saveToLocalCDDB();
					}
					break;
				case IDC_QUERY_FREEDB:
					if(obj->getFromFreedb(false)) goto lrepaint;
					break;
				case IDC_CDTEXT:
					if(cdDevice && !cdda) SendMessage(hWin, WM_COMMAND, ID_FILE_STOP, 0);
					if(obj->getCDText()>1) goto lrepaint;
					msglng(793, "There is no CD-TEXT on this CD");
					break;
				case IDC_CDDELETE:
					obj->reset();
					goto lrepaint;
				case IDC_SUBMIT:
					obj->submit();
					goto lrepaint;

				case 993: //set dialog item texts
				lrepaint :
							ListView_SetItemCountEx(trackList, obj->trackN2, 0);
								 combo= GetDlgItem(hWnd, IDC_CDCATEGORY);
								 SendMessage(combo, CB_SETCURSEL, SendDlgItemMessage(hWnd, IDC_CDCATEGORY, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)anul(obj->Category)), 0);
								 for(i=0; i<sizeA(strOpts); i++){
									 SetDlgItemText(hWnd, strOpts[i].id, anul(obj->*strOpts[i].value));
								 }
								 SetDlgItemText(hWnd, IDC_DISCID, obj->discId);
								 SetDlgItemText(hWnd, IDC_MEDIAID, obj->mediaIdHex);
								 _itot(obj->revision, buf, 10);
								 SetDlgItemText(hWnd, IDC_REVISION, buf);
								 _stprintf(buf, _T("%d:%d"), MCI_MSF_MINUTE(cdLen), MCI_MSF_SECOND(cdLen));
								 SetDlgItemText(hWnd, IDC_TOTAL, buf);
								 break;
			}
			break;
	}
	return FALSE;
}

void editLabel(TcdInfo *obj, HWND trackList, int item, int col)
{
	RECT rc;
	if((col==COL_TITLE || col==COL_EXTT) && item < obj->trackN2 && !wndEdit &&
		ListView_GetSubItemRect(trackList, item, col, LVIR_BOUNDS, &rc)){
		editItem= item;
		editCol= col;
		Ttrack *t= &obj->Tracks[item+1];
		wndEdit= CreateWindow(_T("EDIT"), anul(col==COL_EXTT ? t->extt : t->title),
			WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
			rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, trackList, (HMENU)199, inst, 0);
		SendMessage(wndEdit, WM_SETFONT, SendMessage(trackList, WM_GETFONT, 0, 0), 0);
		SendMessage(wndEdit, EM_SETSEL, 0, -1);
		editWndProc = (WNDPROC)SetWindowLong(wndEdit,
			GWL_WNDPROC, (LPARAM)subitemProc);
		SetFocus(wndEdit);
	}
}

//-------------------------------------------------------------------------

void cdEditInfo()
{
	if(!cdDevice){
		if(!cdFindDrive()) return;
		closeCD();
	}
	DialogBoxParam(inst, MAKEINTRESOURCE(IDD_CD), hWin, (DLGPROC)cdProc, (LPARAM)&cd);
	dlgWin=0;
}

//------------------------------------------------------------------

void cdInfo(Darray<TCHAR> &s)
{
	dtprintf(s, lng(681, _T("Total length: %d:%02d\n\n")),
		MCI_MSF_MINUTE(cdLen), MCI_MSF_SECOND(cdLen));
}

