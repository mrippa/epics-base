/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   5-88
 */

#ifndef INCLserverh
#define INCLserverh

#ifdef epicsExportSharedSymbols
#   error suspect that libCom is being exported from rsrv?
#endif /* ifdef epicsExportSharedSymbols */

#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "bucketLib.h"
#include "asLib.h"
#include "dbAddr.h"
#include "dbNotify.h"
#define CA_MINOR_PROTOCOL_REVISION 11
#include "caProto.h"
#include "ellLib.h"
#include "epicsTime.h"
#include "epicsAssert.h"

#define epicsExportSharedSymbols
#include "rsrv.h"

#define LOCAL static

/* a modified ca header with capacity for large arrays */
typedef struct caHdrLargeArray {
    ca_uint32_t m_postsize;     /* size of message extension */
    ca_uint32_t m_count;        /* operation data count      */
    ca_uint32_t m_cid;          /* channel identifier        */
    ca_uint32_t m_available;    /* protocol stub dependent   */
    ca_uint16_t m_dataType;     /* operation data type       */
    ca_uint16_t m_cmmd;         /* operation to be performed */
}caHdrLargeArray;

/*
 * !! buf must be the first item in this structure !!
 * This guarantees that buf will have 8 byte natural
 * alignment
 *
 * The terminating unsigned pad0 field is there to force the
 * length of the message_buffer to be a multiple of 8 bytes.
 * This is due to the sequential placing of two message_buffer
 * structures (trans, rec) within the client structure.
 * Eight-byte alignment is required by the Sparc 5 and other RISC
 * processors.
 */
enum messageBufferType { mbtUDP, mbtSmallTCP, mbtLargeTCP };
struct message_buffer {
  char                      *buf;
  unsigned                  stk;
  unsigned                  maxstk;
  unsigned                  cnt;    
  enum messageBufferType    type;
};

typedef struct client {
  ELLNODE               node;
  struct message_buffer send;
  struct message_buffer recv;
  epicsMutexId          lock;
  epicsMutexId          putNotifyLock;
  epicsMutexId          addrqLock;
  epicsMutexId          eventqLock;
  ELLLIST               addrq;
  ELLLIST               putNotifyQue;
  struct sockaddr_in    addr;
  epicsTimeStamp        time_at_last_send;
  epicsTimeStamp        time_at_last_recv;
  void                  *evuser;
  char                  *pUserName;
  char                  *pHostName;
  epicsEventId          blockSem; /* used whenever the client blocks */
  SOCKET                sock;
  int                   proto;
  epicsThreadId         tid;
  unsigned              minor_version_number;
  ca_uint32_t           seqNoOfReq; /* for udp  */
  unsigned              recvBytesToDrain;
  unsigned              priority;
  char                  disconnect; /* disconnect detected */
} client;

/*
 * per channel structure 
 * (stored in addrq off of a client block)
 */
struct channel_in_use {
    ELLNODE node;
    ELLLIST eventq;
    struct client *client;
    struct rsrv_put_notify *pPutNotify; /* potential active put notify */
    const unsigned cid;    /* client id */
    const unsigned sid;    /* server id */
    epicsTimeStamp time_at_creation;   /* for UDP timeout */
    struct dbAddr addr;
    ASCLIENTPVT asClientPVT;
};

/*
 * Event block extension for channel access
 * some things duplicated for speed
 */
struct event_ext {
    ELLNODE                 node;
    caHdrLargeArray         msg;
    struct channel_in_use   *pciu;
    struct event_block      *pdbev;     /* ptr to db event block */
    unsigned                size;       /* for speed */
    unsigned                mask;
    char                    modified;   /* mod & ev flw ctrl enbl */
    char                    send_lock;  /* lock send buffer */
};

/*  NOTE: external used so they remember the state across loads */
#ifdef  GLBLSOURCE
#   define GLBLTYPE
#   define GLBLTYPE_INIT(A)
#else
#   define GLBLTYPE extern
#   define GLBLTYPE_INIT(A)
#endif

/*
 *  for debug-level dependent messages:
 */
#ifdef DEBUG
#   define DLOG(LEVEL,ARGSINPAREN) \
    if (CASDEBUG > LEVEL) errlogPrintf ARGSINPAREN
#else
#   define DLOG(LEVEL,ARGSINPAREN) 
#endif

GLBLTYPE int                CASDEBUG;
GLBLTYPE SOCKET             IOC_sock;
GLBLTYPE SOCKET             IOC_cast_sock;
GLBLTYPE unsigned short     ca_server_port;
GLBLTYPE ELLLIST            clientQ; /* locked by clientQlock */
GLBLTYPE ELLLIST            beaconAddrList;
GLBLTYPE epicsMutexId       clientQlock;
GLBLTYPE struct client      *prsrv_cast_client;
GLBLTYPE BUCKET             *pCaBucket;
GLBLTYPE void               *rsrvClientFreeList; 
GLBLTYPE void               *rsrvChanFreeList;
GLBLTYPE void               *rsrvEventFreeList;
GLBLTYPE void               *rsrvSmallBufFreeListTCP; 
GLBLTYPE void               *rsrvLargeBufFreeListTCP; 
GLBLTYPE unsigned           rsrvSizeofLargeBufTCP;
GLBLTYPE void               *rsrvPutNotifyFreeList; 

#define CAS_HASH_TABLE_SIZE 4096

#define SEND_LOCK(CLIENT) epicsMutexMustLock((CLIENT)->lock)
#define SEND_UNLOCK(CLIENT) epicsMutexUnlock((CLIENT)->lock)

#define LOCK_CLIENTQ    epicsMutexMustLock (clientQlock);
#define UNLOCK_CLIENTQ  epicsMutexUnlock (clientQlock);

void camsgtask (struct client *client);
void cas_send_bs_msg ( struct client *pclient, int lock_needed );
void cas_send_dg_msg ( struct client *pclient );
void rsrv_online_notify_task (void);
int cast_server (void);
struct client *create_client ( SOCKET sock, int proto );
void destroy_client ( struct client * );
struct client *create_tcp_client ( SOCKET sock );
void destroy_tcp_client ( struct client * );
void casAttachThreadToClient ( struct client * );
int camessage ( struct client *client );
void write_notify_reply ( void *pArg );
int rsrvCheckPut ( const struct channel_in_use *pciu );
int rsrv_version_reply ( struct client *client );
void rsrvFreePutNotify ( struct client *pClient, 
                        struct rsrv_put_notify *pNotify );
void initializePutNotifyFreeList (void);
unsigned rsrvSizeOfPutNotify ( struct rsrv_put_notify *pNotify );

/*
 * inclming protocol maintetnance
 */
void casExpandRecvBuffer ( struct client *pClient, ca_uint32_t size );

/*
 * outgoing protocol maintenance
 */
void casExpandSendBuffer ( struct client *pClient, ca_uint32_t size );
int cas_copy_in_header ( 
    struct client *pClient, ca_uint16_t response, ca_uint32_t payloadSize,
    ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
    ca_uint32_t responseSpecific, void **pPayload );
void cas_set_header_cid ( struct client *pClient, ca_uint32_t );
void cas_commit_msg ( struct client *pClient, ca_uint32_t size );

#endif /*INCLserverh*/
