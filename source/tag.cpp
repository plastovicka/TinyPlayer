/*
	(C) 2005  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "tinyplay.h"
#include <initguid.h>

#define Dfoot 512
#define MAXTAGLEN 1000000

//displayed names (used only when English is selected)
TCHAR *tagNames[MTAGS]={
	_T("Title"), _T("Artist"), _T("Album"), _T("Track"),
	_T("Year"), _T("Comment"), _T("Genre"), _T("Copyright"),
	_T("Encoded by"), _T("URL"), _T("Description"),
	_T("Composer"), _T("Orig. Artist"), _T("Tag")
};

//APE and Ogg Vorbis field names
struct Tsi {
	char *s;
	int id;
} tagNamesA[]={
	{"TITLE", T_TITLE}, {"ARTIST", T_ARTIST}, {"ALBUM", T_ALBUM},
	{"TRACKNUMBER", T_TRACK}, {"DATE", T_YEAR}, {"COMMENT", T_COMMENT},
	{"GENRE", T_GENRE}, {"COPYRIGHT", T_COPYRIGHT}, {"CONTACT", T_URL},
	{"DESCRIPTION", T_DESCRIPTION}, {"AUTHOR", T_ARTIST},
	{"Track", T_TRACK}, {"Year", T_YEAR}, {"Artist URL", T_URL},
	{"Composer", T_COMPOSER},
};

//ID3v2.4 and ID3v2.2 frame id
struct Tii {
	int tag;
	int id;
} id3v2TagNames[]={
	{'TIT2', T_TITLE}, {'TPE1', T_ARTIST}, {'TALB', T_ALBUM}, {'TRCK', T_TRACK},
	{'TDRC', T_YEAR}, {'TYER', T_YEAR}, {'COMM', T_COMMENT}, {'TCON', T_GENRE}, {'TCOP', T_COPYRIGHT},
	{'TENC', T_ENCODER}, {'WXXX', T_URL}, {'TIT1', T_DESCRIPTION},
	{'TCOM', T_COMPOSER}, {'TOPE', T_ORIG_ARTIST},
	{'TT2', T_TITLE}, {'TP1', T_ARTIST}, {'TAL', T_ALBUM}, {'TRK', T_TRACK},
	{'TYE', T_YEAR}, {'COM', T_COMMENT}, {'TCO', T_GENRE}, {'TCR', T_COPYRIGHT},
	{'TEN', T_ENCODER}, {'WXX', T_URL}, {'TT1', T_DESCRIPTION},
	{'TCM', T_COMPOSER}, {'TOA', T_ORIG_ARTIST},
};

//Windows Media Tags
struct Twi {
	WCHAR *s;
	int id;
} wmaTagNames[]={
	{L"Title", T_TITLE},
	{L"Author", T_ARTIST},
	{L"Description", T_DESCRIPTION},
	{L"Copyright", T_COPYRIGHT},
	{L"WM/AlbumTitle", T_ALBUM},
	{L"WM/Year", T_YEAR},
	{L"WM/Genre", T_GENRE},
	{L"WM/TrackNumber", T_TRACK},
	{L"WM/Track", T_TRACK},
	{L"WM/Composer", T_COMPOSER},
	{L"WM/EncodedBy", T_ENCODER},
	{L"WM/AuthorURL", T_URL},
	{L"WM/PromotionURL", T_URL},
	{L"WM/UserWebURL", T_URL},
	{L"WM/OriginalArtist", T_ORIG_ARTIST},
};

//ID3v1 genre
char *genres[126]={
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
	"Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap",
	"Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska", "Death Metal",
	"Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal",
	"Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental", "Acid", "House",
	"Game", "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul", "Punk",
	"Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic",
	"Gothic", "Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk",
	"Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40",
	"Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave",
	"Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk",
	"Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
	"Folk", "Folk/Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin",
	"Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock", "Progressive Rock",
	"Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band", "Chorus",
	"Easy Listening", "Acoustic", "Humour", "Speech", "Chanson", "Opera", "Chamber Music",
	"Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire",
	"Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad",
	"Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "Acapella",
	"Euro-House", "Dance Hall"
};
//------------------------------------------------------------------

void cpnStr1(TCHAR *&dest, char *src, unsigned n)
{
	dest = new TCHAR[n+1];
#ifdef UNICODE
	if(iso88591){
		for(unsigned i=0; i<n; i++){
			dest[i]=(unsigned char)src[i];
		}
	}
	else{
		MultiByteToWideChar(CP_ACP, 0, src, n, dest, n);
	}
#else
	if(iso88591){
		WCHAR *t= new WCHAR[n];
		for(unsigned i=0; i<n; i++){
			t[i]=(unsigned char)src[i];
		}
		WideCharToMultiByte(CP_ACP, 0, t, n, dest, n, 0,0);
		delete[] t;
	}else{
		for(unsigned i=0; i<n; i++){
			dest[i]=src[i];
		}
	}
#endif
	dest[n]=0;
}

void cpnStrA(TCHAR *&dest, char *src, unsigned n)
{
	if(!n || !src[0]) return;
	if(!src[n-1]) n--;
	TCHAR *old=dest;
	cpnStr1(dest, src, n);
	delete[] old;
}

void cpStrID3v1(TCHAR *&dest, char *src, unsigned n)
{
	if(dest || !src[0]) return;
	//trim trailing zeros and spaces
	while(src[--n]==0);
	while(src[n]==' '){
		if(n==0) return;
		n--;
	}
	n++;
	//copy string
	cpnStr1(dest, src, n);
}

void cpStrUTF8(TCHAR *&dest, char *src, char *srcEnd)
{
	char c=*srcEnd;
	*srcEnd=0;
	convertA2W(CP_UTF8, src, w);
	convertW2T(w, a);
	cpStr(dest, a);
	*srcEnd=c;
}

void cpStrW(TCHAR *&dest, WCHAR *src)
{
	convertW2T(src, t);
	if(t[0]) cpStr(dest, t);
}

void BE2LE(char *s, int n)
{
	for(int i=0; i<n; i+=2){
		char w=s[i];
		s[i]=s[i+1];
		s[i+1]=w;
	}
}

//------------------------------------------------------------------
bool cmpTAG(char *buf)
{
	return buf[0]=='T' && buf[1]=='A' && buf[2]=='G';
}


int tag_ID3v1(TCHAR **t, char *buf)
{
	if(cmpTAG(buf)){
		cpStr(t[T_TAG], _T("ID3v1"));
		cpStrID3v1(t[T_TITLE], buf+3, 30);
		cpStrID3v1(t[T_ARTIST], buf+33, 30);
		cpStrID3v1(t[T_ALBUM], buf+63, 30);
		cpStrID3v1(t[T_YEAR], buf+93, 4);
		cpStrID3v1(t[T_COMMENT], buf+97, 28);
		if(buf[125]==0 && buf[126]!=0){
			char s[4];
			sprintf(s, "%d", buf[126]);
			cpStrID3v1(t[T_TRACK], s, 3);
		}
		if(!t[T_GENRE]){
			unsigned g= (BYTE)buf[127];
			if(g<sizeA(genres)){
				convertA2T(genres[g], w);
				cpStr(t[T_GENRE], w);
			}
		}
		return 1;
	}
	return 0;
}

//------------------------------------------------------------------
int rdSynchInt(char *s)
{
	return (((((s[0]<<7)|s[1])<<7)|s[2])<<7)|s[3];
}

int rdInt(char *s0)
{
	BYTE *s= (BYTE*)s0;
	return (((((s[0]<<8)|s[1])<<8)|s[2])<<8)|s[3];
}

int tag_ID3v2(TCHAR **t, HANDLE f, char *s, DWORD r)
{
	BYTE version, flag;
	int i, tag, result=0;
	unsigned len, o;
	char *e, *buf, tag0;

	if(s[0]=='I' && s[1]=='D' && s[2]=='3' && r>16){
		buf=0;
		e=s+r;
		//header
		version=s[3];
		TCHAR v[20];
		_stprintf(v, _T("ID3v2.%d"), version);
		cpStr(t[T_TAG], v);
		s+=10;
		if(s[-5]&0x40){
			//skip extended header
			if(version>3) s+=rdSynchInt(s);
			else if(version==3) s+=rdInt(s)+4;
		}
		while(s+10<=e && ((s[0]>='0' && s[0]<='9') || (s[0]>='A' && s[0]<='Z'))){
			//frame header
			tag0=s[0];
			if(version<3){
				tag=(((tag0<<8)|s[1])<<8)|s[2];
				len=(((s[3]<<8)|s[4])<<8)|s[5];
				flag=0;
				s+=6;
			}
			else{
				tag=rdInt(s);
				s+=4;
				len= (version==3) ? rdInt(s) : rdSynchInt(s);
				flag=s[5];
				s+=6;
			}
			if(len>MAXTAGLEN) break;
			//read frame from the file
			o= unsigned(e-s);
			if(o<len+10){
				delete[] buf;
				buf=new char[len+10];
				memcpy(buf, s, o);
				ReadFile(f, buf+o, len+10-o, &r, 0);
				if(r<=len-o) break;
				s=buf;
				e=buf+o+r;
			}
			if((flag&0xfd)==0){ //not compressed, not encrypted
				//parse frame
				for(i=0; i<sizeA(id3v2TagNames); i++){
					if(tag==id3v2TagNames[i].tag){
						TCHAR *&dst= t[id3v2TagNames[i].id];
						len--;
						char enc= *s++;
						if(tag=='COMM'){
							//skip language
							s+=3; len-=3;
						}
						if(tag=='COMM' || (tag&0xffff)==(('X'<<8)|'X')){
							//skip description
							if(enc==1 || enc==2){
								do{
									s+=2;
									len-=2;
								} while(((WCHAR*)s)[-1]!=0 && len>2);
							}
							else{
								do{
									s++;
									len--;
								} while(s[-1]!=0 && len>1);
							}
						}
						else if(tag0=='W'){
							//URL does not have an encoding byte
							s--;
							len++;
						}
						if(enc==0 || tag0=='W'){ //ISO-8859-1
							cpnStrA(dst, s, len);
						}
						else if(enc==3){ //UTF-8
							char c=s[len];
							s[len]=0;
							convertA2W(CP_UTF8, s, w);
							convertW2T(w, a);
							cpStr(dst, a);
							s[len]=c;
						}
						else{ //UTF-16
							if(BYTE(enc)>3) break; //unknown text encoding
							WCHAR *sc= (WCHAR*)(s+len);
							WCHAR c=*sc;
							*sc=0;
							if(enc==2 || BYTE(s[0])==0xFE){
								//big endian UTF-16
								BE2LE(s, len);
							}
							if(enc==1){
								//skip BOM
								s+=2;
								len-=2;
							}
							convertW2T((WCHAR*)s, a);
							cpStr(dst, a);
							*sc=c;
						}
						result++;
						break;
					}
				}
			}
			s+=len;
		}
		delete[] buf;
	}
	return result;
}

//------------------------------------------------------------------
struct TapeHead {
	char ape[8];
	DWORD version;
	DWORD size;
	DWORD num;
	DWORD flag;
	char reserved[8];
};


int tag_APE(TCHAR **t, HANDLE f, char *s)
{
	int i, result=0;
	char *data, *e, *s0, *se;
	unsigned len;
	DWORD r;
	int o= cmpTAG(s-128) ? 128 : 0;
	TapeHead &buf= *(TapeHead*)(s-o-32);

	if(!strncmp(buf.ape, "APETAGEX", 8) && buf.size<MAXTAGLEN){
		TCHAR v[24];
		_stprintf(v, _T("APEv%g"), buf.version/1000.0);
		cpStr(t[T_TAG], v);
		data=0;
		len=buf.size+o;
		buf.size-=32;
		if(len<=Dfoot){
			s-=len;
		}
		else{
			//read all fields from the file
			s=0;
			if(SetFilePointer(f, -(LONG)len, 0, FILE_END)!=0xFFFFFFFF){
				data=new char[buf.size+40];
				ReadFile(f, data, buf.size, &r, 0);
				if(r==buf.size){
					s=data;
				}
			}
		}
		if(s){
			e=s+buf.size;
			e[27]=0;
			result++;
			while((buf.num--)>0){
				len= *(DWORD*)s;
				s+=8;
				s0=s;
				while(*s) s++;
				s++;
				se=s+len;
				if(se>e || len>MAXTAGLEN) break;
				for(i=0; i<sizeA(tagNamesA); i++){
					if(!_stricmp(s0, tagNamesA[i].s)){
						TCHAR *&dst=t[tagNamesA[i].id];
						if(buf.version==1000){
							cpnStrA(dst, s, len);
						}
						else{
							cpStrUTF8(dst, s, se);
						}
					}
				}
				s=se;
			}
		}
		delete[] data;
	}
	return result;
}

//------------------------------------------------------------------
bool cmpOgg(char *s)
{
	return s[0]=='O' && s[1]=='g' && s[2]=='g' && s[3]=='S';
}


int tag_Ogg(TCHAR **t, HANDLE f, char *s, DWORD r)
{
	int i;
	char *s0, *se, *buf, *e;
	unsigned n, len, totalLen, offset;

	if(cmpOgg(s) && r>100){
		e=s+r;
		s+=28;
		if(!strncmp(s, "\x01video\0\0\0", 9)){
			//video header
			cpnStrA(t[T_ENCODER], s+9, 4);
			s+=85;
		}
		s+=30; //skip vorbis audio header
		if(cmpOgg(s)){
			s+=26;
			len= *(BYTE*)s;
			offset= 85+len;
			s++;
			//calculate total tags length
			totalLen=*(BYTE*)s;
			while(*(BYTE*)s==255 && len>1){
				s++;
				len--;
				totalLen+=*(BYTE*)s;
			}
			//TODO: tags longer than one page
			s+=len;
			if(!strncmp(s, "\x03vorbis", 7)){
				//read a page from the disk
				buf=0;
				if(r<=offset+totalLen && totalLen<0x10000){
					s= buf= new char[totalLen+4];
					SetFilePointer(f, offset, 0, FILE_BEGIN);
					ReadFile(f, buf, totalLen, &r, 0);
					e=buf+r;
				}
				s+=7;
				//encoder version
				len=*(DWORD*)s;
				if(len<1000){
					s+=4;
					cpnStrA(t[T_TAG], s, len);
					s+=len;
					//number of tags
					n=*(DWORD*)s;
					s+=4;
					while(n-- > 0){
						//tag length
						len=*(DWORD*)s;
						s+=4;
						se=s+len;
						if(se>e || len>MAXTAGLEN) break;
						//tag name
						s0=s;
						while(*s!='='){
							s++;
							if(s>=se) goto l1;
						}
						*s++=0;
						//UTF-8 string
						for(i=0; i<sizeA(tagNamesA); i++){
							if(!_stricmp(s0, tagNamesA[i].s)){
								cpStrUTF8(t[tagNamesA[i].id], s, se);
								break;
							}
						}
						s=se;
					l1:;
					}
				}
				delete[] buf;
				return 1;
			}
		}
	}
	return 0;
}

//------------------------------------------------------------------
enum WMT_ATTR_DATATYPE {
	WMT_TYPE_DWORD=0, WMT_TYPE_STRING=1, WMT_TYPE_BINARY=2,
	WMT_TYPE_BOOL=3, WMT_TYPE_QWORD=4, WMT_TYPE_WORD=5, WMT_TYPE_GUID=6,
};

DEFINE_GUID(G, 0x15cc68e3, 0x27cc, 0x4ecd, 0xb2, 0x22, 0x3f, 0x5d, 0x02, 0xd8, 0x0b, 0xd5);

struct IWMHeaderInfo3 : public IUnknown{
	virtual HRESULT STDMETHODCALLTYPE GetAttributeCount(WORD wStreamNum, WORD* pcAttributes);
	virtual HRESULT STDMETHODCALLTYPE GetAttributeByIndex(WORD wIndex, WORD* pwStreamNum,
		WCHAR* pwszName, WORD* pcchNameLen, WMT_ATTR_DATATYPE* pType,
		BYTE* pValue, WORD* pcbLength);
	virtual HRESULT STDMETHODCALLTYPE GetAttributeByName(WORD* pwStreamNum, LPCWSTR pszName,
		WMT_ATTR_DATATYPE* pType, BYTE* pValue, WORD* pcbLength);
};

struct IWMMetadataEditor2 : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE Open(const WCHAR* pwszFilename);
	virtual HRESULT STDMETHODCALLTYPE Flush();
	virtual HRESULT STDMETHODCALLTYPE Close();
};


int tag_WMA(TCHAR **t, TCHAR *fn)
{
	int result=0;
	HMODULE wmLib= LoadLibraryA("WMVCore.dll");
	if(wmLib){
		FARPROC wmCreateEditor= GetProcAddress(wmLib, "WMCreateEditor");
		if(wmCreateEditor){
			IWMMetadataEditor2 *wmEd=0;
			try{
				((void(_stdcall*)(IWMMetadataEditor2**))wmCreateEditor)(&wmEd);
				convertT2W(fn, fnW);
				wmEd->Open(fnW);
				try{
					IWMHeaderInfo3 *wmInfo=0;
					wmEd->QueryInterface(G, (void**)&wmInfo);
					WORD n, j, stream, nameLen, valueLen;
					n=0;
					wmInfo->GetAttributeCount(0xFFFF, &n);
					for(j=0; j<n; j++){
						WCHAR name[128];
						BYTE value[512];
						WMT_ATTR_DATATYPE type;
						stream=0xFFFF;
						nameLen=sizeA(name);
						valueLen=sizeA(value);
						if(SUCCEEDED(wmInfo->GetAttributeByIndex(j, &stream, name, &nameLen, &type, value, &valueLen))){
							for(int i=0; i<sizeA(wmaTagNames); i++){
								if(!_wcsicmp(name, wmaTagNames[i].s)){
									TCHAR *&dst=t[wmaTagNames[i].id];
									switch(type){
										case WMT_TYPE_STRING:
											cpStrW(dst, (WCHAR*)value);
											result++;
											break;
									}
								}
							}
						}
					}
					wmInfo->Release();
				} catch(...){}
				wmEd->Close();
			} catch(...){}
			SAFE_RELEASE(wmEd);
		}
		FreeLibrary(wmLib);
	}
	if(result) cpStr(t[T_TAG], _T("WMA"));
	return result;
}

//------------------------------------------------------------------
//read tags from item->file and put them to array t
int getTags(Titem *item, TCHAR **t)
{
	int result;
	HANDLE f;
	DWORD r;
	static const int Mhead=512;
	char *head= (char*)_alloca(Mhead);

	f=CreateFile(item->file, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if(f==INVALID_HANDLE_VALUE) return 0;
	if(item->size==0){
		*((DWORD*)&item->size) = GetFileSize(f, ((DWORD*)&item->size)+1);
		item->calcBitrate();
	}
	//tags at the beginning of file
	ReadFile(f, head, Mhead, &r, 0);
	result= (r>0) && (tag_Ogg(t, f, head, r) || tag_ID3v2(t, f, head, r));
	if(!result){
		//tags at the end of file
		if(SetFilePointer(f, -Dfoot, 0, FILE_END)!=0xFFFFFFFF){
			ReadFile(f, head, Dfoot, &r, 0);
			result= (r==Dfoot) && (tag_APE(t, f, head+Dfoot) || tag_ID3v1(t, head+Dfoot-128));
		}
	}
	if(!result){
		result=tag_WMA(t, item->file);
	}
	CloseHandle(f);
	return result;
}
//------------------------------------------------------------------
bool mp3Duration(Titem *item)
{
	HANDLE f, f1;
	LONGLONG fpos, fend;
	double duration;
	unsigned long head;
	char buf[12];
	int j=0;
	DWORD r, framesize;
	int lsf, sfreq, bitrate, lastBitrate;
	bool sizeDiff, vbr;
	static const int bitrateTab[2][16]={
		{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320},
		{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
	};
	static const long freqTab[]={
		44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000};

	if(_tcsicmp(item->ext, _T("mp3")) || item->size==0 || item->file[1]!=':') return false;
	//open file
	f1 = CreateFile(item->file, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING, 0);
	f= CreateFile(item->file, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN, 0);
	if(f==INVALID_HANDLE_VALUE){
		CloseHandle(f1);
		return false;
	}
	//skip ID3v2 header
	fpos=0;
	ReadFile(f, buf, 10, &r, 0);
	if(buf[0]=='I' && buf[1]=='D' && buf[2]=='3' && r==10){
		fpos= (buf[6]<<21)+(buf[7]<<14)+(buf[8]<<7)+buf[9]+10;
	}
	duration=0;
	sizeDiff=false;
	vbr=false;
	lastBitrate=0;
	//parse frames
	for(;;){
		if(SetFilePointer(f, (LONG)fpos, ((LONG*)&fpos)+1, FILE_BEGIN)==0xFFFFFFFF
			&& GetLastError()!=NO_ERROR) break;
		ReadFile(f, &head, 4, &r, 0);
		if(r!=4) break;

		if((head & 0xe0ff)==0xe0ff && //synch
			((head>>9)&3)==1){ //Layer 3
			sfreq= ((head>>18)&0x3);
			lsf= 1;
			if(head & (1<<12)){
				if(head & (1<<11)) lsf=0;
				else sfreq += 3;
			}
			else{
				sfreq += 6;
			}
			bitrate= ((head>>20)&0xf);
			if(bitrate<15 && bitrate>0 && sfreq<9){
				bitrate= bitrateTab[lsf][bitrate];
				framesize = bitrate*144000/(freqTab[sfreq]<<lsf)+((head>>17)&0x1);
				fpos += framesize;
				duration += double(framesize) / (bitrate*125);
				if(bitrate!=lastBitrate){
					if(!lastBitrate){
						lastBitrate=bitrate;
					}
					else{
						vbr=true;
					}
				}
				else if(fpos>100000 && !vbr){
					//CBR
					fend=item->size-128;
					if(SetFilePointer(f, (LONG)fend, ((LONG*)&fend)+1, FILE_BEGIN)!=0xFFFFFFFF
						|| GetLastError()==NO_ERROR){
						ReadFile(f, &head, 4, &r, 0);
						if(r==4 && (head&0xffffff)=='GAT') fpos+=128; //ID3v1 tag
					}
					duration += double(item->size - fpos) / (bitrate*125);
					break;
				}
				//if(sizeDiff) msg("%s\n%d",item->name,j);
				sizeDiff = false;
				continue;
			}
		}
		if((head&0xffffff)=='GAT' && fpos>item->size-300) break; //ID3v1 tag
		if(!sizeDiff){
			sizeDiff=true;
			j=0;
			if(fpos>0){ fpos--; j--; }
		}
		else if(j<=2000){
			fpos++;
			j++;
		}
		else{ //error
			duration=0;
			break;
		}
	}
	CloseHandle(f1);
	CloseHandle(f);
	if(!duration) return false;
	item->length = (REFERENCE_TIME)(duration * UNITS);
	return true;
}
//------------------------------------------------------------------
