/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 * Copyright (C) 1996, 1997 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: sequoia.h,v 1.2 1997/11/11 21:20:16 rob Exp $
 */

typedef unsigned char	UInt8;
typedef unsigned short	UInt16;
typedef unsigned long	UInt32;
typedef UInt32		UInt64[2];

typedef signed char	SInt8;
typedef signed short	SInt16;
typedef signed long	SInt32;
typedef SInt32		SInt64[2];

typedef UInt16 UniChar;
struct UniStr255 {
  UInt16	length;		/* number of unicode characters */
  UniChar	unicode[255];	/* unicode characters */
};
typedef struct UniStr255 UniStr255;
typedef const UniStr255 *ConstUniStr255Param;

typedef UInt32 CatalogNodeID;

/* Signatures used to differentiate between HFS and Sequoia volumes */
enum {
  kHFSSigWord		= 0x4244,
  kHFSPlusSigWord	= 0x4438	/* this will change! */
};

/* Large extent descriptor (Sequoia only) */
struct LargeExtentDescriptor {
  UInt32	startBlock;	/* first allocation block */
  UInt32	blockCount;	/* number of allocation blocks */
};
typedef struct LargeExtentDescriptor LargeExtentDescriptor;

/* Large extent record (Sequoia only) */
typedef LargeExtentDescriptor LargeExtentRecord[8];

/* Fork data info (Sequoia only) - 80 bytes */
struct ForkData {
  UInt64	logicalSize;	/* fork's logical size in bytes */
  UInt32	clumpSize;	/* fork's clump size in bytes */
  UInt32	totalBlocks;	/* total allocation blocks
				   used by this fork */
  LargeExtentRecord
  		extents;	/* initial set of extents */
};
typedef struct ForkData ForkData;

/* VolumeHeader (Sequoia only) - 512 bytes */
/* Stored at sector #0 (1st sector) and last sector */
struct VolumeHeader {
  UInt16	signature;	/* volume signature == kHFSPlusSigWord */
  UInt16	version;	/* version is 1.0 ($0100) */
  UInt32	attributes;	/* volume attributes */
  UInt32	createDate;	/* date and time of volume creation */
  UInt32	modifyDate;	/* date and time of last modification */
  UInt32	backupDate;	/* date and time of last backup */
  UInt32	checkedDate;	/* date and time of last disk check */
  UInt32	fileCount;	/* number of files in volume */
  UInt32	folderCount;	/* number of directories in volume */
  UInt32	blockSize;	/* size (in bytes) of allocation blocks */
  UInt32	totalBlocks;	/* number of allocation blocks in volume
				   (includes this header and VBM)*/
  UInt32	freeBlocks;	/* number of unused allocation blocks */
  UInt32	nextAllocation;	/* start of next allocation search */
  UInt32	rsrcClumpSize;	/* default resource fork clump size */
  UInt32	dataClumpSize;	/* default data fork clump size */
  CatalogNodeID	nextCatalogID;	/* next unused catalog node ID */
  UInt32	writeCount;	/* volume write count */
  UInt64	encodingsBitmap;
  				/* which encodings have been used
				   on this volume */
  UInt8		finderInfo[32];	/* information used by the Finder */
  ForkData	allocationFile;	/* allocation bitmap file */
  ForkData	extentsFile;	/* extents B-tree file */
  ForkData	catalogFile;	/* catalog B-tree file */
  ForkData	attributesFile;	/* extended attributes B-tree file */
  ForkData	startupFile;	/* boot file */
  UInt32	reserved[2];	/* reserved - set to zero */
};
typedef struct VolumeHeader VolumeHeader;

/* Bits 0-6 are reserved (always cleared by MountVol call) */
enum {
  kVolumeHardwareLockBit	= 7,	/* volume is locked by hardware */
  kVolumeUnmountedBit		= 8,	/* volume was successfully unmounted */
  kVolumeSparedBlocksBit	= 9,	/* volume has bad blocks spared */
  kVolumeNoCacheRequiredBit	= 10,	/* don't cache volume blocks */
  kBootVolumeInconsistentBit	= 11,	/* boot volume is inconsistent
					   (System 7.6) */

  /* Bits 12-14 are reserved for future use */
  kVolumeHardLinksBit		= 12,	/* volume has hard links */
  kVolumeSoftwareLockBit	= 15,	/* volume is locked by software */
};

typedef struct {
  UInt32	fLink;		/* forward link */
  UInt32	bLink;		/* backward link */
  SInt8		type;		/* node type */
  SInt8		nHeight;	/* node level */
  UInt16	nRecs;		/* number of records in node */
  UInt16	resv2;		/* reserved */
} BTNodeDescriptor;

typedef struct {
  BTNodeDescriptor
  		node;
  UInt16	treeDepth;
  UInt32	rootNode;
  UInt32 	eafRecords;
  UInt32	firstLeafNode;
  UInt32	lastLeafNode;
  UInt16	nodeSize;
  UInt16	maxKeyLength;
  UInt32	totalNodes;
  UInt32	freeNodes;
  UInt16	reserved1;
  UInt32	clumpSize;	/* misaligned */
  UInt8		btreeType;
  UInt8		reserved2;
  UInt32	attributes;	/* long aligned again */
  UInt32	reserved3[16];
} HeaderRec, *HeaderPtr;

typedef enum {
  kBTBadCloseMask		= 0x00000001,
  kBTBigKeysMask		= 0x00000002,
  kBTVariableIndexKeysMask	= 0x00000004
} BTreeAttributes;

/* Large Catalog key (Sequoia only) */
struct LargeCatalogKey {
  UInt16	keyLength;	/* key length (excluding the length field) */
  CatalogNodeID	parentID;	/* parent folder ID */
  UniStr255	nodeName;	/* catalog node name */
};
typedef struct LargeCatalogKey LargeCatalogKey;

/* Large Extent key (Sequoia only) */
struct LargeExtentKey {
  SInt8		keyLength;	/* length of key, excluding this field */
  SInt8		forkType;	/* 0 = data fork, -1 = resource fork */
  SInt16	pad;		/* make the other fields align on
				   32-bit boundary */
  CatalogNodeID	fileID;		/* file ID */
  UInt32	startBlock;	/* first file allocation block number
				   in this extent */
};
typedef struct LargeExtentKey LargeExtentKey;

/* Key for records in the attributes file. Data is user-defined. */
enum {
  kAttrDontExchangeMask = 0x8000,	/* If set, this property will not be
					   exchanged via ExchangeFiles*/
  kAttrDontExchangeBit = 15
};

struct AttributeKey {
  UInt16	length;		/* must set kBTBigKeysMask in BTree
				   header's attributes */
  UInt16	flags;
  CatalogNodeID	cnid;
  UInt32	creator;
  UInt32	selector;
};
typedef struct AttributeKey AttributeKey;

struct SmallCatalogFolder {
  SInt16	flags;		/* directory flags */
  UInt16	val;		/* directory valence */
  UInt32	dirID;		/* directory ID */
  SInt32	crDat;		/* date and time of creation */
  SInt32	mdDat;		/* date and time of last modification */
  SInt32	bkDat;		/* date and time of last backup */
  DInfo		usrInfo;	/* Finder information */
  DXInfo	fndrInfo;	/* additional Finder information */
  SInt32	resrv[4];	/* reserved */
};
typedef struct SmallCatalogFolder SmallCatalogFolder;

struct SmallCatalogFile {
  SInt8		flags;		/* file flags */
  SInt8		typ;		/* file type */
  FInfo		usrWds;		/* Finder information */
  UInt32	flNum;		/* file ID */
  UInt16	stBlk;		/* first alloc block of data fork */
  UInt32	lgLen;		/* logical EOF of data fork */
  UInt32	pyLen;		/* physical EOF of data fork */
  UInt16	rStBlk;		/* first alloc block of resource fork */
  UInt32	rLgLen;		/* logical EOF of resource fork */
  UInt32	rPyLen;		/* physical EOF of resource fork */
  SInt32	crDat;		/* date and time of creation */
  SInt32	mdDat;		/* date and time of last modification */
  SInt32	bkDat;		/* date and time of last backup */
  FXInfo	fndrInfo;	/* additional Finder information */
  UInt16	clpSize;	/* file clump size */
  ExtDataRec	extRec;		/* first data fork extent record */
  ExtDataRec	rExtRec;	/* first resource fork extent record */
  SInt32	resrv;		/* reserved */
};
typedef struct SmallCatalogFile SmallCatalogFile;

struct SmallCatalogThread {
  SInt32	resrv[2];	/* reserved */
  UInt32	parID;		/* parent ID for this directory */
  Str31		cName;		/* name of this directory */
};
typedef struct SmallCatalogThread SmallCatalogThread;

/* Permissions info (Sequoia only) - 16 bytes */
struct Permissions {
  UInt32	ownerID;	/* user or group ID of file/folder owner */
  UInt32	groupID;	/* additional user of group ID */
  UInt32	permissions;	/* permissions
				   (bytes: unused, owner, group, everyone) */
  UInt32	specialDevice;	/* UNIX: device for
				   character or block special file */
};
typedef struct Permissions Permissions;

/* Large catalog folder record (Sequoia only) - 88 bytes */
struct LargeCatalogFolder {
  SInt16	recordType;	/* record type = Sequoia folder record */
  UInt16	flags;		/* file flags */
  UInt32	valence;	/* folder's valence
				   (limited to 2^16 in Mac OS) */
  CatalogNodeID	folderID;	/* folder ID */
  UInt32	createDate;	/* date and time of creation */
  UInt32	contentModDate;	/* date and time of last
				   content modification */
  UInt32	attributeModDate;
  				/* date and time of last
				   attribute modification */
  UInt32	accessDate;	/* date and time of last
				   access (Rhapsody only) */
  UInt32	backupDate;	/* date and time of last backup */
  Permissions	permissions;	/* permissions (for Rhapsody) */
  DInfo		userInfo;	/* Finder information */
  DXInfo	finderInfo;	/* additional Finder information */
  UInt32	textEncoding;	/* hint for name conversions */
  UInt32	reserved;	/* reserved - set to zero */
};
typedef struct LargeCatalogFolder LargeCatalogFolder;

/* Large catalog file record (Sequoia only) - 248 bytes */
struct LargeCatalogFile {
  SInt16	recordType;	/* record type = Sequoia file record */
  UInt16	flags;		/* file flags */
  UInt32	linkCount;	/* reserved - set to zero */
  CatalogNodeID	fileID;		/* file ID */
  UInt32	createDate;	/* date and time of creation */
  UInt32	contentModDate;	/* date and time of last content
				   (fork) modification */
  UInt32	attributeModDate;
  				/* date and time of last attribute
				   modification */
  UInt32	accessDate;	/* date and time of last access
				   (Rhapsody only) */
  UInt32	backupDate;	/* date and time of last backup */
  Permissions	permissions;	/* permissions (for Rhapsody) */
  FInfo		userInfo;	/* Finder information */
  FXInfo	finderInfo;	/* additional Finder information */
  UInt32	textEncoding;	/* hint for name conversions */
  UInt32	reserved2;	/* reserved - set to zero */

  /* start on double long (64 bit) boundary */
  ForkData	dataFork;	/* size and block data for data fork */
  ForkData	resourceFork;	/* size and block data for resource fork */
};
typedef struct LargeCatalogFile LargeCatalogFile;

/* Catalog file record flags */
enum {
  kFileLockedBit	= 0x0000,	/* file is locked and
					   cannot be written to */
  kFileLockedMask	= 0x0001,
  kFileThreadExistsBit	= 0x0001,	/* a file thread record exists
					   for this file */
  kFileThreadExistsMask	= 0x0002
};

/* Large catalog thread record (Sequoia only) -- 264 bytes */
/* This structure is used for both file and folder thread records. */
struct LargeCatalogThread {
  SInt16	recordType;		/* record type */
  SInt16	reserved;		/* reserved - set to zero */
  CatalogNodeID	parentID;		/* parent ID for this catalog node */
  UniStr255	nodeName;		/* name of this catalog node
					   (variable length) */
};
typedef struct LargeCatalogThread LargeCatalogThread;

union CatalogRecord {
  SInt16		recordType;	/* record type */

  SmallCatalogFolder	smallFolder;	/* HFS folder record */
  SmallCatalogFile	smallFile;	/* HFS file record */
  SmallCatalogThread	smallThread;	/* HFS thread record */

  LargeCatalogFolder	largeFolder;
  LargeCatalogFile	largeFile;
  LargeCatalogThread	largeThread;
};

enum {
  rtypeSeqFolder	= 0x0001,	/* Sequoia Folder record */
  rtypeSeqFile		= 0x0002,	/* Sequoia File record */
  rtypeSeqFoldThread	= 0x0003,	/* Sequoia Folder thread record */
  rtypeSeqFileThread	= 0x0004,	/* Sequoia File thread record */

  rtypeHFSFolder	= 0x0100,	/* Classic HFS Folder record */
  rtypeHFSFile		= 0x0200,	/* Classic HFS File record */
  rtypeHFSFoldThread	= 0x0300,	/* Classic HFS Folder thread record */
  rtypeHFSFileThread	= 0x0400	/* Classic HFS File thread record */
};
