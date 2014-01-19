/*
** Copyright (C) 1999 Albert L. Faber
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef ASPI_INCLUDED
#define ASPI_INCLUDED

// 1 byte alignment for SCSI structures
#pragma pack(push,1)

typedef void(*POSTPROCFUNC)();

#define	SRB_DIR_IN			0x08	// Data transfer from SCSI target to host.
#define SRB_DIR_OUT			0x10	// Data transfer from host to SCSI target.
#define	SRB_EVENT_NOTIFY		0x40	// Enable ASPI command event notification. See section on event notification below.

#define DTC_WORM	0x04	// Write-once device 
#define DTC_CDROM	0x05	// CD-ROM device 

#define SS_PENDING                0x00   /* SRB being processed            */
#define SS_COMP                   0x01   /* SRB completed without error    */
#define SS_ABORTED                0x02   /* SRB aborted                    */
#define SS_ERR                    0x04   /* SRB completed with error       */

#define	SC_EXEC_SCSI_CMD	0x02	// Execute SCSI I/O.

#define CB_CDDASECTORSIZE	2352
#define CB_CDROMSECTOR		2048

typedef struct
{
	BYTE        SRB_Cmd;		// ASPI command code
	BYTE        SRB_Status;		// ASPI command status byte
	BYTE        SRB_HaId;		// ASPI host adapter number
	BYTE        SRB_Flags;		// ASPI request flags
	DWORD       SRB_Hdr_Rsvd;	// Reserved, MUST = 0
}
SRB_HEADER, *LPSRB;

typedef struct
{
	BYTE	 SRB_Cmd;		// ASPI command code = SC_HA_INQUIRY
	BYTE	 SRB_Status;		// ASPI command status byte
	BYTE	 SRB_HaId;		// ASPI host adapter number
	BYTE	 SRB_Flags;		// ASPI request flags
	DWORD	 SRB_Hdr_Rsvd;		// Reserved, MUST = 0
	BYTE	 HA_Count;		// Number of host adapters present
	BYTE	 HA_SCSI_ID;		// SCSI ID of host adapter
	BYTE	 HA_ManagerId[16];	// String describing the manager
	BYTE	 HA_Identifier[16];	// String describing the host adapter
	BYTE	 HA_Unique[16];		// Host Adapter Unique parameters
	WORD	 HA_Rsvd1;
}
SRB_HAINQUIRY, *LPSRB_HAINQUIRY;

#define SENSE_LEN 14	// Maximum sense length

typedef struct
{
	BYTE	 SRB_Cmd;			// ASPI command code = SC_EXEC_SCSI_CMD
	BYTE	 SRB_Status;			// ASPI command status byte
	BYTE	 SRB_HaId;			// ASPI host adapter number
	BYTE	 SRB_Flags;			// ASPI request flags
	DWORD	 SRB_Hdr_Rsvd;			// Reserved
	BYTE	 SRB_Target;			// Target's SCSI ID
	BYTE	 SRB_Lun;			// Target's LUN number
	WORD	 SRB_Rsvd1;			// Reserved for Alignment
	DWORD	 SRB_BufLen;			// Data Allocation Length
	BYTE	*SRB_BufPointer;		// Data Buffer Point
	BYTE	 SRB_SenseLen;			// Sense Allocation Length
	BYTE	 SRB_CDBLen;			// CDB Length
	BYTE	 SRB_HaStat;			// Host Adapter Status
	BYTE	 SRB_TargStat;			// Target Status
	void(*SRB_PostProc)();		// Post routine
	void	*SRB_Rsvd2;			// Reserved
	BYTE	 SRB_Rsvd3[16];			// Reserved for expansion
	BYTE	 CDBByte[16];			// SCSI CDB
	BYTE	 SenseArea[SENSE_LEN+2];	// Request Sense buffer
}
SRB_EXECSCSICMD, *LPSRB_EXECSCSICMD;

typedef struct
{
	BYTE	 SRB_Cmd;	/* 00/000 ASPI cmd code == SC_ABORT_SRB	*/
	BYTE	 SRB_Status;	/* 01/001 ASPI command status byte	*/
	BYTE	 SRB_HaID;	/* 02/002 ASPI host adapter number	*/
	BYTE	 SRB_Flags;	/* 03/003 Reserved, must = 0		*/
	DWORD	 SRB_Hdr_Rsvd;	/* 04/004 Reserved, must = 0		*/
	void	*SRB_ToAbort;	/* 08/008 Pointer to SRB to abort	*/
}
SRB_Abort, *PSRB_Abort, FAR *LPSRB_Abort;

typedef unsigned char	Ucbit;
typedef unsigned char	u_char;

// INQUIRY struct
typedef struct
{
	Ucbit	btDeviceType : 5;	// 0
	Ucbit	btPeripheralQualifier : 3;	// 0

	Ucbit	btDeviceTypeModifier : 7;	// 1
	Ucbit	btRMB : 1;	// 1

	Ucbit	btANSIApprovedVersion : 3;	// 2
	Ucbit	btECMAVersion : 3;	// 2
	Ucbit	btISOVersion : 2;	// 2

	Ucbit	btResponseDataFormat : 3;	// 3
	Ucbit	btReserved_3 : 2;	// 3
	Ucbit	btTrmIOP : 1;	// 3
	Ucbit	btAENC : 1;	// 3

	Ucbit	btAdditionalLength;		// 4
	Ucbit	btReserved_5;			// 5
	Ucbit	btReserved_6;			// 6

	Ucbit	btRelAdr : 1;	// 7
	Ucbit	btWBus32 : 1;	// 7
	Ucbit	btWBus16 : 1;	// 7
	Ucbit	btSync : 1;	// 7
	Ucbit	btLinked : 1;	// 7
	Ucbit	btReserved_7 : 1;	// 7
	Ucbit	btCmdQue : 1;	// 7
	Ucbit	btSftRe : 1;	// 7

	char	lpsVendorId[8];			// 8 .. 15
	char	lpsProductId[16];		// 16 .. 31
	char	lpsRevisionLevel[3];		// 32 .. 35
	char    szTerminateChar;
	//	char	btaVendorSpecific[20];		// 36 .. 55
	//	char	btaReserved[40];		// 56 .. 95
}
SCSI_INQUIRY_RESULT;


#define SCSI_CMD_INQUIRY (0x12)

#pragma pack(pop)


typedef struct
{
	USHORT	 Length;
	UCHAR	 ScsiStatus;
	UCHAR	 PathId;
	UCHAR	 TargetId;
	UCHAR	 Lun;
	UCHAR	 CdbLength;
	UCHAR	 SenseInfoLength;
	UCHAR	 DataIn;
	ULONG	 DataTransferLength;
	ULONG	 TimeOutValue;
	PVOID	 DataBuffer;
	ULONG	 SenseInfoOffset;
	UCHAR	 Cdb[16];
}
SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

typedef struct
{
	SCSI_PASS_THROUGH_DIRECT spt;
	UCHAR			 ucSenseBuf[32];
}
SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

typedef struct
{
	ULONG	 Length;
	UCHAR	 PortNumber;
	UCHAR	 PathId;
	UCHAR	 TargetId;
	UCHAR	 Lun;
}
SCSI_ADDRESS, *PSCSI_ADDRESS;

#define SCSI_IOCTL_DATA_OUT		0
#define SCSI_IOCTL_DATA_IN		1

#define METHOD_BUFFERED		0

#ifndef _WINIOCTL_
#define FILE_ANY_ACCESS		0
#define FILE_READ_ACCESS	1
#define FILE_WRITE_ACCESS	2

#define CTL_CODE(DevType, Function, Method, Access) \
	(((DevType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#endif


#define IOCTL_SCSI_BASE 0x00000004
#define IOCTL_SCSI_PASS_THROUGH		CTL_CODE(IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT	CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_GET_ADDRESS		CTL_CODE(IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef DWORD(*GETASPI32SUPPORTINFO)(void);
typedef DWORD(*SENDASPI32COMMAND)(LPSRB lpSRB);

#endif
