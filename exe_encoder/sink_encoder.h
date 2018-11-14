/******************************************************************************
*
* Copyright (C) 2018 Allegro DVT2.  All rights reserved.
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

#pragma once

#include "lib_app/timing.h"
#include "QPGenerator.h"
#include "EncCmdMngr.h"
#include "CommandsSender.h"

#include "TwoPassMngr.h"

#include "FileUtils.h"


#include <string>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <map>

static bool PreprocessQP(uint8_t* pQPs, const AL_TEncSettings& Settings, const AL_TEncChanParam& tChParam, const std::string& sQPTablesFolder, int iFrameCountSent)
{
  uint8_t* pSegs = NULL;
  return GenerateQPBuffer(Settings.eQpCtrlMode, tChParam.tRCParam.iInitialQP,
                          tChParam.tRCParam.iMinQP, tChParam.tRCParam.iMaxQP,
                          AL_GetWidthInLCU(tChParam), AL_GetHeightInLCU(tChParam),
                          tChParam.eProfile, sQPTablesFolder, iFrameCountSent, pQPs + EP2_BUF_QP_BY_MB.Offset, pSegs);
}

class QPBuffers
{
public:
  struct QPLayerInfo
  {
    BufPool* bufPool;
    std::string sQPTablesFolder;
    std::string sRoiFileName;
  };

  QPBuffers(const AL_TEncSettings& settings) :
    isExternQpTable(settings.eQpCtrlMode & (MASK_QP_TABLE_EXT)), settings(settings)
  {
  }

  ~QPBuffers()
  {

    for(auto roiCtx = mQPLayerRoiCtxs.begin(); roiCtx != mQPLayerRoiCtxs.end(); roiCtx++)
      AL_RoiMngr_Destroy(roiCtx->second);

  }

  void SetBufPool(QPLayerInfo& qpLayerInfo)
  {
    initLayer(qpLayerInfo, 0);
  }

  AL_TBuffer* getBuffer(int frameNum)
  {
    return getBufferP(frameNum, 0);
  }


  void releaseBuffer(AL_TBuffer* buffer)
  {
    if(!isExternQpTable || !buffer)
      return;
    AL_Buffer_Unref(buffer);
  }

private:
  void initLayer(QPLayerInfo& qpLayerInfo, int iLayerID)
  {
    // set QpBuf memory to 0 for traces
    std::vector<AL_TBuffer*> qpBufs;

    while(auto curQp = qpLayerInfo.bufPool->GetBuffer(AL_BUF_MODE_NONBLOCK))
    {
      qpBufs.push_back(curQp);
      Rtos_Memset(AL_Buffer_GetData(curQp), 0, curQp->zSize);
    }

    for(auto qpBuf : qpBufs)
      AL_Buffer_Unref(qpBuf);

    mQPLayerInfos[iLayerID] = qpLayerInfo;
    auto& tChParam = settings.tChParam[iLayerID];
    mQPLayerRoiCtxs[iLayerID] = AL_RoiMngr_Create(tChParam.uWidth, tChParam.uHeight, tChParam.eProfile, AL_ROI_QUALITY_LOW, AL_ROI_QUALITY_ORDER);
  }

  AL_TBuffer* getBufferP(int frameNum, int iLayerID)
  {
    if(!isExternQpTable || mQPLayerInfos.find(iLayerID) == mQPLayerInfos.end())
      return nullptr;

    auto& layerInfo = mQPLayerInfos[iLayerID];
    auto& tLayerChParam = settings.tChParam[iLayerID];

    AL_TBuffer* pQpBuf = layerInfo.bufPool->GetBuffer();
    bool bRet = PreprocessQP(AL_Buffer_GetData(pQpBuf), settings, tLayerChParam, layerInfo.sQPTablesFolder, frameNum);

    if(!bRet)
      bRet = GenerateROIBuffer(mQPLayerRoiCtxs[iLayerID], layerInfo.sRoiFileName, AL_GetWidthInLCU(tLayerChParam), AL_GetHeightInLCU(tLayerChParam),
                               tLayerChParam.eProfile, frameNum, AL_Buffer_GetData(pQpBuf) + EP2_BUF_QP_BY_MB.Offset);

    if(!bRet)
    {
      releaseBuffer(pQpBuf);
      return nullptr;
    }

    return pQpBuf;
  }

private:
  bool isExternQpTable;
  const AL_TEncSettings& settings;
  std::map<int, QPLayerInfo> mQPLayerInfos;
  std::map<int, AL_TRoiMngrCtx*> mQPLayerRoiCtxs;
};



static AL_ERR g_EncoderLastError = AL_SUCCESS;

AL_ERR GetEncoderLastError()
{
  return g_EncoderLastError;
}

static
const char* EncoderErrorToString(AL_ERR eErr)
{
  switch(eErr)
  {
  case AL_ERR_STREAM_OVERFLOW: return "Stream Error: Stream overflow";
  case AL_ERR_TOO_MANY_SLICES: return "Stream Error: Too many slices";
  case AL_ERR_CHAN_CREATION_NO_CHANNEL_AVAILABLE: return "Channel creation failed, no channel available";
  case AL_ERR_CHAN_CREATION_RESOURCE_UNAVAILABLE: return "Channel creation failed, processing power of the available cores insufficient";
  case AL_ERR_CHAN_CREATION_NOT_ENOUGH_CORES: return "Channel creation failed, couldn't spread the load on enough cores";
  case AL_ERR_REQUEST_MALFORMED: return "Channel creation failed, request was malformed";
  case AL_ERR_NO_MEMORY: return "Memory shortage detected (DMA, embedded memory or virtual memory)";
  case AL_WARN_LCU_OVERFLOW: return "Warning some LCU exceed the maximum allowed bits";
  case AL_SUCCESS: return "Success";
  default: return "Unknown error";
  }
}

static
void ThrowEncoderError(AL_ERR eErr)
{
  auto const msg = EncoderErrorToString(eErr);

  Message(CC_RED, "%s\n", msg);
  switch(eErr)
  {
  case AL_SUCCESS:
  case AL_ERR_STREAM_OVERFLOW:
  case AL_WARN_LCU_OVERFLOW:
    // do nothing
    break;

  default:
    throw codec_error(msg, eErr);
  }

  if(eErr != AL_SUCCESS && (eErr != AL_WARN_LCU_OVERFLOW || g_EncoderLastError == AL_SUCCESS))
    g_EncoderLastError = eErr;
}

struct safe_ifstream
{
  std::ifstream fp {};
  safe_ifstream(std::string const& filename, bool binary)
  {
    /* support no file at all */
    if(filename.empty())
      return;
    OpenInput(fp, filename, binary);
  }
};

struct EncoderSink : IFrameSink
{
  EncoderSink(ConfigFile const& cfg, TScheduler* pScheduler, AL_TAllocator* pAllocator, BufPool & qpBufPool
              ) :
    CmdFile(cfg.sCmdFileName, false),
    EncCmd(CmdFile.fp, cfg.RunInfo.iScnChgLookAhead, cfg.Settings.tChParam[0].tGopParam.uFreqLT),
    twoPassMngr(cfg.sTwoPassFileName, cfg.Settings.TwoPass, cfg.Settings.bEnableFirstPassCrop, cfg.Settings.tChParam[0].tGopParam.uGopLength,
                cfg.Settings.tChParam[0].tRCParam.uCPBSize / 90, cfg.Settings.tChParam[0].tRCParam.uInitialRemDelay / 90, cfg.FileInfo.FrameRate),
    qpBuffers(cfg.Settings)
  {
    QPBuffers::QPLayerInfo qpInf
    {
      &qpBufPool,
      cfg.sQPTablesFolder,
      cfg.sRoiFileName
    };

    qpBuffers.SetBufPool(qpInf);

    AL_CB_EndEncoding onEndEncoding = { &EncoderSink::EndEncoding, this };

    AL_ERR errorCode = AL_Encoder_Create(&hEnc, pScheduler, pAllocator, &cfg.Settings, onEndEncoding);

    if(errorCode)
      ThrowEncoderError(errorCode);


    commandsSender.reset(new CommandsSender(hEnc));
    BitstreamOutput.reset(new NullFrameSink);
    RecOutput.reset(new NullFrameSink);
    m_pictureType = cfg.RunInfo.printPictureType ? SLICE_MAX_ENUM : -1;
  }

  ~EncoderSink()
  {
    Message(CC_DEFAULT, "\n\n%d pictures encoded. Average FrameRate = %.4f Fps\n",
            m_picCount, (m_picCount * 1000.0) / (m_EndTime - m_StartTime));

    AL_Encoder_Destroy(hEnc);
  }


  std::function<void(void)> m_done;

  void ProcessFrame(AL_TBuffer* Src) override
  {
    if(m_picCount == 0)
      m_StartTime = GetPerfTime();

    if(Src)
      DisplayFrameStatus(m_picCount);
    else
      Message(CC_DEFAULT, "Flushing...");

    AL_TBuffer* QpBuf = nullptr;

    if(Src)
    {
      EncCmd.Process(commandsSender.get(), m_picCount);



      if(twoPassMngr.iPass)
      {
        auto pPictureMetaTP = AL_TwoPassMngr_CreateAndAttachTwoPassMetaData(Src);

        if(twoPassMngr.iPass == 2)
          twoPassMngr.GetFrame(pPictureMetaTP);

        if(twoPassMngr.iPass == 1 && twoPassMngr.bEnableFirstPassCrop)
          AL_TwoPassMngr_CropBufferSrc(Src);
      }

      QpBuf = qpBuffers.getBuffer(m_picCount);
    }

    std::shared_ptr<AL_TBuffer> QpBufShared(QpBuf, [&](AL_TBuffer* pBuf) { qpBuffers.releaseBuffer(pBuf); });

    if(!AL_Encoder_Process(hEnc, Src, QpBuf))
      throw std::runtime_error("Failed");

    if(Src)
      m_picCount++;
  }


  std::unique_ptr<IFrameSink> RecOutput;
  std::unique_ptr<IFrameSink> BitstreamOutput;
  AL_HEncoder hEnc;
  bool shouldAddDummySei = false;

private:
  int m_picCount = 0;
  int m_pictureType = -1;
  uint64_t m_StartTime = 0;
  uint64_t m_EndTime = 0;
  safe_ifstream CmdFile;
  CEncCmdMngr EncCmd;
  TwoPassMngr twoPassMngr;
  QPBuffers qpBuffers;
  std::unique_ptr<CommandsSender> commandsSender;

  static inline bool isStreamReleased(AL_TBuffer* pStream, AL_TBuffer const* pSrc)
  {
    return pStream && !pSrc;
  }

  static inline bool isSourceReleased(AL_TBuffer* pStream, AL_TBuffer const* pSrc)
  {
    return !pStream && pSrc;
  }

  static void EndEncoding(void* userParam, AL_TBuffer* pStream, AL_TBuffer const* pSrc, int)
  {
    auto pThis = (EncoderSink*)userParam;

    if(isStreamReleased(pStream, pSrc) || isSourceReleased(pStream, pSrc))
      return;


    if(pThis->twoPassMngr.iPass == 1)
    {
      if(!pSrc)
        pThis->twoPassMngr.Flush();
      else
      {
        auto pPictureMetaTP = (AL_TLookAheadMetaData*)AL_Buffer_GetMetaData(pSrc, AL_META_TYPE_LOOKAHEAD);

        if(pThis->twoPassMngr.bEnableFirstPassCrop)
        {
          AL_TwoPassMngr_UncropBufferSrc((AL_TBuffer*)pSrc);

          if(pPictureMetaTP)
            pPictureMetaTP->iPictureSize = (int32_t)(((uint64_t)pPictureMetaTP->iPictureSize * 3500) / 1000);
        }

        pThis->twoPassMngr.AddFrame(pPictureMetaTP);
      }
    }

    pThis->processOutput(pStream);
  }

  void AddSei(AL_TBuffer* pStream, bool isPrefix, int payloadType, uint8_t* payload, int payloadSize)
  {
    int seiSection = AL_Encoder_AddSei(hEnc, pStream, isPrefix, payloadType, payload, payloadSize);

    if(seiSection < 0)
      Message(CC_DEFAULT, "Failed to add dummy SEI (id:%d) \n", seiSection);
  }

  AL_ERR PreprocessOutput(AL_TBuffer* pStream)
  {
    if(AL_ERR eErr = AL_Encoder_GetLastError(hEnc))
      ThrowEncoderError(eErr);

    if(pStream && shouldAddDummySei)
    {
      constexpr int payloadSize = 8 * 10;
      uint8_t payload[payloadSize];

      for(int i = 0; i < payloadSize; ++i)
        payload[i] = i;

      AddSei(pStream, false, 15, payload, payloadSize);
      AddSei(pStream, true, 18, payload, payloadSize);
    }

    if(pStream && m_pictureType != -1)
    {
      auto const pMeta = (AL_TPictureMetaData*)AL_Buffer_GetMetaData(pStream, AL_META_TYPE_PICTURE);
      m_pictureType = pMeta->eType;
      Message(CC_DEFAULT, "Picture Type %i\n", m_pictureType);
    }

    BitstreamOutput->ProcessFrame(pStream);
    return AL_SUCCESS;
  }

  void processOutput(AL_TBuffer* pStream)
  {
    auto eErr = PreprocessOutput(pStream);

    if(eErr != AL_SUCCESS)
      ThrowEncoderError(eErr);

    if(pStream)
    {
      auto bRet = AL_Encoder_PutStreamBuffer(hEnc, pStream);
      assert(bRet);
    }

    TRecPic RecPic;

    while(AL_Encoder_GetRecPicture(hEnc, &RecPic))
    {
      auto buf = WrapBufferYuv(&RecPic.tBuf);
      RecOutput->ProcessFrame(buf);
      AL_Buffer_Destroy(buf);

      AL_Encoder_ReleaseRecPicture(hEnc, &RecPic);
    }

    if(!pStream)
    {
      RecOutput->ProcessFrame(EndOfStream);
      m_EndTime = GetPerfTime();
      m_done();
    }
  }


  static AL_TBuffer* WrapBufferYuv(TBufferYuv* frame)
  {
    AL_TBuffer* pBuf = AL_Buffer_WrapData(frame->tMD.pVirtualAddr, frame->tMD.uSize, NULL);
    AL_TSrcMetaData* pBufMeta = AL_SrcMetaData_Create(frame->tDim, frame->tPlanes[AL_PLANE_Y], frame->tPlanes[AL_PLANE_UV], frame->tFourCC);
    AL_Buffer_AddMetaData(pBuf, (AL_TMetaData*)pBufMeta);
    return pBuf;
  }
};

