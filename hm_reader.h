#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "stb_vorbis.h"

#define HM_MODULE_NAME_LENGTH 32
#define HM_MAX_CHANNELS 32

#define FREQUENCY_MULTIPLIER 0.05946f

struct hm_sample {
	uint8_t instrument_id;
	uint8_t ogg;
	uint32_t data_length;

	uint32_t frame_count;
	uint32_t sample_rate;

	uint8_t sixteen_bit;
	uint8_t channels;

	uint8_t loop;
	uint32_t loop_start;

	float pan;

	uint8_t relative_note;
	uint8_t key_range_start;
	uint8_t key_range_end;

	uint8_t envelope;
	uint32_t predelay;
	uint32_t attack;
	uint32_t hold;
	uint32_t decay;
	float sustain;
	uint32_t fadeout;
	uint64_t envelope_timer;

	float *frames;
};

struct hm_ramp {
	uint8_t enabled;
	int32_t start;
	int32_t end;
	uint32_t frame_pos;
	uint32_t frame_duration;
};

struct hm_trill {
	uint8_t enabled;
	int16_t depth;
	uint8_t up;
	int8_t result;
	uint32_t frame_pos;
	uint32_t frame_length;
};

struct hm_channel {
	uint16_t sample_id;
	uint8_t base_note; // Note being played
	uint8_t key_off; // Note being played

	uint8_t command_id;
	uint8_t command_param;

	int8_t coarse_detune;
	int8_t fine_detune;

	float pan;
	float vol;
	uint16_t predelay;

	int64_t sample_frame;
	double pos_between_samples; // For resampling and pitch shifting

	uint32_t fadeout_timer;

	struct hm_ramp ramps[4];
	struct hm_trill trills[2];
};

struct hm_context {
	uint8_t *data;

	char name[HM_MODULE_NAME_LENGTH];
	uint32_t rate;
	uint16_t length; // Measured in ticks
	uint16_t loop_position; // Measured in ticks

	uint8_t num_channels;
	uint16_t num_samples;

	uint8_t bpm;
	uint8_t subdivision; // Ticks in a beat
	uint32_t tick_length; // in samples

	int64_t tick_position;
	uint32_t samples_left_in_tick;
	struct hm_channel channels[HM_MAX_CHANNELS];
	struct hm_sample *samples;
};

static inline uint16_t hm_read_16(const uint8_t *data, uint32_t *i) {
	unsigned int a = data[(*i)++];
	unsigned int b = data[(*i)++];
	return a << 8 | b;
}

static inline uint32_t hm_read_32(const uint8_t *data, uint32_t *i) {
	unsigned int a = data[(*i)++];
	unsigned int b = data[(*i)++];
	unsigned int c = data[(*i)++];
	unsigned int d = data[(*i)++];
	return a << 24 | b << 16 | c << 8 | d;
}

static void
hm_load_samples(struct hm_context *ctx, const uint8_t *data,
	uint32_t data_length, uint32_t *index)
{
	const float envelope_multiplier = (float) ctx->rate / 1000.0f;
	int32_t temp_thirty_two;
	int i;
	uint32_t j;
	int16_t *sixteen_pointer;
	uint8_t *eight_pointer;
	float temp_float, vol;
	struct hm_sample *cur_sample;
	stb_vorbis *ogg;
	for (i = 0; i < ctx->num_samples; i++) {
		cur_sample = ctx->samples + i;
		cur_sample->instrument_id = data[(*index)++];
		cur_sample->ogg = data[(*index)++];

		cur_sample->data_length = hm_read_32(data, index);
		cur_sample->frame_count = hm_read_32(data, index);
		cur_sample->sample_rate = hm_read_32(data, index);

		cur_sample->sixteen_bit = data[(*index)++];
		cur_sample->channels = data[(*index)++];
		cur_sample->loop = data[(*index)++];

		cur_sample->loop_start = hm_read_32(data, index);

		temp_thirty_two = (((int32_t) hm_read_16(data, index)) - 32767);
		cur_sample->pan = ((float) temp_thirty_two) / 32767.0f;
		vol = ((float) hm_read_16(data, index)) / 65535.0f;

		cur_sample->relative_note = data[(*index)++];
		cur_sample->key_range_start = data[(*index)++];
		cur_sample->key_range_end = data[(*index)++];
		cur_sample->envelope = data[(*index)++];

		cur_sample->predelay = hm_read_16(data, index)
			* envelope_multiplier;
		cur_sample->attack = hm_read_16(data, index)
			* envelope_multiplier;
		cur_sample->attack += cur_sample->predelay;
		cur_sample->hold = hm_read_16(data, index)
			* envelope_multiplier;
		cur_sample->hold += cur_sample->attack;
		cur_sample->decay = hm_read_16(data, index)
			* envelope_multiplier;
		cur_sample->decay += cur_sample->hold;

		cur_sample->sustain = ((float) hm_read_16(data, index)) / 65535.0f;

		cur_sample->fadeout = hm_read_16(data, index)
			* envelope_multiplier;

		cur_sample->frames = malloc(cur_sample->frame_count
			* cur_sample->channels * sizeof(float));

		if (cur_sample->ogg) {
			ogg = stb_vorbis_open_memory(data + *index,
				cur_sample->data_length, NULL, NULL);
			stb_vorbis_get_samples_float_interleaved(ogg,
				cur_sample->channels, cur_sample->frames,
				cur_sample->frame_count
				* cur_sample->channels);
			stb_vorbis_close(ogg);
			if (cur_sample->channels == 2) {
				for (j = 0; j < cur_sample->frame_count; j++) {
					cur_sample->frames[j * 2] *= vol;
					cur_sample->frames[j * 2 + 1] *= vol;
				}
			} else {
				for (j = 0; j < cur_sample->frame_count; j++) {
					cur_sample->frames[j] *= vol;
				}
			}
		} else if (cur_sample->sixteen_bit) {
			sixteen_pointer = (int16_t *) (data + *index);
			if (cur_sample->channels == 2) {
				for (j = 0; j < cur_sample->frame_count; j++) {
					cur_sample->frames[j * 2]
						= ((float)
						*sixteen_pointer++)
						/ 32767.0f;
					cur_sample->frames[j * 2] *= vol;
					cur_sample->frames[j * 2 + 1]
						= ((float)
						*sixteen_pointer++)
						/ 32767.0f;
					cur_sample->frames[j * 2 + 1] *= vol;
				}
			} else {
				for (j = 0; j < cur_sample->frame_count; j++) {
					temp_float = (float) *sixteen_pointer++;
					cur_sample->frames[j] = temp_float
						/ 32767.0f;
					cur_sample->frames[j] *= vol;
				}
			}
		} else {
			eight_pointer = (uint8_t *) (data + *index);
			if (cur_sample->channels == 2) {
				for (j = 0; j < cur_sample->frame_count; j++) {
					temp_thirty_two = *eight_pointer++;
					temp_thirty_two -= 128;
					cur_sample->frames[j * 2]
						= ((float) temp_thirty_two)
						/ 128.0f;
					cur_sample->frames[j * 2] *= vol;

					temp_thirty_two = *eight_pointer++;
					temp_thirty_two -= 128;
					cur_sample->frames[j * 2 + 1]
						= ((float) temp_thirty_two)
						/ 128.0f;
					cur_sample->frames[j * 2 + 1] *= vol;
				}
			} else {
				for (j = 0; j < cur_sample->frame_count; j++) {
					temp_thirty_two = *eight_pointer++;
					temp_thirty_two -= 128;
					cur_sample->frames[j]
						= ((float) temp_thirty_two)
						/ 128.0f;
					cur_sample->frames[j] *= vol;
				}
			}
		}
		*index += cur_sample->data_length;
	}
}

int
hm_create_context(struct hm_context **ctxp, const void *data,
	uint32_t data_length, uint32_t rate)
{
	struct hm_context *ctx = NULL;
	const uint8_t *info = (uint8_t *) data;
	uint32_t i = 14;
	int j;
	uint8_t *mempool = calloc(1, sizeof(struct hm_context));
	ctx = (*ctxp = (struct hm_context*) mempool);

	ctx->rate = rate;
	while (info[i]) {
		ctx->name[i - 14] = info[i];
		i++;
	}
	i++;

	ctx->num_channels = info[i++];

	ctx->num_samples = info[i++];
	mempool = calloc(ctx->num_samples, sizeof(struct hm_sample));
	ctx->samples = (struct hm_sample*) mempool;

	ctx->bpm = info[i++];
	ctx->subdivision = info[i++];

	ctx->tick_length = ((ctx->rate * 60) / ctx->bpm) / ctx->subdivision;
	ctx->tick_position = -1;
	ctx->samples_left_in_tick = 0;

	ctx->length = hm_read_16(info, &i);
	ctx->loop_position = hm_read_16(info, &i);
	hm_load_samples(ctx, info, data_length, &i);

	mempool = malloc((data_length - i) * sizeof(uint8_t));
	memcpy(mempool, info + i, (data_length - i));
	ctx->data = mempool;
	for (j = 0; j < ctx->num_channels; j++) {
		ctx->channels[j].vol = 1.0f;
		ctx->channels[j].sample_frame = -1;
	}
	return 0;
}

static void
hm_identify_sample(struct hm_context *ctx, struct hm_channel *channel,
	uint8_t note, uint8_t instrument)
{
	int i;
	for (i = 0; i < ctx->num_samples; i++) {
		if (ctx->samples[i].instrument_id == instrument
			&& note >= ctx->samples[i].key_range_start
			&& note <= ctx->samples[i].key_range_end) {
			channel->sample_id = i;
			break;
		}
	}

	ctx->samples[channel->sample_id].envelope_timer = 0;
}

static void
hm_init_ramp(struct hm_ramp *ramp, uint32_t duration, int32_t start,
	int32_t end)
{
	ramp->enabled = 1;
	ramp->frame_pos = 0;
	ramp->frame_duration = duration;
	ramp->start = start;
	ramp->end = end;
}

static void
hm_process_command(struct hm_context *ctx, struct hm_channel *channel)
{
	int i;
	int32_t temp_32;
	struct hm_ramp *cur_ramp;
	switch (channel->command_id & 15) {
	case 1:
		if (channel->command_id >> 4) {
			hm_init_ramp(channel->ramps + 0,
				((channel->command_id >> 4) + 1)
				* ctx->tick_length,
				(int32_t) (channel->vol * 255.0f),
				channel->command_param);
		} else {
			channel->vol = ((float) channel->command_param)
				/ 255.0f;
			channel->ramps[0].enabled = 0;
		}
		break;
	case 2:
		if (channel->command_id >> 4) {
			hm_init_ramp(channel->ramps + 1,
				((channel->command_id >> 4) + 1)
				* ctx->tick_length,
				(int32_t) (channel->pan * 127.0f),
				((int32_t) channel->command_param) - 127);
		} else {
			temp_32 = ((int32_t) channel->command_param) - 127;
			channel->pan = ((float) temp_32) / 127.0f;
			channel->ramps[1].enabled = 0;
		}
		break;
	case 3:
		if (channel->command_id >> 4) {
			hm_init_ramp(channel->ramps + 2,
				((channel->command_id >> 4) + 1)
				* ctx->tick_length,
				channel->coarse_detune,
				((int32_t) channel->command_param) - 127);
		} else {
			channel->coarse_detune = ((int32_t) channel->command_param) - 127;
			channel->ramps[2].enabled = 0;
		}
		break;
	case 4:
		if (channel->command_id >> 4) {
			hm_init_ramp(channel->ramps + 3,
				((channel->command_id >> 4) + 1)
				* ctx->tick_length,
				channel->fine_detune,
				((int32_t) channel->command_param) - 127);
		} else {
			channel->fine_detune = ((int32_t) channel->command_param) - 127;
			channel->ramps[3].enabled = 0;
		}
		break;
	case 5:
		channel->predelay = channel->command_param;
		break;
	case 6:
		channel->trills[0].enabled = channel->command_id >> 4 & 1;
		channel->trills[0].depth = channel->command_param & 15;
		channel->trills[0].frame_length = (ctx->rate / 100)
			* (channel->command_param >> 4);
		channel->trills[0].frame_pos = channel->trills[0].frame_length;
		break;
	case 7:
		channel->trills[1].enabled = channel->command_id >> 4;
		channel->trills[1].depth = channel->command_param & 15 + 10;
		channel->trills[1].frame_length = (ctx->rate / 100)
			* (channel->command_param >> 4);
		channel->trills[1].frame_pos = channel->trills[1].frame_length;
		break;
	}
}

static void
hm_loop(struct hm_context *ctx)
{
	int i, j;
	ctx->tick_position = ctx->loop_position;
	for (i = 0; i < ctx->num_channels; i++)
		for (j = 0; j < 4; j++)
			ctx->channels[i].ramps[j].enabled = 0;
}

static void
hm_load_new_tick(struct hm_context *ctx)
{
	int i;
	uint8_t instrument_id;
	uint8_t requested_note;
	uint32_t data_index;
	struct hm_channel *channel;
	ctx->tick_position++;
	if (ctx->tick_position >= ctx->length)
		hm_loop(ctx);

	data_index = 4 * ctx->num_channels * ctx->tick_position;
	for (i = 0; i < ctx->num_channels; i++) {
		channel = ctx->channels + i;
		if (ctx->data[data_index] >> 7) {
			requested_note = ctx->data[data_index] & 127;

			instrument_id = ctx->data[data_index + 1];

			if (requested_note) {
				requested_note--;
				hm_identify_sample(ctx, channel,
					requested_note, instrument_id);
				channel->base_note = requested_note;

				channel->key_off = 0;

				channel->coarse_detune = 0;
				channel->fine_detune = 0;
				channel->predelay = 0;
				channel->fadeout_timer = 0;

				channel->sample_frame = 0;
				channel->pos_between_samples = 0;

				channel->trills[0].enabled = 0;
				channel->trills[1].enabled = 0;
			} else {
				channel->key_off = 1;
			}
		}
		data_index += 2;

		if (ctx->data[data_index]) {
			channel->command_id = ctx->data[data_index];
			channel->command_param = ctx->data[data_index + 1];
			hm_process_command(ctx, channel);
			channel->predelay *= ((float) ctx->rate) / 1000.0f;
		}
		data_index += 2;
	}

	ctx->samples_left_in_tick = ctx->tick_length;
}

static void
hm_pan_frame(float *left, float *right, float pan)
{
	if (pan < 0.0f) {
		*right -= *right * (-1.0f * pan);
	} else if (pan > 0.0f) {
		*left -= *left * pan;
	}
}

static void
hm_read_sample(struct hm_sample *sample, uint32_t number, float *left, float *right)
{
	float panned_sample;
	float envelope_multiplier = 1.0f;

	if (sample->channels == 2) {
		*left = sample->frames[number * 2];
		*right = sample->frames[number * 2 + 1];
	} else {
		*left = sample->frames[number];
		*right = *left;
	}

	hm_pan_frame(left, right, sample->pan);

	if (sample->envelope) {
		if (sample->envelope_timer < sample->predelay) {
			sample->envelope_timer++;
			envelope_multiplier = 0.0f;
		} else if (sample->envelope_timer < sample->attack) {
			envelope_multiplier = sample->envelope_timer
				/ ((float) sample->attack);
			sample->envelope_timer++;
		} else if (sample->envelope_timer < sample->hold) {
			envelope_multiplier = 1.0f;
			sample->envelope_timer++;
		} else if (sample->envelope_timer < sample->decay) {
			envelope_multiplier -=
				((float) sample->envelope_timer
				 / (float) sample->decay)
				* (1.0f - sample->sustain);
			sample->envelope_timer++;
		} else {
			envelope_multiplier = sample->sustain;
		}

		*left *= envelope_multiplier;
		*right *= envelope_multiplier;
	}
}

static void
hm_update_ramps(struct hm_context *ctx, struct hm_channel *channel)
{
	int i;
	struct hm_ramp *cur_ramp;
	int32_t ramp_val;
	float ramp_pos;
	for (i = 0; i < 4; i++) {
		if (channel->ramps[i].enabled) {
			cur_ramp = channel->ramps + i;
			ramp_pos = (float) cur_ramp->frame_pos
				/ (float) cur_ramp->frame_duration;
			ramp_val = cur_ramp->start + ramp_pos
				* (cur_ramp->end - cur_ramp->start);
			switch (i) {
			case 0:
				channel->vol = ((float) ramp_val) / 255.0f;
				break;
			case 1:
				channel->pan = (float) ramp_val / 127.0f;
				break;
			case 2:
				channel->coarse_detune = ramp_val;
				channel->fine_detune = 100
					* (ramp_pos - (uint32_t) ramp_pos);
				if (cur_ramp->end < 0)
					channel->fine_detune *= -1;
				break;
			case 3:
				channel->fine_detune = ramp_val;
				break;
			}
			cur_ramp->frame_pos++;
			if (cur_ramp->frame_pos >= cur_ramp->frame_duration)
				cur_ramp->enabled = 0;
		}
	}
}

static void
hm_update_trills(struct hm_context *ctx, struct hm_channel *channel)
{
	int i;
	struct hm_trill *cur_trill;
	int32_t trill_val;
	for (i = 0; i < 4; i++) {
		if (channel->trills[i].enabled) {
			cur_trill = channel->trills + i;
			cur_trill->frame_pos--;
			if (!cur_trill->frame_pos) {
				cur_trill->frame_pos = cur_trill->frame_length;
				cur_trill->up = !cur_trill->up;
			}
			switch (i) {
			case 0:
				cur_trill->result = cur_trill->up
					* cur_trill->depth;
				break;
			case 1:
				cur_trill->result = cur_trill->depth
					- (cur_trill->depth * 2
					* !cur_trill->up);
				break;
			}
		}
	}
}

static void
hm_channel_generate_sample(struct hm_context *ctx, struct hm_channel *channel,
	float *left, float *right)
{
	int i, dist;
	float l1 = 0, r1 = 0;
	float l2 = 0, r2 = 0;
	struct hm_sample *sample = &ctx->samples[channel->sample_id];
	double step_size = 1.0f;
	float panned_sample;

	if (channel->sample_frame < 0)
		return;

	if (channel->predelay > 0) {
		channel->predelay--;
		return;
	}
	hm_update_ramps(ctx, channel);
	hm_update_trills(ctx, channel);

	dist = (sample->relative_note)
		- (channel->base_note + channel->coarse_detune
		+ (channel->trills[0].result * channel->trills[0].enabled));
	if (dist < 0)
		for (i = 0; i > dist; i--)
			step_size *= 1 + FREQUENCY_MULTIPLIER;
	else if (dist > 0)
		for (i = 0; i < dist; i++)
			step_size *= 1 - FREQUENCY_MULTIPLIER;

	step_size *= 1 + ((channel->fine_detune
		+ (channel->trills[1].result * channel->trills[1].enabled))
		* (FREQUENCY_MULTIPLIER / 100.0f));

	step_size *=  ((double) sample->sample_rate / (double) ctx->rate);
	step_size += channel->pos_between_samples;

	channel->sample_frame += (uint32_t) step_size;
	step_size -= (uint32_t) step_size;
	channel->pos_between_samples = step_size;

	if ((channel->sample_frame + 1) < sample->frame_count) {
		hm_read_sample(sample, channel->sample_frame, &l1, &r1);
		hm_read_sample(sample, channel->sample_frame + 1, &l2, &r2);
	} else if (channel->sample_frame < sample->frame_count) {
		if (sample->loop) {
			hm_read_sample(sample, channel->sample_frame, &l1,
				&r1);
			hm_read_sample(sample, sample->loop_start, &l2, &r2);
		} else {
			hm_read_sample(sample, channel->sample_frame, &l1, &r1);
			l2 = r2 = 0.0f;
		}
	} else {
		if (sample->loop) {
			channel->sample_frame = (channel->sample_frame
				% sample->frame_count) + sample->loop_start;
			hm_read_sample(sample, channel->sample_frame, &l1,
				&r1);
			hm_read_sample(sample, channel->sample_frame + 1, &l2,
				&r2);
		} else {
			channel->sample_frame = -1;
			return;
		}
	}

	l1 += step_size * (l2 - l1);
	r1 += step_size * (r2 - r1);

	hm_pan_frame(&l1, &r1, channel->pan);

	l1 *= channel->vol;
	r1 *= channel->vol;

	if (channel->key_off) {
		channel->fadeout_timer++;
		if (channel->fadeout_timer > sample->fadeout) {
			channel->sample_frame = -1;
		} else {
			l1 += ((float) channel->fadeout_timer
				/ (float) sample->fadeout) * (0.0f - l1);
			r1 += ((float) channel->fadeout_timer
				/ (float) sample->fadeout) * (0.0f - r1);
		}
	}

	if (l1 > 1.0f)
		l1 = 1.0f;
	if (r1 > 1.0f)
		r1 = 1.0f;
	if (l1 < -1.0f)
		l1 = -1.0f;
	if (r1 < -1.0f)
		r1 = -1.0f;

	*left += l1;
	*right += r1;
}

void
hm_mixdown(struct hm_context *ctx, float *left, float *right)
{
	int i;
	if (ctx->samples_left_in_tick <= 0)
		hm_load_new_tick(ctx);
	ctx->samples_left_in_tick--;
	*left = *right = 0.0f;

	for (i = 0; i < ctx->num_channels; i++)
		hm_channel_generate_sample(ctx, ctx->channels + i, left, right);

	if (*left > 1.0f)
		*left = 1.0f;
	if (*right > 1.0f)
		*right = 1.0f;
	if (*left < -1.0f)
		*left = -1.0f;
	if (*right < -1.0f)
		*right = -1.0f;
}

void
hm_generate_samples(struct hm_context *ctx, float *buffer, uint64_t sample_count)
{
	uint64_t i;
	for (i = 0; i < sample_count; i++)
		hm_mixdown(ctx, &buffer[i * 2], &buffer[i * 2 + 1]);
}

void
hm_free_context(struct hm_context *ctx)
{
	int i;
	if (!ctx)
		return;
	free(ctx->data);
	for (i = 0; i < ctx->num_samples; i++)
		if (ctx->samples[i].frames)
			free(ctx->samples[i].frames);
	free(ctx->samples);
	free(ctx);
}
