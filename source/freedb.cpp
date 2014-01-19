/*
	(C) 2005  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "tinyplay.h"
#include "cd.h"

#define APP_NAME "TinyPlayer"
//------------------------------------------------------------------

int port=80;

int useProxy=0;
TCHAR proxy[256];
TCHAR proxyUser[128];
TCHAR proxyPassword[128];
int proxyPort=80;
int inetTimeout=20;
char *sites;
TCHAR username[64]=_T("user");
char hostname[256]="host";
char productVersion[32];

SOCKET sock;

//------------------------------------------------------------------
void skipSpace(char *&s)
{
	while(*s==' ') s++;
}

void skipArg(char *&s)
{
	while(*s){
		if(*s==' '){
			*s=0;
			s++;
			skipSpace(s);
			break;
		}
		s++;
	}
}

void skipEol(char *&s)
{
	while(*s){
		if(*s=='\r' && s[1]=='\n'){
			*s=0;
			s+=2;
			break;
		}
		if(*s=='\n'){
			*s=0;
			s++;
			break;
		}
		s++;
	}
}

void cpStrUTF8(TCHAR *&dest, char *src)
{
	convertA2W(CP_UTF8, src, w);
	convertW2T(w, a);
	cpStr(dest, a);
}

void cpnStr(TCHAR *dest, char *src, int n)
{
	while(--n){
		if((*dest++=*src++)==0) break;
	}
}

void toA(TCHAR *s)
{
	for(; *s; s++){
		if(!isalpha(*s) && !isdigit(*s)) *s='_';
	}
}

void strcpyToA(char *d, TCHAR *s)
{
	while((*d++ = char(*s++))!=0);
}


bool isEmpty(TCHAR *s)
{
	return !s || !*s;
}

//------------------------------------------------------------------
int initWSA()
{
	WSAData wd;
	int err= WSAStartup(0x0202, &wd);
	if(err) msg("Cannot initialize WinSock");
	return err;
}

int startConnection()
{
	unsigned long a;
	sockaddr_in sa;
	hostent *he;

	if(!proxy[0]) useProxy=0;
	if(!initWSA()){
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		TCHAR *server= useProxy ? proxy : servername;
		convertT2A(server, servernameA);
		a= inet_addr(servernameA);
		if(a!=INADDR_NONE){
			sa.sin_addr.S_un.S_addr=a;
		}
		else if((he= gethostbyname(servernameA))!=0){
			memcpy(&sa.sin_addr, he->h_addr, he->h_length);
		}
		else{
			msglng(863, "freedb server not found or you are not connected to the internet");
			goto le;
		}
		if((sock= socket(PF_INET, SOCK_STREAM, 0))==INVALID_SOCKET){
			msg("Error: socket");
		}
		else{
			sa.sin_port = htons((u_short)(useProxy ? proxyPort : port));
			if(connect(sock, (sockaddr*)&sa, sizeof(sa))!=0){
				msglng(864, "Cannot connect to freedb server.\nTry it later or choose another server in program options.");
			}
			else{
				return 0;
			}
			closesocket(sock);
		}
	le:
		WSACleanup();
	}
	return 1;
}

int wr(char *buf)
{
	int i, n, len;

	len=(int)strlen(buf);

	for(n=0; n<len;){
		i=send(sock, buf+n, len-n, 0);
		if(i==SOCKET_ERROR) return -1;
		n+=i;
	}
	return 0;
}

int rd(Darray<char> &buf, char *&res)
{
	int i, n, code=0;
	char *body, *marker;
	const int buf_incr=4096;

	aminmax(inetTimeout, 2, 100);
	buf.reset();
	for(n=0;;){
		timeval timeout;
		timeout.tv_sec= inetTimeout;
		timeout.tv_usec= 0;
		fd_set fds;
		FD_ZERO(&fds);
#pragma warning(disable:4127)
		FD_SET(sock, &fds);
#pragma warning(default:4127)
		if(select(1, &fds, NULL, NULL, &timeout)<=0) return -2; //time out
		i=recv(sock, buf+=buf_incr, buf_incr-4, 0);
		if(i<=0){
			//connection closed
			buf-=buf_incr;
			return -1;
		}
		n+=i;
		buf[n]=0;
		//get three digits result code
		body= strstr(buf, "\r\n\r\n");
		if(!body) body= strstr(buf, "\n\n");
		if(body){
			while(*body=='\r' || *body=='\n') body++;
			body[3]=0;
			code=atoi(body);
			body[3]=' ';
			res=body+4;
		}
		//find termination marker
		marker= strstr(buf, "\n.");
		buf-= buf_incr-i;
		if(marker && (marker[2]=='\r' || marker[2]=='\n')){
			marker[1]=0;
			break;
		}
		if(code && (code<210 || code>219)){
			//one line result or error
			*buf++=0;
			break;
		}
	}
	return code;
}
//------------------------------------------------------------------

char *base64Tab="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void encBase64(TCHAR *d, const TCHAR *s)
{
	BYTE c1, c2, c3;
	int nLineLen=0;
	int inLen=(int)_tcslen(s);

	// 3 x 8-bit -> 4 x 6-bit
	for(int i=0; i<inLen/3; i++){
		c1 = (BYTE)*s++;
		c2 = (BYTE)*s++;
		c3 = (BYTE)*s++;

		*d++ = base64Tab[(c1 & 0xFC) >> 2];
		*d++ = base64Tab[((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4)];
		*d++ = base64Tab[((c2 & 0x0F) << 2) | ((c3 & 0xC0) >> 6)];
		*d++ = base64Tab[c3 & 0x3F];
		nLineLen += 4;

		if(nLineLen >= 73){
			//line is too long
			*d++ = '\r';
			*d++ = '\n';
			nLineLen = 0;
		}
	}

	// the remaining one or two characters in the input buffer
	switch(inLen % 3){
		case 1:
			c1 = (BYTE)*s;
			*d++ = base64Tab[(c1 & 0xFC) >> 2];
			*d++ = base64Tab[((c1 & 0x03) << 4)];
			*d++ = '=';
			*d++ = '=';
			break;
		case 2:
			c1 = (BYTE)*s++;
			c2 = (BYTE)*s;
			*d++ = base64Tab[(c1 & 0xFC) >> 2];
			*d++ = base64Tab[((c1 & 0x03) << 4) | ((c2 & 0xF0) >> 4)];
			*d++ = base64Tab[((c2 & 0x0F) << 2)];
			*d++ = '=';
			break;
	}
	*d=0;
}

//------------------------------------------------------------------
void prepareFreedb()
{
	gethostname(hostname, sizeA(hostname));
	DWORD n=sizeA(username);
	GetUserName(username, &n);
	toA(username);
	DWORD ver= getVer();
	sprintf(productVersion, "%d.%d", HIWORD(ver), LOWORD(ver));
}

int command1(Darray<char> &result, char *&res, char *cmd)
{
	int err, code;

	//create a socket and connect
	err=startConnection();
	if(err) return -2;
	SetCursor(LoadCursor(0, IDC_WAIT));
	//send GET or POST request
	wr(cmd);
	//receive response
	code=rd(result, res);
	//disconnect
	closesocket(sock);
	WSACleanup();
	SetCursor(LoadCursor(0, IDC_ARROW));

	if(code<=0){
		if(result.len>0) msglng(859, "freedb server returned error or incorrectly formatted result");
		else msglng(860, "freedb server did not respond to your request");
	}
	if(code>=400) msg("%hs", res-4);
	return code;
}

void proxyHeader(Darray<TCHAR> &buf)
{
	if(useProxy){
		if(proxyUser[0]){
			TCHAR auth[sizeA(proxyUser)+sizeA(proxyPassword)];
			_stprintf(auth, _T("%s:%s"), proxyUser, proxyPassword);
			TCHAR enc[sizeA(auth)*2];
			encBase64(enc, auth);
			dtprintf(buf, _T("Proxy-Authorization: Basic %s\r\n"), enc);
		}
		dtprintf(buf, _T("Host: %s\r\n"), servername);
	}
}

int command(Darray<char> &result, char *&res, TCHAR *cmd)
{
	int code;
	Darray<TCHAR> buf;
	char *cmdA;

	prepareFreedb();
	dtprintf(buf, _T("GET "));
	if(useProxy){
		dtprintf(buf, _T("http://%s"), servername);
	}
	dtprintf(buf, _T("%s?cmd=%s&hello=%s+%hs+%hs+%hs&proto=6 HTTP/1.0\r\n"),
		serverCgi, cmd, username, hostname, APP_NAME, productVersion);
	proxyHeader(buf);
	dtprintf(buf, _T("\r\n"));
	//convert to US-ASCII
	cmdA= new char[buf.len];
	strcpyToA(cmdA, buf);

	code= command1(result, res, cmdA);
	delete[] cmdA;
	return code;
}

//------------------------------------------------------------------
void TcdInfo::submit()
{
	int code, i, n;
	char *res;
	TCHAR *Order0;
	Darray<TCHAR> s;
	Darray<char> cmd, rec;
	TCHAR app[64];
	TCHAR cmdT[256];

	//all required fields must not be empty
	if(!email[0]){
		msglng(794, "You have to specify your email address (menu File / Options)");
		return;
	}
	n=0;
	for(i=1; i<=trackN2; i++){
		if(!isEmpty(Tracks[i].title)) n++;
	}
	if(isEmpty(Artist) && isEmpty(Album) ||
		isEmpty(Category) || Category[0]=='-' || n<trackN2/2){
		msglng(795, "You have to fill in track titles, artist, album and freedb category");
		return;
	}
	//test whether there is discid collision
	if(revision<0){
		_stprintf(cmdT, _T("cddb+read+%s+%s"), Category, discId);
		if(command(rec, res, cmdT)==210){
			msglng(799, "There is another CD in category %s which has id=%s !", Category, discId);
			return;
		}
		rec.reset();
	}
	//create body
	prepareFreedb();
	_stprintf(app, _T("%hs %hs"), APP_NAME, productVersion);
	cpStr(SubmittedVia, app);
	if(inetCategory && !_tcsicmp(inetCategory, Category) &&
		!_tcsicmp(inetDiscId, discId)){
		revision= inetRevision;
	}
	revision++;
	Order0=Order;
	Order=0;
	wrToString(rec);
	Order=Order0;

	//create header
	dtprintf(s, _T("POST "));
	if(useProxy){
		dtprintf(s, _T("http://%s"), servername);
	}
	dtprintf(s, _T("%s HTTP/1.0\r\n"), serverSubmitCgi);
	proxyHeader(s);
	dtprintf(s, _T("Category: %s\r\nDiscid: %s\r\nUser-Email: %s\r\nSubmit-Mode: submit\r\nContent-Length: %d\r\nCharset: utf-8\r\n\r\n"),
		Category, discId, email, rec.len-1);
	//concatenate header and body
	strcpyToA(cmd+=s.len, s);
	cmd--;
	strcpy(cmd+=rec.len, rec);

	code= command1(cmd, res, cmd);
	if(code>=200 && code<=209){
		//save to a file, because the revision number has been changed
		saveToLocalCDDB(rec);
	}
	else{
		revision--;
	}
}


void TcdInfo::addResult(char *&s)
{
	char *arg, *cat, *id;
	TmultipeResultInfo *p;

	skipSpace(s);
	cat=s;
	skipArg(s);
	id=s;
	skipArg(s);
	arg=s;
	skipEol(s);
	if(*arg && *id && *cat){
		p=results++;
		p->category=p->discId=p->description=0;
		cpStrUTF8(p->category, cat);
		cpStrUTF8(p->discId, id);
		cpStrUTF8(p->description, arg);
	}
}

void TcdInfo::cddbQuery()
{
	char *res;
	TCHAR *s;
	int i, code;
	Darray<char> result;
	TCHAR cmd[1024];

	resetResult();
	s=cmd;
	s+=_stprintf(s, _T("cddb+query+%s+%d"), discId, trackN2);
	for(i=1; i<=trackN2; i++){
		s+=_stprintf(s, _T("+%d"), Tracks[i].start);
	}
	s+=_stprintf(s, _T("+%d"), total);
	code= command(result, res, cmd);
	if(code==200){ //exact result
		addResult(res);
	}
	else if(code>=210 && code<=219){ //multiple matches
		skipEol(res);
		while(*res) addResult(res);
	}
	else if(code==202){ //not found
		msglng(792, "This CD is not yet in the internet database");
	}
}

int TcdInfo::getFromFreedb(bool save)
{
	char *res;
	int which, code;
	Darray<char> result;
	TCHAR cmdT[256];

	cddbQuery();
	which=results.len-1;
	if(which>0){
		//more results
		which= chooseFreedbResult(results);
	}
	if(which<0){
		//not found or canceled
		resetResult();
		return 0;
	}
	_stprintf(cmdT, _T("cddb+read+%s+%s"), results[which].category, results[which].discId);
	code= command(result, res, cmdT);
	if(code>=210 && code<=219){
		cpStr(inetCategory, results[which].category);
		cpStr(inetDiscId, results[which].discId);
		skipEol(res);
		rdFromString(res, inetCategory);
		cpStr(Category, inetCategory);
		inetRevision=revision;
		if(save){
			saveToLocalCDDB(res);
		}
		else{
			modif=true;
		}
		return 1;
	}
	else{
		resetResult();
		return 0;
	}
}

void getSites()
{
	int code;
	char *res;
	Darray<char> result;

	code= command(result, res, _T("sites"));
	if(code==210){
		skipEol(res);
		cpStr(sites, res);
	}
}

void parseSites(HWND list, int i)
{
	char *s, *t, *a, *sites1;

	sites1=dupStr(sites);
	for(s=sites1; *s;){
		t=s;
		skipEol(s);
		if(!strstr(t, " cddbp ")){
			if(i<0){
				SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)t);
			}
			else if(i==0){
				a=t;
				skipArg(t);
				cpnStr(servername, a, sizeA(servername));
				skipArg(t);
				a=t;
				skipArg(t);
				port=atoi(a);
				a=t;
				skipArg(t);
				cpnStr(serverCgi, a, sizeA(serverCgi));
				break;
			}
			else{
				i--;
			}
		}
	}
	delete[] sites1;
}

