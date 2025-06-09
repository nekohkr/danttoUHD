/* ------------------------------------------------------------------
 * Copyright (C) 2009 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#include "wavreader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>

#define TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

struct mem_stream {
	const uint8_t* data;
	size_t size;
	size_t pos;
};

static int mem_getc(struct mem_stream* ms) {
	if (ms->pos >= ms->size) return EOF;
	return ms->data[ms->pos++];
}

static size_t mem_read(void* ptr, size_t size, size_t count, struct mem_stream* ms) {
	size_t remain = ms->size - ms->pos;
	size_t total = size * count;
	if (total > remain) total = remain;
	memcpy(ptr, ms->data + ms->pos, total);
	ms->pos += total;
	return total / size;
}

static int mem_seek(struct mem_stream* ms, long offset, int origin) {
	size_t newpos = 0;
	if (origin == SEEK_SET) newpos = offset;
	else if (origin == SEEK_CUR) newpos = ms->pos + offset;
	else if (origin == SEEK_END) newpos = ms->size + offset;
	if (newpos > ms->size) return -1;
	ms->pos = newpos;
	return 0;
}

static long mem_tell(struct mem_stream* ms) {
	return (long)ms->pos;
}

static int mem_eof(struct mem_stream* ms) {
	return ms->pos >= ms->size;
}

struct wav_reader {
	struct mem_stream ms;
	uint32_t data_length;

	int format;
	int sample_rate;
	int bits_per_sample;
	int channels;
	int byte_rate;
	int block_align;

	int streamed;
};

static uint32_t read_tag(struct wav_reader* wr) {
	uint32_t tag = 0;
	tag = (tag << 8) | mem_getc(&wr->ms);
	tag = (tag << 8) | mem_getc(&wr->ms);
	tag = (tag << 8) | mem_getc(&wr->ms);
	tag = (tag << 8) | mem_getc(&wr->ms);
	return tag;
}

static uint32_t read_int32(struct wav_reader* wr) {
	uint32_t value = 0;
	value |= mem_getc(&wr->ms) << 0;
	value |= mem_getc(&wr->ms) << 8;
	value |= mem_getc(&wr->ms) << 16;
	value |= mem_getc(&wr->ms) << 24;
	return value;
}

static uint16_t read_int16(struct wav_reader* wr) {
	uint16_t value = 0;
	value |= mem_getc(&wr->ms) << 0;
	value |= mem_getc(&wr->ms) << 8;
	return value;
}

static void skip(mem_stream* ms, int n) {
	ms->pos += n;
	if (ms->pos > ms->size) ms->pos = ms->size;
}

void* wav_read_open(const std::vector<uint8_t>& input) {
	struct wav_reader* wr = (struct wav_reader*)malloc(sizeof(*wr));
	long data_pos = 0;
	memset(wr, 0, sizeof(*wr));
	wr->ms.data = input.data();
	wr->ms.size = input.size();
	wr->ms.pos = 0;

	while (1) {
		uint32_t tag, tag2, length;
		tag = read_tag(wr);
		if (mem_eof(&wr->ms))
			break;
		length = read_int32(wr);
		if (!length || length >= 0x7fff0000) {
			wr->streamed = 1;
			length = ~0;
		}
		if (tag != TAG('R', 'I', 'F', 'F') || length < 4) {
			skip(&wr->ms, length);
			continue;
		}
		tag2 = read_tag(wr);
		length -= 4;
		if (tag2 != TAG('W', 'A', 'V', 'E')) {
			skip(&wr->ms, length);
			continue;
		}
		// RIFF chunk found, iterate through it
		while (length >= 8) {
			uint32_t subtag, sublength;
			subtag = read_tag(wr);
			if (mem_eof(&wr->ms))
				break;
			sublength = read_int32(wr);
			length -= 8;
			if (length < sublength)
				break;
			if (subtag == TAG('f', 'm', 't', ' ')) {
				if (sublength < 16) break;
				wr->format = read_int16(wr);
				wr->channels = read_int16(wr);
				wr->sample_rate = read_int32(wr);
				wr->byte_rate = read_int32(wr);
				wr->block_align = read_int16(wr);
				wr->bits_per_sample = read_int16(wr);
				if (wr->format == 0xfffe) {
					if (sublength < 28) break;
					skip(&wr->ms, 8);
					wr->format = read_int32(wr);
					skip(&wr->ms, sublength - 28);
				}
				else {
					skip(&wr->ms, sublength - 16);
				}
			}
			else if (subtag == TAG('d', 'a', 't', 'a')) {
				data_pos = mem_tell(&wr->ms);
				wr->data_length = sublength;
				if (!wr->data_length || wr->streamed) {
					wr->streamed = 1;
					return wr;
				}
				skip(&wr->ms, sublength);
			}
			else {
				skip(&wr->ms, sublength);
			}
			length -= sublength;
		}
		if (length > 0) {
			skip(&wr->ms, length);
		}
	}
	mem_seek(&wr->ms, data_pos, SEEK_SET);
	return wr;
}

void wav_read_close(void* obj) {
	struct wav_reader* wr = (struct wav_reader*)obj;
	free(wr);
}

int wav_get_header(void* obj, int* format, int* channels, int* sample_rate, int* bits_per_sample, unsigned int* data_length) {
	struct wav_reader* wr = (struct wav_reader*)obj;
	if (format)
		*format = wr->format;
	if (channels)
		*channels = wr->channels;
	if (sample_rate)
		*sample_rate = wr->sample_rate;
	if (bits_per_sample)
		*bits_per_sample = wr->bits_per_sample;
	if (data_length)
		*data_length = wr->data_length;
	return wr->format && wr->sample_rate;
}

int wav_read_data(void* obj, unsigned char* data, unsigned int length) {
	struct wav_reader* wr = (struct wav_reader*)obj;
	if (wr == NULL)
		return -1;
	if (length > wr->data_length && !wr->streamed)
		length = wr->data_length;

	size_t n = mem_read(data, 1, length, &wr->ms);
	wr->data_length -= length;
	return (int)n;
}