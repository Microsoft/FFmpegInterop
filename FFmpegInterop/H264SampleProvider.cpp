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

#include "pch.h"
#include "H264SampleProvider.h"

using namespace winrt::FFmpegInterop::implementation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

H264SampleProvider::H264SampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
	NALUSampleProvider(stream, reader)
{
	// Parse codec private data if present
	if (m_stream->codecpar->extradata != nullptr && m_stream->codecpar->extradata_size > 0)
	{
		// Check the H264 bitstream flavor
		if (m_stream->codecpar->extradata[0] == 1)
		{
			// avcC config format
			m_isBitstreamAnnexB = false;

			AVCConfigParser parser{ m_stream->codecpar->extradata, static_cast<uint32_t>(m_stream->codecpar->extradata_size) };
			m_naluLengthSize = parser.GetNaluLengthSize();
			tie(m_spsPpsData, m_spsPpsNaluLengths) = parser.GetSpsPpsData();
		}
		else
		{
			AnnexBParser parser{ m_stream->codecpar->extradata, static_cast<uint32_t>(m_stream->codecpar->extradata_size) };
			tie(m_spsPpsData, m_spsPpsNaluLengths) = parser.GetSpsPpsData();
		}
	}
}

void H264SampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
{
	NALUSampleProvider::SetEncodingProperties(encProp, setFormatUserData);

	if (!m_isBitstreamAnnexB)
	{
		AVCConfigParser parser{ m_stream->codecpar->extradata, static_cast<uint32_t>(m_stream->codecpar->extradata_size) };
		// TODO: Set MF_MT_VIDEO_H264_NO_FMOASO
	}
}

AVCConfigParser::AVCConfigParser(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize) :
	m_data(data),
	m_dataSize(dataSize)
{
	// Validate parameters
	WINRT_ASSERT(m_data != nullptr);
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, m_dataSize < MIN_SIZE);
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, m_data[0] != 1);
}

uint8_t AVCConfigParser::GetNaluLengthSize() const noexcept
{
	return m_data[4] & 0x03 + 1;
}

tuple<vector<uint8_t>, vector<uint32_t>> AVCConfigParser::GetSpsPpsData() const
{
	vector<uint8_t> spsPpsData;
	vector<uint32_t> spsPpsNaluLengths;
	uint32_t pos{ 5 };

	// Parse SPS NALUs
	uint8_t spsCount{ static_cast<uint8_t>(m_data[pos++] & 0x1F) };
	pos += ParseParameterSets(spsCount, m_data + pos, m_dataSize - pos, spsPpsData, spsPpsNaluLengths);

	// Parse PPS NALUs
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos >= m_dataSize);
	uint8_t ppsCount{ m_data[pos++] };
	ParseParameterSets(ppsCount, m_data + pos, m_dataSize - pos, spsPpsData, spsPpsNaluLengths);

	return { spsPpsData, spsPpsNaluLengths };
}

uint32_t AVCConfigParser::ParseParameterSets(
	_In_ uint8_t parameterSetCount,
	_In_reads_(dataSize) const uint8_t* data,
	_In_ uint32_t dataSize,
	_Inout_ vector<uint8_t>& spsPpsData,
	_Inout_ vector<uint32_t>& spsPpsNaluLengths) const
{
	// Reserve estimated space now to minimize reallocations
	spsPpsNaluLengths.reserve(spsPpsNaluLengths.size() + parameterSetCount);

	// Parse the parameter sets and convert the SPS/PPS data to Annex B format
	uint32_t pos{ 0 };
	for (uint8_t i{ 0 }; i < parameterSetCount; i++)
	{
		// Get the NALU length
		uint32_t naluLength{ GetAVCNaluLength(data + pos, dataSize - pos, sizeof(uint16_t)) };
		pos += sizeof(uint16_t);

		// Write the NALU start code and SPS/PPS data to the buffer
		spsPpsData.insert(spsPpsData.end(), begin(NALU_START_CODE), end(NALU_START_CODE));
		spsPpsData.insert(spsPpsData.end(), data + pos, data + pos + naluLength);

		spsPpsNaluLengths.push_back(sizeof(NALU_START_CODE) + naluLength);

		pos += naluLength;
	}

	return pos; // Return the number of bytes read
}

AVCSequenceParameterSetParser::AVCSequenceParameterSetParser(_In_reads_(dataSize) const uint8_t* data, _In_ int dataSize)
{
	THROW_HR_IF_NULL(MF_E_INVALID_FILE_FORMAT, data);
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, dataSize < 3);

	m_profile = data[1];
		
	uint8_t profileCompatibility{ data[2] };
	m_constraintSet1 = (profileCompatibility & (1 << 6)) != 0;
}

AVCPictureParamterSetParser::AVCPictureParamterSetParser(_In_reads_(dataSize) const uint8_t* data, _In_ int dataSize)
{
	// TODO: Implement
}
