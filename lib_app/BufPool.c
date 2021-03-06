/******************************************************************************
*
* Copyright (C) 2017 Allegro DVT2.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX OR ALLEGRO DVT2 BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of  Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
*
* Except as contained in this notice, the name of Allegro DVT2 shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Allegro DVT2.
*
******************************************************************************/

#include <assert.h>
#include "lib_rtos/lib_rtos.h"

#include "BufPool.h"
#include "BufferMetaFactory.h"
#include "lib_common/Allocator.h"

/* lockless, one producer, one consumer */

static bool Fifo_Init(App_Fifo* pFifo, size_t zMaxElem);
static void Fifo_Deinit(App_Fifo* pFifo);
static bool Fifo_Queue(App_Fifo* pFifo, void* pElem, uint32_t uWait);
static void* Fifo_Dequeue(App_Fifo* pFifo, uint32_t uWait);

/****************************************************************************/
static void FreeBufInPool(AL_TBuffer* pBuf)
{
  AL_TBufPool* pBufPool = AL_Buffer_GetUserData(pBuf);
  Fifo_Queue(&pBufPool->fifo, pBuf, AL_WAIT_FOREVER);
}

/****************************************************************************/
static bool AL_sBufPool_AllocBuf(AL_TBufPool* pBufPool)
{
  AL_TMetaData* pMeta = NULL;

  assert(pBufPool->uNumBuf < pBufPool->config.uNumBuf);

  AL_TBuffer* pBuf = AL_Buffer_Create_And_AllocateNamed(pBufPool->pAllocator, pBufPool->config.zBufSize, FreeBufInPool, pBufPool->config.debugName);

  if(!pBuf)
    goto fail_buffer_init;

  AL_Buffer_SetUserData(pBuf, pBufPool);

  if(pBufPool->config.pMetaData)
  {
    pMeta = AL_MetaData_Clone(pBufPool->config.pMetaData);

    if(!pMeta)
      return false;

    if(!AL_Buffer_AddMetaData(pBuf, pMeta))
      goto fail_buffer_add_meta;
  }

  pBufPool->pPool[pBufPool->uNumBuf++] = pBuf;

  Fifo_Queue(&pBufPool->fifo, pBuf, AL_WAIT_FOREVER);

  return true;

  fail_buffer_add_meta:
  AL_Allocator_Free(pBufPool->pAllocator, pBuf);
  AL_Buffer_Destroy(pBuf);
  fail_buffer_init:

  if(pMeta)
    pMeta->MetaDestroy(pMeta);
  return false;
}

bool AL_BufPool_Init(AL_TBufPool* pBufPool, AL_TAllocator* pAllocator, AL_TBufPoolConfig* pConfig)
{
  if(!pBufPool)
    return false;

  if(!pAllocator)
    return false;

  pBufPool->pAllocator = pAllocator;

  if(!Fifo_Init(&pBufPool->fifo, pConfig->uNumBuf))
    goto fail_init;

  pBufPool->config = *pConfig;
  pBufPool->uNumBuf = 0;

  size_t zMemPoolSize = pConfig->uNumBuf * sizeof(AL_TBuffer*);

  pBufPool->pPool = (AL_TBuffer**)Rtos_Malloc(zMemPoolSize);

  if(!pBufPool->pPool)
    goto fail_alloc_pool;

  // Create uMin free buffers
  while(pBufPool->uNumBuf < pConfig->uNumBuf)
    if(!AL_sBufPool_AllocBuf(pBufPool))
      goto fail_alloc_pool;

  return true;

  fail_alloc_pool:
  AL_BufPool_Deinit(pBufPool);
  fail_init:
  return false;
}

/****************************************************************************/
void AL_BufPool_Deinit(AL_TBufPool* pBufPool)
{
  for(int u = 0; u < pBufPool->uNumBuf; ++u)
  {
    AL_TBuffer* pBuf = pBufPool->pPool[u];
    AL_Allocator_Free(pBufPool->pAllocator, pBuf->hBuf);
    AL_Buffer_Destroy(pBuf);
    pBufPool->pPool[u] = NULL;
  }

  if(pBufPool->config.pMetaData)
    pBufPool->config.pMetaData->MetaDestroy(pBufPool->config.pMetaData);
  Fifo_Deinit(&pBufPool->fifo);
  Rtos_Free(pBufPool->pPool);
  Rtos_Memset(pBufPool, 0, sizeof(*pBufPool));
}

/****************************************************************************/
AL_TBuffer* AL_BufPool_GetBuffer(AL_TBufPool* pBufPool, AL_EBufMode eMode)
{
  uint32_t Wait = AL_GetWaitMode(eMode);

  AL_TBuffer* pBuf = Fifo_Dequeue(&pBufPool->fifo, Wait);

  if(!pBuf)
    return NULL;

  AL_Buffer_Ref(pBuf);
  return pBuf;
}

static bool Fifo_Init(App_Fifo* pFifo, size_t zMaxElem)
{
  pFifo->m_zMaxElem = zMaxElem + 1;
  pFifo->m_zTail = 0;
  pFifo->m_zHead = 0;

  size_t zElemSize = pFifo->m_zMaxElem * sizeof(void*);
  pFifo->m_ElemBuffer = Rtos_Malloc(zElemSize);

  if(!pFifo->m_ElemBuffer)
    return false;
  Rtos_Memset(pFifo->m_ElemBuffer, 0xCD, zElemSize);

  pFifo->hCountSem = Rtos_CreateSemaphore(0);

  if(!pFifo->hCountSem)
  {
    Rtos_Free(pFifo->m_ElemBuffer);
    return false;
  }

  pFifo->hSpaceSem = Rtos_CreateSemaphore(zMaxElem);
  pFifo->hMutex = Rtos_CreateMutex();

  if(!pFifo->hSpaceSem)
  {
    Rtos_DeleteSemaphore(pFifo->hCountSem);
    Rtos_Free(pFifo->m_ElemBuffer);
    return false;
  }

  return true;
}

static void Fifo_Deinit(App_Fifo* pFifo)
{
  Rtos_Free(pFifo->m_ElemBuffer);
  Rtos_DeleteSemaphore(pFifo->hCountSem);
  Rtos_DeleteSemaphore(pFifo->hSpaceSem);
  Rtos_DeleteMutex(pFifo->hMutex);
}

static bool Fifo_Queue(App_Fifo* pFifo, void* pElem, uint32_t uWait)
{
  if(!Rtos_GetSemaphore(pFifo->hSpaceSem, uWait))
    return false;

  Rtos_GetMutex(pFifo->hMutex);
  pFifo->m_ElemBuffer[pFifo->m_zTail] = pElem;
  pFifo->m_zTail = (pFifo->m_zTail + 1) % pFifo->m_zMaxElem;
  Rtos_ReleaseMutex(pFifo->hMutex);

  Rtos_ReleaseSemaphore(pFifo->hCountSem);

  /* new item was added in the queue */
  return true;
}

static void* Fifo_Dequeue(App_Fifo* pFifo, uint32_t uWait)
{
  /* wait if no items */
  if(!Rtos_GetSemaphore(pFifo->hCountSem, uWait))
    return NULL;

  Rtos_GetMutex(pFifo->hMutex);
  void* pElem = pFifo->m_ElemBuffer[pFifo->m_zHead];
  pFifo->m_zHead = (pFifo->m_zHead + 1) % pFifo->m_zMaxElem;
  Rtos_ReleaseMutex(pFifo->hMutex);

  /* new empty space available */
  Rtos_ReleaseSemaphore(pFifo->hSpaceSem);
  return pElem;
}

