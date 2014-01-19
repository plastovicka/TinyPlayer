/*
 (C) 1999-2002 Albert L. Faber
 (C) 1999 Jay A. Key (AKrip)
 (C) 2005 Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#include "hdr.h"
#include "tinyplay.h"
#include "cd.h"
#include "aspi.h"
#include "sound.h"

const int cdBegin=150;
const int aspiTimeOut=10000;
int cdPeriodicSpeed=0;
int cdSpeedPeriod=400;

enum READMETHOD {
	READMMC, READMMC2, READMMC3, READMMC4, READ10, READNEC, READSONY,
	READC1, READC2, READC3
};

BYTE cdSpeedTab[]={1, 2, 4, 8, 12, 16, 24, 32, 40, 0};

HANDLE ripThread;
AudioFilter *cdAudioFilter;
DWORD dwNumSectors;
DWORD dwReadBufferSize;
BYTE *pbtReadBuffer;
LONG lSector;
REFERENCE_TIME refTimeDiff;

HMODULE hAspiLib;
GETASPI32SUPPORTINFO GetASPI32SupportInfo;
SENDASPI32COMMAND SendASPI32Command;

BYTE TargetID;
BYTE AdapterID;
BYTE LunID;
HANDLE hDevice;

#define NTSCSI_HA_INQUIRY_SIZE 0x24
SCSI_INQUIRY_RESULT inqData;

//------------------------------------------------------------------

void closeAspi()
{
	FreeLibrary(hAspiLib);
}

int initAspi()
{
	hAspiLib = 0;
	//open library from the SYSTEM folder
	if(!hAspiLib) hAspiLib= LoadLibrary(_T("wnaspi32.dll"));
	//try to use the ASPI installed by Nero
	HKEY key;
	if(!hAspiLib && RegOpenKey(HKEY_LOCAL_MACHINE, _T("Software\\Ahead\\Shared"), &key)==ERROR_SUCCESS){
		TCHAR path[512];
		DWORD size=sizeof(path)-28;
		DWORD type;
		if(RegQueryValueEx(key, _T("NeroAPI"), 0, &type, (BYTE*)&path, &size)==ERROR_SUCCESS){
			_tcscat(path, _T("\\wnaspi32.dll"));
			hAspiLib = LoadLibrary(path);
		}
		RegCloseKey(key);
	}

	if(hAspiLib){
		GetASPI32SupportInfo = (GETASPI32SUPPORTINFO)GetProcAddress(hAspiLib, "GetASPI32SupportInfo");
		SendASPI32Command = (SENDASPI32COMMAND)GetProcAddress(hAspiLib, "SendASPI32Command");
		if(GetASPI32SupportInfo && SendASPI32Command) return 1;
		closeAspi();
	}
	return 0;
}

//------------------------------------------------------------------

BYTE sendASPI(BYTE *cmd, int cmdLen, BYTE *buf, int bufLen)
{
	HANDLE hEvent;
	SRB_EXECSCSICMD mySrb;

	// create manual reset event
	if((hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL){
		return SS_ERR;
	}

	// Set SRB fields
	memset(&mySrb, 0, sizeof(SRB_EXECSCSICMD));
	mySrb.SRB_Cmd=SC_EXEC_SCSI_CMD;
	mySrb.SRB_HaId=AdapterID;
	if(buf) mySrb.SRB_Flags = SRB_EVENT_NOTIFY | SRB_DIR_IN;
	else mySrb.SRB_Flags = SRB_EVENT_NOTIFY | SRB_DIR_OUT;
	mySrb.SRB_Target=TargetID;
	mySrb.SRB_Lun=LunID;
	mySrb.SRB_SenseLen=SENSE_LEN;
	mySrb.SRB_CDBLen=(BYTE)cmdLen;
	mySrb.SRB_BufLen=bufLen;
	mySrb.SRB_BufPointer=buf;
	mySrb.SRB_PostProc= (POSTPROCFUNC)hEvent;
	if(cmd) memcpy(&mySrb.CDBByte, cmd, cmdLen);

	// send ASPI command
	if(SendASPI32Command((LPSRB)&mySrb) == SS_PENDING){
		// wait until finished or time-out
		WaitForSingleObject(hEvent, aspiTimeOut);
	}
	CloseHandle(hEvent);
	if(mySrb.SenseArea[2]==2){
		return SS_ABORTED;
	}
	return mySrb.SRB_Status;
}

//------------------------------------------------------------------

/*
 Get a file handle to the CD device.
 NT4 wants just GENERIC_READ flag
 Win2000 wants both GENERIC_READ and GENERIC_WRITE
 */
HANDLE GetDeviceHandle()
{
	char buf[12];
	sprintf(buf, "\\\\.\\%c:", cdLetter);

	HANDLE fh = CreateFileA(buf, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if(fh == INVALID_HANDLE_VALUE){
		fh = CreateFileA(buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	}
	return fh;
}

// Get the drive inquiry data
int GetDriveInformation()
{
	int result=0;
	ULONG returned;
	SCSI_ADDRESS scsiAddr;
	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER swb;

	hDevice = GetDeviceHandle();

	if(hDevice != INVALID_HANDLE_VALUE){
		memset(&swb, 0, sizeof(swb));
		swb.spt.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
		swb.spt.CdbLength = 6;
		swb.spt.SenseInfoLength = 24;
		swb.spt.DataIn = SCSI_IOCTL_DATA_IN;
		swb.spt.DataTransferLength = sizeof(inqData);
		swb.spt.TimeOutValue = 5;
		swb.spt.DataBuffer = &inqData;
		swb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
		swb.spt.Cdb[0] = SCSI_CMD_INQUIRY;
		swb.spt.Cdb[4] = NTSCSI_HA_INQUIRY_SIZE;

		if(DeviceIoControl(hDevice, IOCTL_SCSI_PASS_THROUGH_DIRECT,
				&swb, sizeof(swb), &swb, sizeof(swb), &returned, NULL)){

			// get the address (path/tgt/lun) of the drive
			scsiAddr.Length = sizeof(SCSI_ADDRESS);

			if(DeviceIoControl(hDevice, IOCTL_SCSI_GET_ADDRESS, NULL, 0, &scsiAddr, sizeof(SCSI_ADDRESS), &returned, NULL)){
				AdapterID = scsiAddr.PortNumber;
				TargetID = scsiAddr.TargetId;
				LunID = scsiAddr.Lun;
				result=1;
			}
			else{
				// support USB/FIREWIRE devices where this call is not supported
				if(GetLastError() == 50){
					result=1;
				}
			}
		}
	}
	return result;
}


// Convert ASPI-style SRB to SCSI Pass Through IOCTL
DWORD NtScsiSendASPI32Command(LPSRB param)
{
	LPSRB_EXECSCSICMD lpsrb= (LPSRB_EXECSCSICMD)param;
	BOOL status;
	ULONG	returned;
	BOOL bBeenHereBefore=FALSE;
	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER swb;

start:

	memset(&swb, 0, sizeof(swb));
	swb.spt.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
	swb.spt.DataIn = SCSI_IOCTL_DATA_IN;
	swb.spt.DataTransferLength = lpsrb->SRB_BufLen;
	swb.spt.TimeOutValue = 15;
	swb.spt.DataBuffer = lpsrb->SRB_BufPointer;
	swb.spt.SenseInfoLength = lpsrb->SRB_SenseLen;
	swb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
	swb.spt.CdbLength = lpsrb->SRB_CDBLen;

	memcpy(swb.spt.Cdb, lpsrb->CDBByte, lpsrb->SRB_CDBLen);

	status = DeviceIoControl(hDevice,
		IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&swb, sizeof(swb), &swb, sizeof(swb),
		&returned, NULL);

	// copy sense data
	memcpy(lpsrb->SenseArea, swb.ucSenseBuf, lpsrb->SRB_SenseLen);

	if(status){
		lpsrb->SRB_Status = SS_COMP;
	}
	else{
		DWORD dwErrCode;
		lpsrb->SRB_Status   = SS_ERR;
		lpsrb->SRB_TargStat = 0x0004;
		lpsrb->SRB_Hdr_Rsvd = dwErrCode = GetLastError();

		//Whenever a disk changer switches disks, it may render the device
		//handle invalid.  We try to catch these errors here.
		if(!bBeenHereBefore &&
			(dwErrCode == ERROR_MEDIA_CHANGED || dwErrCode == ERROR_INVALID_HANDLE)){
			CloseHandle(hDevice);
			GetDriveInformation();
			bBeenHereBefore=TRUE;
			goto start;
		}
	}

	return lpsrb->SRB_Status;
}

//------------------------------------------------------------------
struct TcdTextBlock {
	BYTE type;
	BYTE trackNumber;
	BYTE sequenceNumber;
	BYTE offset : 4;
	BYTE block : 3;
	BYTE bDBC : 1;
	BYTE data[12];
	BYTE crc0, crc1;
};

int TcdInfo::parseCDText(BYTE *buf)
{
	int result=1;
	char *pEnd;
	int pos=0;
	TcdTextBlock *p;
	static char dst[CB_CDROMSECTOR];

	int n = (((buf[0]<<8)|buf[1]) - 2) / 18;
	if(n<110){
		for(int i=0; i<n; i++){
			p = (TcdTextBlock *)&buf[i * 18 + 4];
			if(p->block==0){
				memcpy(dst+pos, p->data, 12);
				pos+=12;
				while(pos>0 && (pEnd = (char*)memchr(dst, 0, pos))!=0){
					convertA2T(dst, t);
					if(p->type==0x80){ //Album or Track title
						if(p->trackNumber==0) cpStr(Album, t);
						else if(p->trackNumber<=trackN2) cpStr(Tracks[p->trackNumber].title, t);
						result++;
					}
					else if(p->type==0x81){ //Artist
						if(p->trackNumber==0){
							cpStr(Artist, t);
							result++;
						}
					}
					pEnd++;
					int k = int(pEnd - dst);
					pos -= k;
					memmove(dst, pEnd, pos);
					while(pos>0 && !dst[0]){
						pos--;
						memmove(dst, dst + 1, pos);
					}
					p->trackNumber++;
				}
			}
		}
	}
	return result;
}

//------------------------------------------------------------------

int setCDSpeed()
{
	static BYTE pbtBuffer[16];
	static BYTE cmd[12];

	aminmax(cdSpeed, 0, sizeA(cdSpeedTab));
	unsigned nSpeed= cdSpeedTab[cdSpeed];
	if(nSpeed){

		switch(cdMethod){
			default:
				cmd[0] = 0xBB;
				cmd[1] = (BYTE)(LunID<<5);
				nSpeed = unsigned(176.4 * nSpeed + 1);
				cmd[2] = HIBYTE(nSpeed);
				cmd[3] = LOBYTE(nSpeed);
				cmd[4] = cmd[5] = 0xFF;
				sendASPI(cmd, 12, 0, 0);
				/*
				cmd[0] = 0x5A;//MODESENSE
				cmd[2] = 0x2A;
				cmd[1] = cmd[3] = cmd[4] = cmd[5] = 0;
				cmd[7] = 1;
				sendASPI(cmd,12,pbtBuffer,256);
				msg("%d, %d", nSpeed, (pbtBuffer[22]<<8)|pbtBuffer[23]);
				*/
				break;
			case READSONY:
				cmd[0]=0x15;  // MODE_SELECT
				cmd[1]=0x10;  // no save page
				cmd[2]=0;     // reserved
				cmd[3]=0;     // reserved
				cmd[4]=8;     // sizeof(mode)
				cmd[5]=0;     // reserved
				pbtBuffer[4]=0x31;
				pbtBuffer[5]=2;
				pbtBuffer[6] = (BYTE)(nSpeed / 2);
				sendASPI(cmd, 6, pbtBuffer, 8);
				break;
		}
	}
	return 1;
}
//------------------------------------------------------------------
const struct TdriveData {
	BYTE cmd;
	BYTE cmdSize;
	BYTE byte9;
	BYTE offset;
}
driveData[]={
	{0xBE, 12, 0xF8, 7},
	{0xBE, 12, 0xF8, 7},
	{0xBE, 12, 0x10, 7},
	{0xBE, 12, 0x10, 7},
	{0x28, 10, 0, 7},
	{0xD4, 10, 0, 7},
	{0xD8, 12, 0, 8},
	{0xD4, 12, 0, 8},
	{0xD5, 10, 0, 7},
	{0xA8, 12, 0, 8},
};

int rip()
{
	BYTE cmd[12];

	//set speed
	if(cdPeriodicSpeed){
		static LONG lastSpeedSector;
		int d= lSector-lastSpeedSector;
		if(d>cdSpeedPeriod || d<0){
			lastSpeedSector=lSector;
			setCDSpeed();
		}
	}

	//aspi command
	aminmax(cdMethod, 0, sizeA(driveData));
	const TdriveData *dd= &driveData[cdMethod];
	cmd[0] = dd->cmd;
	cmd[1] = (BYTE)(LunID<<5);
	if(cdMethod==READMMC2 || cdMethod==READMMC4) cmd[1]|=4;
	cmd[2] = (BYTE)(lSector >> 24);
	cmd[3] = (BYTE)((lSector >> 16) & 0xFF);
	cmd[4] = (BYTE)((lSector >> 8) & 0xFF);
	cmd[5] = (BYTE)(lSector & 0xFF);
	cmd[6] = cmd[7] = cmd[10] = cmd[11] = 0;

	cmd[9]=dd->byte9;
	cmd[dd->offset]=HIBYTE(dwNumSectors);
	cmd[dd->offset+1]=LOBYTE(dwNumSectors);

	memset(pbtReadBuffer, 0, dwReadBufferSize);

	if(sendASPI(cmd, dd->cmdSize,
		pbtReadBuffer, dwReadBufferSize) !=SS_COMP)
		return 0;

	//byte swapping
	if(cdBigEndian){
		BYTE* pbtBuffer=pbtReadBuffer;
		for(DWORD i=0; i<dwReadBufferSize; i+=2){
			BYTE w = pbtBuffer[1];
			pbtBuffer[1] = pbtBuffer[0];
			pbtBuffer[0] = w;
			pbtBuffer += 2;
		}
	}
	//channel swapping
	if(cdSwapLR){
		WORD *psBuffer=(WORD*)pbtReadBuffer;
		for(DWORD i = 0; i < dwReadBufferSize; i+=4){
			WORD w = psBuffer[1];
			psBuffer[1] = psBuffer[0];
			psBuffer[0] = w;
			psBuffer += 2;
		}
	}
	return 1;
}
//------------------------------------------------------------------

int TcdInfo::driveCmd(int action)
{
	if(inqData.btDeviceType==DTC_CDROM || inqData.btDeviceType==DTC_WORM){
		if(action==1) return rip();
		if(action==2) return setCDSpeed();
		static BYTE buf[CB_CDROMSECTOR];
		BYTE cmd[10]={0x43, BYTE(LunID<<5), BYTE(action), 0, 0, 0, 1, CB_CDROMSECTOR>>8, CB_CDROMSECTOR & 255, 0};
		if(sendASPI(cmd, sizeof(cmd), buf, sizeof(buf))==SS_COMP){
			if(action==5) return parseCDText(buf);
			//get number of all tracks
			int n= ((buf[0] << 8) + buf[1] - 2) / 8 - 1;
			if(n==trackN2 || n==trackN2+1){
				trackN2=n;
				//copy result to struct TcdInfo
				BYTE *p= buf+4;
				for(int i=0; i<=trackN2; i++){
					Tracks[i+1].start= (p[4]<<24 | p[5]<<16 | p[6]<<8 | p[7])+cdBegin;
					if(i>0){
						DWORD t=Tracks[i+1].start-Tracks[i].start;
						if(t<Tracks[i].length || !Tracks[i].length){
							Tracks[i].length=t;
						}
					}
					p+=8;
				}
				return 1;
			}
		}
	}
	return 0;
}

int TcdInfo::aspiCmd(int action)
{
	int result=0;
	if(initAspi()){
		DWORD stat = GetASPI32SupportInfo();
		int numHA = LOBYTE(LOWORD(stat));
		if(HIBYTE(LOWORD(stat)) == SS_COMP){
			for(AdapterID=0; AdapterID < numHA; AdapterID++){
				for(TargetID = 0; TargetID < 16; TargetID++){
					for(LunID = 0; LunID <= 7; LunID++){
						BYTE cmd[6] ={SCSI_CMD_INQUIRY, BYTE(LunID<<5), 0, 0, sizeof(SCSI_INQUIRY_RESULT), 0};
						if(sendASPI(cmd, sizeof(cmd), (BYTE*)&inqData, sizeof(SCSI_INQUIRY_RESULT))==SS_COMP){
							result=driveCmd(action);
							if(result) goto lclose;
						}
					}
				}
			}
		}
	lclose:
		closeAspi();
	}
	return result;
}

int TcdInfo::ntscsiCmd(int action)
{
	int result= GetDriveInformation();
	if(result){
		SendASPI32Command= NtScsiSendASPI32Command;
		result=driveCmd(action);
	}
	CloseHandle(hDevice);
	return result;
}

int TcdInfo::aspiCommand(int action)
{
	static CCritSec c;
	c.Lock();
	int result;
	for(int n=0;; n++){
		result= ntscsiCmd(action);
		if(!result) result=aspiCmd(action);
		if(result || n>5) break;
		Sleep(20);
	}
	c.Unlock();
	return result;
}

//------------------------------------------------------------------

//working thread rips CD sectors and writes them to the audio buffer
DWORD __stdcall ripThreadFunc(void* p)
{
	aminmax(cdReadSectors, 1, 54);
	dwNumSectors= cdReadSectors;
	dwReadBufferSize= dwNumSectors * CB_CDDASECTORSIZE;
	pbtReadBuffer= new BYTE[dwReadBufferSize];
	AudioFilter *ds= (AudioFilter*)p;

	cd.setSpeed();
	LONG remain;
	for(;;){
		Ttrack *t=&cd.Tracks[getCDtrack()];
		remain= t->start+t->length-cdBegin-lSector;
		if(remain <= (LONG)dwNumSectors){
			if(remain<=0){
				//end of track
				AudioFilter::ending=true;
				PostMessage(hWin, MM_MCINOTIFY, MCI_NOTIFY_SUCCESSFUL, 0);
				break;
			}
			dwNumSectors=remain;
			dwReadBufferSize= dwNumSectors * CB_CDDASECTORSIZE;
		}
		//rip sectors from CD
		if(!cd.getSectors()){
			PostMessage(hWin, MM_MCINOTIFY, MCI_NOTIFY_FAILURE, 0);
			SetEvent(ds->playEvent);
			goto lend;
		}
		lSector+=dwNumSectors;

		//call Visualization
		ds->LastMediaSampleSize= dwReadBufferSize;
		if(SUCCEEDED(ds->Transform(pbtReadBuffer, 133333*(LONGLONG)(lSector-cd.Tracks[getCDtrack()].start+cdBegin)))){
			//write samples to DirectSound buffer
			if(ds->receive(pbtReadBuffer, dwReadBufferSize)!=DS_OK) break;
		}
		else{
			if(ds->m_State==State_Stopped) break;
		}
	}
lend:
	delete[] pbtReadBuffer;
	return 0;
}

//start working thread and play
bool ripStartThread()
{
	cdAudioFilter->m_pInput->m_bFlushing=FALSE;
	DWORD id;
	ripThread=CreateThread(0, 0, ripThreadFunc, cdAudioFilter, 0, &id);
	if(!ripThread) return true;
	cdAudioFilter->Run(0);
	return false;
}

//finish working thread
void ripEndThread()
{
	cdAudioFilter->m_pInput->m_bFlushing=TRUE;
	SetEvent(cdAudioFilter->notifyEvent);
	WaitForSingleObject(ripThread, INFINITE);
	CloseHandle(ripThread);
	ripThread=0;
}

//destroy Audio Renderer Filter and finish working thread
void ripShut()
{
	if(!cdAudioFilter) return;
	ripEndThread();
	RELEASE(cdAudioFilter);
}

int ripStart()
{
	ripShut();
	lSector= cd.Tracks[getCDtrack()].start-cdBegin;
	//create filter
	HRESULT hr=S_OK;
	AudioFilter *ds = new AudioFilter(&hr);
	if(hr!=S_OK){
		delete ds;
		return 1;
	}
	cdAudioFilter=ds;
	ds->AddRef();
	//create audio buffer
	WAVEFORMATEX fmt;
	fmt.cbSize=0;
	fmt.nAvgBytesPerSec=176400;
	fmt.nBlockAlign=4;
	fmt.nChannels=2;
	fmt.nSamplesPerSec=44100;
	fmt.wBitsPerSample=16;
	fmt.wFormatTag=WAVE_FORMAT_PCM;
	if(ds->createBuffer(&fmt)!=DS_OK) return 2;
	ds->format.Format = fmt;
	if(!pBA) ds->QueryInterface(IID_IBasicAudio, (void **)&pBA);

	//set writePos and refTimeDiff
	ds->Pause();
	REFERENCE_TIME r;
	ds->GetTime(&r);
	refTimeDiff= -r;
	//wait until dsPreBuffer ms are in the buffer and start playing
	return ripStartThread() ? 3 : 0;
}

bool ripSeek(LONG frame)
{
	//flush old data
	cdAudioFilter->Pause();
	ripEndThread();
	cdAudioFilter->flush(0);
	cdAudioFilter->setBeginTime();
	//set new position
	refTimeDiff= 133333*(LONGLONG)frame-cdAudioFilter->beginTime;
	lSector= cd.Tracks[getCDtrack()].start-cdBegin+frame;
	//start working thread and play
	return ripStartThread();
}

bool ripPos()
{
	REFERENCE_TIME pos;
	if(!cdAudioFilter || FAILED(cdAudioFilter->GetTime(&pos))) return false;
	position= pos+refTimeDiff;
	return true;
}
