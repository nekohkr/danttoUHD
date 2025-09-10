#include "aacEncoder.h"

int AacEncoder::encode(const std::vector<uint8_t>& input, std::vector<std::vector<uint8_t>>& output) {
	wav = wav_read_open(input);
	if (!inited) {
		if (!wav) {
			fprintf(stderr, "Unable to open wav file %s\n", infile);
			return 1;
		}
		if (!wav_get_header(wav, &format, &channels, &sample_rate, &bits_per_sample, NULL)) {
			fprintf(stderr, "Bad wav file %s\n", infile);
			return 1;
		}
		if (format != 1) {
			fprintf(stderr, "Unsupported WAV format %d\n", format);
			return 1;
		}
		if (bits_per_sample != 16) {
			fprintf(stderr, "Unsupported WAV sample depth %d\n", bits_per_sample);
			return 1;
		}
		if (aacEncOpen(&handle, 0, channels) != AACENC_OK) {
			fprintf(stderr, "Unable to open encoder\n");
			return 1;
		}
		if (aacEncoder_SetParam(handle, AACENC_AOT, aot) != AACENC_OK) {
			fprintf(stderr, "Unable to set the AOT\n");
			return 1;
		}
		if (aot == 39 && eld_sbr) {
			if (aacEncoder_SetParam(handle, AACENC_SBR_MODE, 1) != AACENC_OK) {
				fprintf(stderr, "Unable to set SBR mode for ELD\n");
				return 1;
			}
		}
		if (aacEncoder_SetParam(handle, AACENC_SAMPLERATE, sample_rate) != AACENC_OK) {
			fprintf(stderr, "Unable to set the sample rate\n");
			return 1;
		}
		if (aacEncoder_SetParam(handle, AACENC_CHANNELMODE, channels) != AACENC_OK) {
			fprintf(stderr, "Unable to set the channel mode\n");
			return 1;
		}
		if (aacEncoder_SetParam(handle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
			fprintf(stderr, "Unable to set the wav channel order\n");
			return 1;
		}
		if (vbr) {
			if (aacEncoder_SetParam(handle, AACENC_BITRATEMODE, vbr) != AACENC_OK) {
				fprintf(stderr, "Unable to set the VBR bitrate mode\n");
				return 1;
			}
		}
		else {
			if (aacEncoder_SetParam(handle, AACENC_BITRATE, bitrate * 2 * channels) != AACENC_OK) {
				fprintf(stderr, "Unable to set the bitrate\n");
				return 1;
			}
		}
		if (aacEncoder_SetParam(handle, AACENC_TRANSMUX, TT_MP4_ADTS) != AACENC_OK) {
			fprintf(stderr, "Unable to set the ADTS transmux\n");
			return 1;
		}
		if (aacEncoder_SetParam(handle, AACENC_AFTERBURNER, afterburner) != AACENC_OK) {
			fprintf(stderr, "Unable to set the afterburner mode\n");
			return 1;
		}
		if (aacEncEncode(handle, NULL, NULL, NULL, NULL) != AACENC_OK) {
			fprintf(stderr, "Unable to initialize the encoder\n");
			return 1;
		}
		if (aacEncInfo(handle, &info) != AACENC_OK) {
			fprintf(stderr, "Unable to get the encoder info\n");
			return 1;
		}
		inited = true;
	}

	input_size = channels * 2 * info.frameLength;
	input_buf = (uint8_t*)malloc(input_size);
	convert_buf = (int16_t*)malloc(input_size);

	while (1) {
		AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
		AACENC_InArgs in_args = { 0 };
		AACENC_OutArgs out_args = { 0 };
		int in_identifier = IN_AUDIO_DATA;
		int in_size, in_elem_size;
		int out_identifier = OUT_BITSTREAM_DATA;
		int out_size, out_elem_size;
		int read, i;
		void* in_ptr, * out_ptr;
		uint8_t outbuf[20480];
		AACENC_ERROR err;

		read = wav_read_data(wav, input_buf, input_size);
		for (i = 0; i < read / 2; i++) {
			const uint8_t* in = &input_buf[2 * i];
			convert_buf[i] = in[0] | (in[1] << 8);
		}
		in_ptr = convert_buf;
		in_size = read;
		in_elem_size = 2;

		in_args.numInSamples = read <= 0 ? -1 : read / 2;
		in_buf.numBufs = 1;
		in_buf.bufs = &in_ptr;
		in_buf.bufferIdentifiers = &in_identifier;
		in_buf.bufSizes = &in_size;
		in_buf.bufElSizes = &in_elem_size;

		out_ptr = outbuf;
		out_size = sizeof(outbuf);
		out_elem_size = 1;
		out_buf.numBufs = 1;
		out_buf.bufs = &out_ptr;
		out_buf.bufferIdentifiers = &out_identifier;
		out_buf.bufSizes = &out_size;
		out_buf.bufElSizes = &out_elem_size;

		if ((err = aacEncEncode(handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
			if (err == AACENC_ENCODE_EOF)
				break;
			fprintf(stderr, "Encoding failed\n");
			return 1;
		}
		if (out_args.numOutBytes == 0)
			continue;

		output.emplace_back(outbuf, outbuf + out_args.numOutBytes);
	}

	free(input_buf);
	free(convert_buf);

	return 0;
}

void AacEncoder::close() {
	wav_read_close(wav);
	aacEncClose(&handle);
}
