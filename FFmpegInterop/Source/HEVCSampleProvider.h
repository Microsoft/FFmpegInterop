//*****************************************************************************
//
//	Copyright 2015 Microsoft Corporation
//
//	Licensed under the Apache License, Version 2.0 (the "License");
//	you may not use this file except in compliance with the License.
//	You may obtain a copy of the License at
//
//	http ://www.apache.org/licenses/LICENSE-2.0
//
//	Unless required by applicable law or agreed to in writing, software
//	distributed under the License is distributed on an "AS IS" BASIS,
//	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//	See the License for the specific language governing permissions and
//	limitations under the License.
//
//*****************************************************************************

#pragma once
#include "MediaSampleProvider.h"

namespace FFmpegInterop
{
	ref class HEVCSampleProvider :
		public MediaSampleProvider
	{
	public:
		virtual ~HEVCSampleProvider();

	private:
		HRESULT WriteNALPacket(DataWriter^ dataWriter, AVPacket* avPacket);
		HRESULT GetSPSAndPPSBuffer(DataWriter^ dataWriter);
		int ReadNALLength(byte* buffer, int position, int lenSize);
		bool m_bIsRawNalStream;
		bool m_bHasSentExtradata;
		int m_nalLenSize;

	internal:
		HEVCSampleProvider(
			FFmpegReader^ reader,
			AVFormatContext* avFormatCtx,
			AVCodecContext* avCodecCtx);
		virtual HRESULT WriteAVPacketToStream(DataWriter^ writer, AVPacket* avPacket) override;
	};
}
