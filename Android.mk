# Copyright (C) 2017 Mentor Graphics Inc.
# Copyright (C) 2017 Xilinx Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(TARGET_USES_XILINX_VCU),true)

LOCAL_PATH := $(call my-dir)

#######################
# Common src files
#######################
LIB_FPGA_SRC := \
	lib_fpga/DmaAllocLinux.c \
	lib_fpga/DevicePool.c

LIB_APP_SRC := \
	lib_app/utils.cpp \
	lib_app/convert.cpp \
	lib_app/BufPool.cpp \
	lib_app/BufferMetaFactory.c \
	lib_app/AllocatorTracker.cpp \
	lib_app/console_linux.cpp

LIB_CFG_SRC := \
	lib_cfg/CfgParser.cpp

LIB_COMMON_BASE_SRC := \
	lib_common/Utils.c \
	lib_common/UtilsQp.c \
	lib_common/BufCommon.c \
	lib_common/AllocatorDefault.c \
	lib_common/ChannelResources.c

LIB_COMMON_SRC := \
	$(LIB_COMMON_BASE_SRC) \
	lib_common/MemDesc.c \
	lib_common/HwScalingList.c \
	lib_common/BufferAPI.c \
	lib_common/BufferSrcMeta.c \
	lib_common/BufferCircMeta.c \
	lib_common/BufferStreamMeta.c \
	lib_common/BufferPictureMeta.c \
	lib_common/BufferAccess.c \
	lib_common/Fifo.c \
	lib_common/AvcLevelsLimit.c \
	lib_common/StreamBuffer.c \
	lib_common/FourCC.c

LIB_COMMON_MCU_SRC := \
	$(LIB_COMMON_BASE_SRC)

LIB_COMMON_ENC_BASE_SRC := \
	lib_common_enc/L2PrefetchParam.c \
	lib_common_enc/EncBuffers.c \
	lib_common_enc/EncRecBuffer.c \
	lib_common_enc/IpEncFourCC.c \
	lib_common_enc/EncSize.c

LIB_COMMON_ENC_SRC := \
	$(LIB_COMMON_ENC_BASE_SRC) \
	lib_common_enc/EncHwScalingList.c \
	lib_common_enc/Settings.c \

LIB_COMMON_ENC_MCU_SRC := \
	$(LIB_COMMON_ENC_BASE_SRC)

LIB_CONV_SRC := \
	lib_conv_yuv/AL_NvxConvert.cpp

LIB_RTOS_SRC := \
	lib_rtos/lib_rtos.c

LIB_PREPROCESS_SRC := \
	lib_preprocess/ChooseLda.c \
	lib_preprocess/LoadLda.c

LIB_BITSTREAM_SRC := \
	lib_bitstream/BitStreamLite.c \
	lib_bitstream/RbspEncod.c \
	lib_bitstream/HEVC_RbspEncod.c \
	lib_bitstream/HEVC_SkippedPict.c \
	lib_bitstream/AVC_RbspEncod.c \
	lib_bitstream/AVC_SkippedPict.c

#######################
# liballegro_encode
#######################
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := liblog libcutils

LOCAL_C_INCLUDES := \
	system/core/include/ \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/extra/include

CONFIG_H := $(LOCAL_PATH)/include/config.h
LOCAL_CFLAGS := -DLOG_TAG=\"liballegro_encode\" -include $(CONFIG_H)
LOCAL_CPPFLAGS += -fexceptions

LIB_ENCODE_SRC := \
	lib_encode/SchedulerMcu.c \
	lib_encode/driverDataConversions.c \
	lib_encode/McuTimers.c \
	lib_encode/Com_Encoder.c \
	lib_encode/SourceBufferChecker.c \
	lib_encode/HEVC_Encoder.c \
	lib_encode/AVC_Encoder.c \
	lib_encode/IP_Utils.c \
	lib_encode/IP_Stream.c \
	lib_encode/lib_encoder.c \
	lib_encode/ISchedulerCommon.c \
	lib_encode/hardwareDriver.c \
	lib_encode/HEVC_Sections.c \
	lib_encode/AVC_Sections.c \
	lib_encode/Sections.c \
	lib_encode/NalWriters.c \
	lib_encode/IScheduler.c

LOCAL_SRC_FILES := \
	$(LIB_FPGA_SRC) \
	$(LIB_COMMON_SRC) \
	$(LIB_COMMON_ENC_SRC) \
	$(LIB_RTOS_SRC) \
	$(LIB_ENCODE_SRC) \
	$(LIB_BITSTREAM_SRC) \
	$(LIB_PREPROCESS_SRC)

LOCAL_MODULE := liballegro_encode
include $(BUILD_SHARED_LIBRARY)

#######################
# liballegro_decode
#######################
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := liblog libcutils

LOCAL_C_INCLUDES := \
	system/core/include/ \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/extra/include

CONFIG_H := $(LOCAL_PATH)/include/config.h
LOCAL_CFLAGS := -DLOG_TAG=\"liballegro_decode\" -include $(CONFIG_H) -Wno-error=pointer-bool-conversion
LOCAL_CPPFLAGS += -fexceptions

LIB_COMMON_DEC_BASE_SRC := \
	lib_common_dec/DecBuffers.c

LIB_COMMON_DEC_SRC := \
	$(LIB_COMMON_DEC_BASE_SRC) \
	lib_common_dec/DecHwScalingList.c \
	lib_common_dec/DecInfo.c \
	lib_common_dec/RbspParser.c \

LIB_PARSING_SRC := \
	lib_parsing/common_syntax.c \
	lib_parsing/AvcParser.c \
	lib_parsing/HevcParser.c \
	lib_parsing/SliceHdrParsing.c \
	lib_parsing/DPB.c \
	lib_parsing/I_PictMngr.c \
	lib_parsing/Avc_PictMngr.c \
	lib_parsing/Hevc_PictMngr.c \
	lib_parsing/Concealment.c

LIB_DECODE_SRC := \
	lib_decode/DecChannelMcu.c \
	lib_decode/NalUnitParser.c \
	lib_decode/HevcDecoder.c \
	lib_decode/AvcDecoder.c \
	lib_decode/NalDecoder.c \
	lib_decode/FrameParam.c \
	lib_decode/SliceDataParsing.c \
	lib_decode/DefaultDecoder.c \
	lib_decode/lib_decode.c \
	lib_decode/BufferFeeder.c \
	lib_decode/Patchworker.c \
	lib_decode/DecoderFeeder.c

LOCAL_SRC_FILES := \
	$(LIB_RTOS_SRC) \
	$(LIB_COMMON_SRC) \
	$(LIB_COMMON_DEC_SRC) \
	$(LIB_FPGA_SRC) \
	$(LIB_PARSING_SRC) \
	$(LIB_DECODE_SRC)

LOCAL_MODULE := liballegro_decode
include $(BUILD_SHARED_LIBRARY)

#######################
# al_decoder
#######################
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := liblog libcutils liballegro_decode

LOCAL_C_INCLUDES := \
	system/core/include/ \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/extra/include

CONFIG_H := $(LOCAL_PATH)/include/config.h
LOCAL_CFLAGS := -DLOG_TAG=\"al_decoder\" -include $(CONFIG_H)
LOCAL_CPPFLAGS += -fexceptions

LOCAL_SRC_FILES := \
	exe_decoder/main.cpp \
	exe_decoder/crc.cpp \
	exe_decoder/IpDevice.cpp \
	exe_decoder/CodecUtils.cpp \
	exe_decoder/Conversion.cpp \
	$(LIB_APP_SRC)

LOCAL_MODULE := al_decoder
include $(BUILD_EXECUTABLE)

#######################
# al_encoder
#######################
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := liblog libcutils liballegro_encode

LOCAL_C_INCLUDES := \
	system/core/include/ \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/extra/include

CONFIG_H := $(LOCAL_PATH)/include/config.h
LOCAL_CFLAGS := -DLOG_TAG=\"al_encoder\" -include $(CONFIG_H)
LOCAL_CPPFLAGS += -fexceptions

LOCAL_SRC_FILES := \
	exe_encoder/CodecUtils.cpp \
	exe_encoder/IpDevice.cpp \
	exe_encoder/main.cpp \
	exe_encoder/sink_bitstream_writer.cpp \
	exe_encoder/sink_frame_writer.cpp \
	exe_encoder/sink_md5.cpp \
	exe_encoder/MD5.cpp \
	exe_encoder/ROIMngr.cpp \
	exe_encoder/EncCmdMngr.cpp \
	exe_encoder/QPGenerator.cpp \
	exe_encoder/CommandsSender.cpp\
	$(LIB_CFG_SRC) \
	$(LIB_CONV_SRC) \
	$(LIB_APP_SRC)

LOCAL_MODULE := al_encoder
include $(BUILD_EXECUTABLE)
endif
