/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include "flacDecoder.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <FLAC/stream_decoder.h>
#include "common/log.h"

using namespace std;


static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);


static msg::Header* flacHeader = NULL;
static msg::PcmChunk* flacChunk = NULL;
static msg::PcmChunk* pcmChunk = NULL;
static FLAC__StreamDecoder *decoder = NULL;


FlacDecoder::FlacDecoder() : Decoder()
{
	flacChunk = new msg::PcmChunk();
}


FlacDecoder::~FlacDecoder()
{
	delete flacChunk;
	delete decoder;
}


bool FlacDecoder::decode(msg::PcmChunk* chunk)
{
	pcmChunk = chunk;
//logO << "Decode start: " << chunk->payloadSize << endl;
	flacChunk->payload = (char*)realloc(flacChunk->payload, chunk->payloadSize);
	memcpy(flacChunk->payload, chunk->payload, chunk->payloadSize);
	flacChunk->payloadSize = chunk->payloadSize;

	pcmChunk->payload = (char*)realloc(pcmChunk->payload, 0);
	pcmChunk->payloadSize = 0;
	FLAC__stream_decoder_process_single(decoder);
	if (flacChunk->payloadSize > 0)
		FLAC__stream_decoder_process_single(decoder);
//logO << "Decode end\n" << endl;
	return true;
}


bool FlacDecoder::setHeader(msg::Header* chunk)
{
	flacHeader = chunk;
	FLAC__bool ok = true;
	FLAC__StreamDecoderInitStatus init_status;

	if((decoder = FLAC__stream_decoder_new()) == NULL) {
		fprintf(stderr, "ERROR: allocating decoder\n");
		return 1;
	}

//	(void)FLAC__stream_decoder_set_md5_checking(decoder, true);

	init_status = FLAC__stream_decoder_init_stream(decoder, read_callback, NULL, NULL, NULL, NULL, write_callback, metadata_callback, error_callback, this);

//	init_status = FLAC__stream_decoder_init_file(decoder, argv[1], write_callback, metadata_callback, error_callback, fout);
	if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		fprintf(stderr, "ERROR: initializing decoder: %s\n", FLAC__StreamDecoderInitStatusString[init_status]);
		ok = false;
	}
//	FLAC__stream_decoder_process_until_end_of_stream(decoder);
	FLAC__stream_decoder_process_until_end_of_metadata(decoder);

	return ok;
}


FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	if (flacHeader != NULL)
	{
		*bytes = flacHeader->payloadSize;
		memcpy(buffer, flacHeader->payload, *bytes);
		flacHeader = NULL;
	}
	else if (flacChunk != NULL)
	{
//logO << "Read: " << *bytes << "\t" << flacChunk->payloadSize << "\n";
		FLAC__StreamDecoderReadStatus result = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		if (*bytes > flacChunk->payloadSize)
			*bytes = flacChunk->payloadSize;
//		else
//			result = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

//		if (flacChunk->payloadSize == 0)
//			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

		memcpy(buffer, flacChunk->payload, *bytes);
		memmove(flacChunk->payload, flacChunk->payload + *bytes, flacChunk->payloadSize - *bytes);
		flacChunk->payloadSize = flacChunk->payloadSize - *bytes;
		flacChunk->payload = (char*)realloc(flacChunk->payload, flacChunk->payloadSize);
//logO << "Read end\n";
//		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		return result;
	}
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}


FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
//logO << "Write start\n";
	(void)decoder;

/*	if(channels != 2 || bps != 16) {
		fprintf(stderr, "ERROR: this example only supports 16bit stereo streams\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(frame->header.channels != 2) {
		fprintf(stderr, "ERROR: This frame contains %d channels (should be 2)\n", frame->header.channels);
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
*/	if(buffer [0] == NULL) {
		fprintf(stderr, "ERROR: buffer [0] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(buffer [1] == NULL) {
		fprintf(stderr, "ERROR: buffer [1] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (pcmChunk != NULL)
	{
		size_t bytes = frame->header.blocksize * 4;
//logO << "blocksize: " << frame->header.blocksize << "\tframe_number: " << frame->header.number.frame_number << "\tsample_number: " << frame->header.number.sample_number << "\n";
//logO << "Write: " << bytes << "\tpayloadSize: " << pcmChunk->payloadSize + bytes << "\n";
//flacChunk->payloadSize = 0;
		pcmChunk->payload = (char*)realloc(pcmChunk->payload, pcmChunk->payloadSize + bytes);

		for(size_t i = 0; i < frame->header.blocksize; i++) 
		{
			memcpy(pcmChunk->payload + pcmChunk->payloadSize + 4*i, (char*)(buffer[0] + i), 2);
			memcpy(pcmChunk->payload + pcmChunk->payloadSize + 4*i+2, (char*)(buffer[1] + i), 2);
		}
		pcmChunk->payloadSize += bytes;
//logO << "Write end: " << flacChunk->payloadSize << "\n";
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	(void)decoder, (void)client_data;

	/* print some stats */
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) 
	{
		logO << "sample rate    : " << metadata->data.stream_info.sample_rate << "Hz\n";
		logO << "channels       : " << metadata->data.stream_info.channels << "\n";
		logO << "bits per sample: " << metadata->data.stream_info.bits_per_sample << "\n";
	}
}


void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;
	fprintf(stderr, "Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}





