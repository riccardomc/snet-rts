#include <mpi.h>

#include "debug.h"
#include "memfun.h"
#include "imanager.h"
#include "distribcommon.h"
#include "interface_functions.h"
#include "omanager.h"
#include "record.h"
#include "reference.h"

#include "mpi_buf.h"

extern int node_location;

inline static void MPISend(void *src, int size, int dest, int type)
{ MPI_Send(src, size, MPI_BYTE, dest, type, MPI_COMM_WORLD); }

inline static void MPISendBuf(mpi_buf_t *buf, int dest, int type)
{ MPI_Send(buf->data, buf->offset, MPI_PACKED, dest, type, MPI_COMM_WORLD); }

snet_msg_t SNetDistribRecvMsg(void)
{
  int count;
  snet_msg_t result;
  MPI_Status status;
  static mpi_buf_t recvBuf = {0, 0, NULL};

  MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
  MPI_Get_count(&status, MPI_PACKED, &count);

  MPI_Pack_size(count, MPI_PACKED, MPI_COMM_WORLD, &recvBuf.offset);
  if ((unsigned) recvBuf.offset > recvBuf.size) {
    SNetMemFree(recvBuf.data);
    recvBuf.data = SNetMemAlloc(recvBuf.offset);
    recvBuf.size = recvBuf.offset;
  }

  MPI_Recv(recvBuf.data, count, MPI_PACKED, MPI_ANY_SOURCE, status.MPI_TAG,
            MPI_COMM_WORLD, &status);

  recvBuf.offset = 0;
  result.type = status.MPI_TAG;

  switch (result.type) {
    case snet_rec:
        result.rec = SNetRecDeserialise(&recvBuf);
    case snet_block:
    case snet_unblock:
      SNetDistribUnpack(&recvBuf, &result.dest, sizeof(snet_dest_t));
      result.dest.node = status.MPI_SOURCE;
      break;
    case snet_ref_set:
      result.ref = SNetRefDeserialise(&recvBuf);
      result.data = (uintptr_t) SNetInterfaceGet(SNetRefInterface(result.ref))->unpackfun(&recvBuf);
      break;
    case snet_ref_fetch:
      result.ref = SNetRefDeserialise(&recvBuf);
      result.data = status.MPI_SOURCE;
      break;
    case snet_ref_update:
      result.ref = SNetRefDeserialise(&recvBuf);
      SNetDistribUnpack(&recvBuf, &result.val, sizeof(result.val));
      break;
    default:
      break;
  }

  return result;
}

void SNetDistribSendRecord(snet_dest_t dest, snet_record_t *rec)
{
  static mpi_buf_t sendBuf = {0, 0, NULL};

  sendBuf.offset = 0;
  SNetRecSerialise(rec, &sendBuf);
  SNetDistribPack(&sendBuf, &dest, sizeof(snet_dest_t));
  MPISendBuf(&sendBuf, dest.node, snet_rec);
}

void SNetDistribFetchRef(snet_ref_t *ref)
{
  mpi_buf_t buf = {0, 0, NULL};
  SNetRefSerialise(ref, &buf);
  MPISendBuf(&buf, SNetRefNode(ref), snet_ref_fetch);
  SNetMemFree(buf.data);
}

void SNetDistribUpdateRef(snet_ref_t *ref, int count)
{
  mpi_buf_t buf = {0, 0, NULL};
  SNetRefSerialise(ref, &buf);
  SNetDistribPack(&buf, &count, sizeof(count));
  MPISendBuf(&buf, SNetRefNode(ref), snet_ref_update);
  SNetMemFree(buf.data);
}

void SNetDistribUpdateBlocked(void)
{ MPISend(NULL, 0, node_location, snet_update); }

void SNetDistribUnblockDest(snet_dest_t dest)
{ MPISend(&dest, sizeof(snet_dest_t), dest.node, snet_unblock); }

void SNetDistribBlockDest(snet_dest_t dest)
{ MPISend(&dest, sizeof(snet_dest_t), dest.node, snet_block); }

void SNetDistribSendData(snet_ref_t *ref, void *data, void *dest)
{
  // Only called from imanager, so no safety issue to use static
  static mpi_buf_t buf = {0, 0, NULL};
  buf.offset = 0;
  SNetRefSerialise(ref, &buf);
  SNetInterfaceGet(SNetRefInterface(ref))->packfun(data, &buf);
  MPISendBuf(&buf, (uintptr_t) dest, snet_ref_set);
}
