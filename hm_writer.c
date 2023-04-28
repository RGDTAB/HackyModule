#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dr_wav.h"
#include "../stb_vorbis.h"
#include "raylib.h"

enum menu_types {
	NO_MODULE_MENU = 0,
	MODULE_EDITOR_MENU,
	MODULE_RECORDING_MENU,
	MODULE_GOTO_MENU,
	MODULE_SETTINGS_MENU,
	SAMPLE_EDITOR_MENU,
	OPEN_MODULE_MENU,
	OPEN_SAMPLE_MENU,
	SAVE_MODULE_MENU,
	SAVE_INSTRUMENT_MENU
};

struct chantick {
	struct chantick *left;
	struct chantick *right;
	struct chantick *up;
	struct chantick *down;

	uint8_t new_note;
	uint8_t note;
	uint8_t instrument;
	uint8_t command;
	uint8_t param;
};

struct sample {
	uint8_t id;
	uint8_t ogg;
	uint32_t data_length;

	uint32_t frame_count;
	uint32_t sample_rate;

	uint8_t sixteen_bit;
	uint8_t channels;

	uint8_t loop;
	uint32_t loop_start;

	uint16_t panning;
	uint16_t volume;

	uint8_t relative_note;
	uint8_t range_start;
	uint8_t range_end;

	uint8_t envelope;
	uint16_t predelay;
	uint16_t attack;
	uint16_t hold;
	uint16_t decay;
	uint16_t sustain;

	uint16_t fadeout;

	uint8_t *data;
};

void read_sample(const uint8_t *data, uint32_t *index, unsigned int i);
void read_tick(const uint8_t *data, uint32_t *index);
void open_module(const char *file_name);

void new_module(void);

void write_16(unsigned char *data, uint16_t num, uint32_t *index);
void write_32(unsigned char *data, uint32_t num, uint32_t *index);
void write_sample(unsigned char *data, struct sample *cur_sample,
	uint32_t *index);
void write_tick(unsigned char *data, struct chantick *tick, uint32_t *index);
void save_module(const unsigned char *file_name);

void load_wav_file(drwav *wav);
void load_ogg_file(stb_vorbis *ogg, stb_vorbis_info ogg_info);
uint16_t read_16(const uint8_t *data, uint32_t *index);
uint32_t read_32(const uint8_t *data, uint32_t *index);
void load_hacky_instrument(const uint8_t *data);
void open_sample(const char *file_name);
void save_instrument(const char *file_name, uint8_t instrument_index);

void save_file_dialogue(int type);
void open_file_dialogue(int type);
void close_file_dialogue(void);

void previous_file(void);
void next_file(void);
void select_file(void);
void save_file(void);

void open_sample_editor(void);
void delete_sample(void);
void close_sample_editor(void);

void start_recording(void);
void quit_recording(void);

void open_goto_menu(void);
void close_goto_menu(void);

void open_settings_menu(void);
void close_settings_menu(void);

void move_cursor_up(void);
void move_cursor_down(void);
void move_cursor_left(void);
void move_cursor_right(void);
void goto_tick(void);

void raise_command(int amount);
void lower_command(int amount);
void raise_instrument(void);
void lower_instrument(void);

void add_new_tick(void);
void delete_tick(void);

void add_new_channel(void);
void delete_channel(void);

void open_file_menu_process_input(void);
void sm_menu_process_input(void);
void si_menu_process_input(void);
void recording_menu_process_input(void);
void module_editor_process_input(void);
void goto_menu_process_input(void);
void settings_editor_process_input(void);
void sample_editor_process_input(void);
void no_module_menu_process_input(void);
void process_input(void);

void draw_chantick(struct chantick *tick, int x, int y);
void draw_module_editor(void);
void draw_sample_info(struct sample *cur_sample);
void draw_sample_editor(void);
void draw_goto_menu(void);
void draw_module_settings(void);

/* GLOBAL VARIABLES */

const uint8_t midi_keys[] = {
	KEY_Z,
	KEY_S,
	KEY_X,
	KEY_D,
	KEY_C,
	KEY_V,
	KEY_G,
	KEY_B,
	KEY_H,
	KEY_N,
	KEY_J,
	KEY_M
};

const struct sample EMPTY_SAMPLE = {
	.id = 0,
	.ogg = 0,
	.data_length = 0,
	.frame_count = 0,
	.sample_rate = 0,
	.sixteen_bit = 0,
	.channels = 0,
	.loop = 0,
	.loop_start = 0,
	.panning = 0,
	.volume = 0,
	.relative_note = 0,
	.range_start = 0,
	.range_end = 0,
	.envelope = 0,
	.predelay = 0,
	.attack = 0,
	.hold = 0,
	.decay = 0,
	.sustain = 0,
	.fadeout = 0,
	.data = NULL
};

const int screenWidth = 800;
const int screenHeight = 450;

unsigned int current_menu = NO_MODULE_MENU;
unsigned int file_index = 0;
FilePathList file_list = { 0 };

uint8_t sample_index = 0;
unsigned int sample_item = 0;
uint8_t sample_count = 0;
struct sample samples[256];

unsigned int bpm = 120;
unsigned int subdivision = 2;
char name[32];
uint8_t name_length = 0;
char save_file_name[34];
uint8_t save_file_name_length = 0;
uint16_t loop_tick = 0;

uint16_t tick = 0;
uint16_t tick_count = 0;
uint8_t channel = 0;
uint8_t channel_count = 0;
uint8_t instrument_index = 0;
uint8_t octave = 0;

unsigned int settings_index = 0;

struct chantick *cur_chantick = NULL;
unsigned int dest_tick;

void
read_sample(const uint8_t *data, uint32_t *index, unsigned int i)
{
	samples[i].id = data[(*index)++];
	samples[i].ogg = data[(*index)++];

	samples[i].data_length = read_32(data, index);
	samples[i].frame_count = read_32(data, index);
	samples[i].sample_rate = read_32(data, index);

	samples[i].sixteen_bit = data[(*index)++];
	samples[i].channels = data[(*index)++];
	samples[i].loop = data[(*index)++];
	samples[i].loop_start = read_32(data, index);

	samples[i].panning = read_16(data, index);
	samples[i].volume = read_16(data, index);

	samples[i].relative_note = data[(*index)++];
	samples[i].range_start = data[(*index)++];
	samples[i].range_end = data[(*index)++];
	samples[i].envelope = data[(*index)++];

	samples[i].predelay = read_16(data, index);
	samples[i].attack = read_16(data, index);
	samples[i].hold = read_16(data, index);
	samples[i].decay = read_16(data, index);
	samples[i].sustain = read_16(data, index);
	samples[i].fadeout = read_16(data, index);

	samples[i].data = malloc(samples[i].data_length);
	memcpy(samples[i].data, data + *index, samples[i].data_length);
	*index += samples[i].data_length;
}

void
read_tick(const uint8_t *data, uint32_t *index)
{
	struct chantick *previous_chantick = NULL, *new_chantick;
	int i;
	for (i = 0; i < channel_count; i++) {
		new_chantick = (struct chantick *)
			calloc(1, sizeof(struct chantick));
		if (previous_chantick) {
			previous_chantick->right = new_chantick;
		}
		new_chantick->left = previous_chantick;
		new_chantick->new_note = data[*index] >> 7;
		new_chantick->note = data[(*index)++] & 127;
		new_chantick->instrument = data[(*index)++];
		new_chantick->command = data[(*index)++];
		new_chantick->param = data[(*index)++];
		previous_chantick = new_chantick;
	}
	if (cur_chantick) {
		while (cur_chantick->left) {
			cur_chantick = cur_chantick->left;
		}
		while (new_chantick->left) {
			new_chantick = new_chantick->left;
		}
		while (1) {
			cur_chantick->down = new_chantick;
			new_chantick->up = cur_chantick;
			if (new_chantick->right) {
				new_chantick = new_chantick->right;
				cur_chantick = cur_chantick->right;
			} else {
				break;
			}
		}
	}
	cur_chantick = new_chantick;
}

void
open_module(const char *file_name)
{
	unsigned int i;
	unsigned int data_size;
	const uint8_t *data = LoadFileData(file_name, &data_size);
	uint32_t index = 14;

	TextCopy(name, data + 14);
	while (name[name_length]) {
		name_length++;
	}

	index += name_length + 1;
	channel_count = data[index++];
	sample_count = data[index++];
	bpm = data[index++];
	subdivision = data[index++];

	tick_count = read_16(data, &index);
	loop_tick = read_16(data, &index);

	for (i = 0; i < sample_count; i++) {
		read_sample(data, &index, i);
	}
	for (i = 0; i < tick_count; i++) {
		read_tick(data, &index);
	}

	while (cur_chantick->up) {
		cur_chantick = cur_chantick->up;
	}
	while (cur_chantick->left) {
		cur_chantick = cur_chantick->left;
	}
}

void
new_module(void)
{
	cur_chantick = calloc(1, sizeof(struct chantick));

	tick_count = 1;
	channel_count = 1;
	current_menu = MODULE_EDITOR_MENU;
}

void
write_16(unsigned char *data, uint16_t num, uint32_t *index)
{
	data[(*index)++] = num >> 8;
	data[(*index)++] = num & 255;
}

void
write_32(unsigned char *data, uint32_t num, uint32_t *index)
{
	data[(*index)++] = (num >> 24) & 255;
	data[(*index)++] = (num >> 16) & 255;
	data[(*index)++] = (num >> 8) & 255;
	data[(*index)++] = num & 255;
}

void
write_sample(unsigned char *data, struct sample *cur_sample, uint32_t *index)
{
	data[(*index)++] = cur_sample->id;
	data[(*index)++] = cur_sample->ogg;

	printf("INDEX: %d\n", *index);
	write_32(data, cur_sample->data_length, index);
	printf("INDEX: %d\n", *index);
	write_32(data, cur_sample->frame_count, index);
	printf("INDEX: %d\n", *index);
	write_32(data, cur_sample->sample_rate, index);
	printf("INDEX: %d\n", *index);

	data[(*index)++] = cur_sample->sixteen_bit;
	data[(*index)++] = cur_sample->channels;
	data[(*index)++] = cur_sample->loop;

	write_32(data, cur_sample->loop_start, index);

	write_16(data, cur_sample->panning, index);
	write_16(data, cur_sample->volume, index);

	data[(*index)++] = cur_sample->relative_note;
	data[(*index)++] = cur_sample->range_start;
	data[(*index)++] = cur_sample->range_end;
	data[(*index)++] = cur_sample->envelope;

	write_16(data, cur_sample->predelay, index);
	write_16(data, cur_sample->attack, index);
	write_16(data, cur_sample->hold, index);
	write_16(data, cur_sample->decay, index);
	write_16(data, cur_sample->sustain, index);
	write_16(data, cur_sample->fadeout, index);

	memcpy(data + *index, cur_sample->data, cur_sample->data_length);
	*index += cur_sample->data_length;
}

void
write_tick(unsigned char *data, struct chantick *tick, uint32_t *index)
{
	while (1) {
		data[*index] = tick->new_note << 7;
		data[(*index)++] |= tick->note;

		data[(*index)++] = tick->instrument;
		if (tick->command) {
			printf("COMMAND: %d at %d\n", tick->command, *index);
		}
		data[(*index)++] = tick->command;

		if (tick->command) {
			printf("PARAM: %d at %d\n", tick->param, *index);
		}
		data[(*index)++] = tick->param;

		if (tick->right)
			tick = tick->right;
		else
			break;
	}

}

void
save_module(const unsigned char *file_name)
{
	uint64_t i;
	uint64_t alloc_size = (23 + name_length)
		+ (41 * sample_count)
		+ (5 * tick_count * channel_count);
	uint32_t index = name_length + 14;
	unsigned char *data;
	struct chantick *work_chantick = cur_chantick;

	for (i = 0; i < sample_count; i++) {
		alloc_size += samples[i].data_length;
	}
	data = calloc(1, alloc_size);

	TextCopy(data, "Hacky Module: ");
	TextCopy(data + 14, name);
	data[index++] = '\0';
	printf("NAME INDEX: %d:\n", index);
	data[index++] = channel_count;
	data[index++] = sample_count;
	data[index++] = bpm;
	data[index++] = subdivision;

	write_16(data, tick_count, &index);
	write_16(data, loop_tick, &index);

	for (i = 0; i < sample_count; i++) {
		write_sample(data, &samples[i], &index);
	}

	while (work_chantick->up)
		work_chantick = work_chantick->up;
	while (work_chantick->left)
		work_chantick = work_chantick->left;

	for (i = 0; i < tick_count; i++) {
		write_tick(data, work_chantick, &index);
		work_chantick = work_chantick->down;
		if (work_chantick) {
			while (work_chantick->left) {
				work_chantick = work_chantick->left;
			}
		}
	}

	SaveFileData(file_name, data, alloc_size);
	free(data);
}

void
load_wav_file(drwav *wav)
{
	samples[sample_count].panning = 32767;
	samples[sample_count].volume = 65535;
	samples[sample_count].frame_count = wav->totalPCMFrameCount;
	samples[sample_count].sample_rate = wav->sampleRate;
	samples[sample_count].channels = wav->channels;
	samples[sample_count].sixteen_bit = wav->bitsPerSample == 16;
	samples[sample_count].data_length = wav->totalPCMFrameCount
		* wav->channels;
	if (samples[sample_count].sixteen_bit)
		samples[sample_count].data_length *= 2;
	samples[sample_count].data = malloc(samples[sample_count].data_length);
	drwav_read_pcm_frames(wav, wav->totalPCMFrameCount,
		samples[sample_count].data);
	sample_count++;
}

void
load_ogg_file(stb_vorbis *ogg, stb_vorbis_info ogg_info)
{
	samples[sample_count].ogg = 1;
	samples[sample_count].panning = 32767;
	samples[sample_count].volume = 65535;
	samples[sample_count].frame_count = stb_vorbis_stream_length_in_samples(ogg);
	samples[sample_count].channels = ogg_info.channels;
	samples[sample_count].sample_rate = ogg_info.sample_rate;
	samples[sample_count].sixteen_bit = 1;
}

uint16_t
read_16(const uint8_t *data, uint32_t *index)
{
	uint16_t r = data[(*index)++] << 8;
	r |= data[(*index)++];
	return r;
}

uint32_t
read_32(const uint8_t *data, uint32_t *index)
{
	uint32_t r = data[(*index)++] << 24;
	r |= data[(*index)++] << 16;
	r |= data[(*index)++] << 8;
	r |= data[(*index)++];
	return r;
}
void
load_hacky_instrument(const uint8_t *data)
{
	int i;
	uint32_t index = 1;
	unsigned int num_samples = data[0];
	struct sample *cur_sample;
	if (num_samples + sample_count > 255) {
		printf("COULDN'T LOAD: TOO MANY SAMPLES");
		return;
	}

	for (i = 0; i < num_samples; i++) {
		cur_sample = &samples[sample_count];
		cur_sample->ogg = data[index++];
		cur_sample->data_length = read_32(data, &index);
		cur_sample->frame_count = read_32(data, &index);
		cur_sample->sample_rate = read_32(data, &index);

		cur_sample->sixteen_bit = data[index++];
		cur_sample->channels = data[index++];
		cur_sample->loop = data[index++];

		cur_sample->panning = read_16(data, &index);
		cur_sample->volume = read_16(data, &index);

		cur_sample->loop = data[index++];

		cur_sample->predelay = read_16(data, &index);
		cur_sample->attack = read_16(data, &index);
		cur_sample->hold = read_16(data, &index);
		cur_sample->decay = read_16(data, &index);
		cur_sample->sustain = read_16(data, &index);

		cur_sample->fadeout = read_16(data, &index);

		cur_sample->data = malloc(cur_sample->data_length);
		memcpy(cur_sample->data, &data[index],
			cur_sample->data_length);
		index += cur_sample->data_length;
		sample_count++;
	}
}

void
open_sample(const char *file_name)
{
	const char *file_extension = GetFileExtension(file_name);
	unsigned int data_size;
	uint8_t *data = LoadFileData(file_name, &data_size);
	drwav wav = {0};
	stb_vorbis *ogg;
	stb_vorbis_info ogg_info;
	if (sample_count > 255) {
		printf("MAX SAMPLES REACHED\n");
		return;
	}
	if (!strcmp(file_extension, ".wav")) {
		drwav_init_memory(&wav, data, data_size, NULL);
		if (wav.bitsPerSample == 8 || wav.bitsPerSample == 16) {
			load_wav_file(&wav);
		} else {
			printf("INVALID WAVE FILE\n");
		}
		drwav_uninit(&wav);
	} else if (!strcmp(file_extension, ".ogg")) {
		ogg = stb_vorbis_open_memory(data, data_size, NULL, NULL);
		ogg_info = stb_vorbis_get_info(ogg);
		load_ogg_file(ogg, ogg_info);
		samples[sample_count].data_length = data_size;
		samples[sample_count].data = malloc(data_size);
		memcpy(samples[sample_count].data, data, data_size);
		sample_count++;
		stb_vorbis_close(ogg);
	} else if (!strcmp(file_extension, ".hi")) {
		load_hacky_instrument(data);
	} else {
		printf("FILE FORMAT NOT RECOGNIZED\n");
	}
	printf("SAMPLE_COUNT %d\n", sample_count);
	UnloadFileData(data);
}

// TODO: Finish
void
save_instrument(const char *file_name, uint8_t instrument_index)
{
	uint32_t alloc_size = 0, index = 1;
	unsigned int num_samples = 0;
	unsigned char *data;
	int i;
	for (i = 0; i < sample_count; i++) {
		if (samples[i].id == instrument_index) {
			alloc_size += samples[i].data_length;
			num_samples++;
		}
	}
	data = malloc(alloc_size);
	data[0] = num_samples;
	for (i = 0; i < sample_count; i++) {
		if (samples[i].id == instrument_index) {
			write_sample(data, &samples[i], &index);
		}
	}
	if (num_samples) {
	}
	free(data);
}

void
save_file_dialogue(int type)
{
	current_menu = type ? SAVE_INSTRUMENT_MENU : SAVE_MODULE_MENU;
}
void
open_file_dialogue(int type)
{
	if (type) {
		current_menu = OPEN_SAMPLE_MENU;
		file_list = LoadDirectoryFiles(GetApplicationDirectory());
	} else {
		current_menu = OPEN_MODULE_MENU;
		file_list = LoadDirectoryFilesEx(GetApplicationDirectory(),
			".hm", false);
	}
}

void
close_file_dialogue(void)
{
	if (current_menu == OPEN_SAMPLE_MENU
		|| current_menu == OPEN_MODULE_MENU)
		UnloadDirectoryFiles(file_list);
	if (cur_chantick)
		current_menu = MODULE_EDITOR_MENU;
	else
		current_menu = NO_MODULE_MENU;
	file_index = 0;
}

void
previous_file(void)
{
	if (file_index)
		file_index--;
	else
		file_index = file_list.count - 1;
}

void
next_file(void)
{
	file_index = (file_index + 1) % file_list.count;
}

void
select_file(void)
{
	if (current_menu == OPEN_SAMPLE_MENU) {
		open_sample(file_list.paths[file_index]);
	} else {
		open_module(file_list.paths[file_index]);
	}
	close_file_dialogue();
}

void
save_file(void)
{
	if (current_menu == SAVE_INSTRUMENT_MENU) {
		strcat(save_file_name, ".hi");
		save_instrument(save_file_name, instrument_index);
	} else {
		strcpy(save_file_name, name);
		strcat(save_file_name, ".hm");
		save_module(save_file_name);
	}
	close_file_dialogue();
}

void
open_sample_editor(void)
{
	current_menu = SAMPLE_EDITOR_MENU;
}

void
delete_sample(void)
{
	int i = sample_index;
	if (samples[sample_index].data) {
		free(samples[sample_index].data);
		sample_count--;
	}
	for (i = sample_index; i < 255; i++) {
		samples[i] = samples[i + 1];
	}
	samples[i] = EMPTY_SAMPLE;
}

void
close_sample_editor(void)
{
	current_menu = MODULE_EDITOR_MENU;
	sample_index = 0;
	sample_item = 0;
}

void
start_recording(void)
{
	current_menu = MODULE_RECORDING_MENU;
}

void
quit_recording(void)
{
	current_menu = MODULE_EDITOR_MENU;
}

void
open_goto_menu(void)
{
	current_menu = MODULE_GOTO_MENU;
	dest_tick = tick;
}

void
close_goto_menu(void)
{
	current_menu = MODULE_EDITOR_MENU;
}

void
open_settings_menu(void)
{
	current_menu = MODULE_SETTINGS_MENU;
	settings_index = 0;
}

void
close_settings_menu(void)
{
	current_menu = MODULE_EDITOR_MENU;
}

void
move_cursor_up(void)
{
	if (cur_chantick->up) {
//		printf("GOING UP\n");
		cur_chantick = cur_chantick->up;
		tick--;
	}
}

void
move_cursor_down(void)
{
	if (cur_chantick->down) {
//		printf("GOING DOWN\n");
		cur_chantick = cur_chantick->down;
		tick++;
	}
}

void
move_cursor_left(void)
{
	if (cur_chantick->left) {
//		printf("GOING LEFT\n");
		cur_chantick = cur_chantick->left;
		channel--;
	}
}

void
move_cursor_right(void)
{
	if (cur_chantick->right) {
//		printf("GOING RIGHT\n");
		cur_chantick = cur_chantick->right;
		channel++;
	}
}

void
goto_tick(void)
{
	int i;
	if (dest_tick < tick)
		for (i = tick; i > dest_tick; i--)
			move_cursor_up();
	else
		for (i = tick; i < dest_tick; i++)
			move_cursor_down();
	close_goto_menu();
}

void
raise_command(int amount)
{
	cur_chantick->command += amount;
}

void
lower_command(int amount)
{
	if (cur_chantick->command >= amount)
		cur_chantick->command -= amount;
	else
		cur_chantick->command = 0;
}

void
raise_instrument(void)
{
	instrument_index++;
}

void
lower_instrument(void)
{
	if (instrument_index)
		instrument_index--;
}

void
add_new_tick(void)
{
	struct chantick *work_chantick, *new_chantick;
	int i;

	work_chantick = cur_chantick;

	while (work_chantick->left)
		work_chantick = work_chantick->left;

	while (work_chantick) {
		new_chantick = calloc(1, sizeof(struct chantick));

		new_chantick->up = work_chantick;
		new_chantick->down = work_chantick->down;
		if (work_chantick->left) {
			new_chantick->left = work_chantick->left->down;
			new_chantick->left->right = new_chantick;
		} else {
			new_chantick->left = NULL;
		}
		if (work_chantick->down)
			work_chantick->down->up = new_chantick;

		work_chantick->down = new_chantick;
		work_chantick = work_chantick->right;
	}
	tick_count++;
}

void
delete_tick(void)
{
	struct chantick *work_chantick, *temp_chantick;
	if (tick_count == 1)
		return;

	work_chantick = cur_chantick;

	tick_count--;

	if (cur_chantick->down) {
		cur_chantick = cur_chantick->down;
	} else {
		cur_chantick = cur_chantick->up;
		tick--;
	}

	while (work_chantick->left)
		work_chantick = work_chantick->left;

	while (work_chantick) {
		if (work_chantick->up) {
			work_chantick->up->down = work_chantick->down;
		}
		if (work_chantick->down) {
			work_chantick->down->up = work_chantick->up;
		}
		temp_chantick = work_chantick;
		work_chantick = work_chantick->right;
		free(temp_chantick);
	}
}

void
add_new_channel(void)
{
	struct chantick *work_chantick, *temp_chantick;

	work_chantick = cur_chantick;

	while (work_chantick->up)
		work_chantick = work_chantick->up;

	while (work_chantick->right)
		work_chantick = work_chantick->right;

	while (work_chantick) {
		temp_chantick = calloc(1, sizeof(struct chantick));

		temp_chantick->left = work_chantick;
		work_chantick->right = temp_chantick;

		if (work_chantick->up) {
			temp_chantick->up = work_chantick->up->right;
			work_chantick->up->right->down = temp_chantick;
		}

		work_chantick = work_chantick->down;
	}

	channel_count++;
}

void
delete_channel(void)
{
	struct chantick *work_chantick, *temp_chantick;

	if (channel_count == 1)
		return;

	work_chantick = cur_chantick;
	if (cur_chantick->right) {
		cur_chantick = cur_chantick->right;
	} else {
		cur_chantick = cur_chantick->left;
		channel--;
	}

	while (work_chantick->up)
		work_chantick = work_chantick->up;

	while (work_chantick) {
		if (work_chantick->left)
			work_chantick->left->right = work_chantick->right;
		if (work_chantick->right)
			work_chantick->right->left = work_chantick->left;

		temp_chantick = work_chantick;
		work_chantick = work_chantick->down;
		free(temp_chantick);
	}

	channel_count--;
}

void
open_file_menu_process_input(void)
{
	if (IsKeyPressed(KEY_ENTER)) {
		select_file();
	}
	if (IsKeyPressed(KEY_ESCAPE)) {
		close_file_dialogue();
	}
	if (IsKeyPressed(KEY_UP)) {
		previous_file();
	}
	if (IsKeyPressed(KEY_DOWN)) {
		next_file();
	}
}

void
sm_menu_process_input(void)
{
	int i;
	for (i = 0; i < 26; i++) {
		if (IsKeyPressed(KEY_A + i) && name_length < 31) {
			name[name_length] = KEY_A + i;
			name[name_length++] += 32
				* IsKeyUp(KEY_LEFT_SHIFT);
			name[name_length] = '\0';
		}
	}
	if (IsKeyPressed(KEY_BACKSPACE) && name_length) {
		name_length--;
		name[name_length] = '\0';
	}
	if (IsKeyPressed(KEY_ESCAPE)) {
		close_file_dialogue();
	}
	if (IsKeyPressed(KEY_ENTER)) {
		save_file();
	}
}

void
si_menu_process_input(void)
{
	int i;
	for (i = 0; i < 26; i++) {
		if (IsKeyPressed(KEY_A + i) && name_length < 31) {
			save_file_name[save_file_name_length] = KEY_A + i;
			save_file_name[save_file_name_length++] += 32
				* IsKeyUp(KEY_LEFT_SHIFT);
			save_file_name[save_file_name_length] = '\0';
		}
	}
	if (IsKeyPressed(KEY_ESCAPE)) {
		close_file_dialogue();
	}
	if (IsKeyPressed(KEY_ENTER)) {
		save_file();
	}

	if (IsKeyPressed(KEY_DOWN)) {
		lower_instrument();
	}
	if (IsKeyPressed(KEY_UP)) {
		raise_instrument();
	}
}

void
recording_menu_process_input(void)
{
	uint32_t temp_int;
	int i;
	for (i = 0; i < 12; i++) {
		if (IsKeyPressed(midi_keys[i])) {
			cur_chantick->new_note = 1;
			cur_chantick->note = (i + 1) + (12 * octave);
			cur_chantick->instrument = instrument_index;
		}
	}
	for (i = 0; i < 8; i++) {
		if (IsKeyPressed(KEY_F1 + i)) {
			octave = i;
		}
	}
	for (i = 0; i < 10; i++) {
		if (IsKeyPressed(KEY_ZERO + i)) {
			temp_int = cur_chantick->param * 10 + i;
			if (temp_int < 256) {
				cur_chantick->param = temp_int;
			}
		}
	}

	if (IsKeyPressed(KEY_DOWN)) {
		move_cursor_down();
	}
	if (IsKeyPressed(KEY_UP)) {
		move_cursor_up();
	}
	if (IsKeyPressed(KEY_LEFT)) {
		move_cursor_left();
	}
	if (IsKeyPressed(KEY_RIGHT)) {
		move_cursor_right();
	}

	if (IsKeyPressed(KEY_Q)) {
		raise_command(1 + 9 * IsKeyDown(KEY_LEFT_SHIFT));
	}
	if (IsKeyPressed(KEY_W)) {
		lower_command(1 + 9 * IsKeyDown(KEY_LEFT_SHIFT));
	}
	if (IsKeyPressed(KEY_E)) {
		raise_instrument();
	}
	if (IsKeyPressed(KEY_R)) {
		lower_instrument();
	}

	if (IsKeyPressed(KEY_TAB)) {
		cur_chantick->note = 0;
		cur_chantick->new_note = 1;
	}
	if (IsKeyPressed(KEY_BACKSPACE)) {
		cur_chantick->new_note = 0;
		cur_chantick->note = 0;
	}
	if (IsKeyPressed(KEY_BACKSLASH)) {
		cur_chantick->param = 0;
	}

	if (IsKeyPressed(KEY_ENTER)) {
		add_new_tick();
	}
	if (IsKeyPressed(KEY_DELETE)) {
		delete_tick();
	}

	if (IsKeyPressed(KEY_ESCAPE)) {
		quit_recording();
	}
}

void
module_editor_process_input(void)
{
	if (IsKeyPressed(KEY_DOWN)) {
		move_cursor_down();
	}
	if (IsKeyPressed(KEY_UP)) {
		move_cursor_up();
	}
	if (IsKeyPressed(KEY_LEFT)) {
		move_cursor_left();
	}
	if (IsKeyPressed(KEY_RIGHT)) {
		move_cursor_right();
	}

	if (IsKeyPressed(KEY_G)) {
		open_goto_menu();
	}

	if (IsKeyPressed(KEY_U)) {
		open_file_dialogue(1);
	}
	if (IsKeyPressed(KEY_I)) {
		open_sample_editor();
	}
	if (IsKeyPressed(KEY_SPACE)) {
		start_recording();
	}
	if (IsKeyPressed(KEY_E)) {
		open_settings_menu();
	}

	if (IsKeyPressed(KEY_S) && IsKeyDown(KEY_LEFT_CONTROL)) {
		save_file_dialogue(0);
	}

	if (IsKeyPressed(KEY_ENTER)) {
		add_new_channel();
	}
	if (IsKeyPressed(KEY_DELETE)) {
		delete_channel();
	}
}

void
goto_menu_process_input(void)
{
	int i;
	unsigned int temp_int;
	for (i = 0; i < 10; i++) {
		if (IsKeyPressed(KEY_ZERO + i)) {
			temp_int = 10 * dest_tick + i;
			if (temp_int < tick_count) {
				dest_tick = temp_int;
			}
		}
	}
	if (IsKeyPressed(KEY_BACKSPACE)) {
		dest_tick /= 10;
	}

	if (IsKeyPressed(KEY_ENTER)) {
		goto_tick();
	}
	if (IsKeyPressed(KEY_ESCAPE)) {
		close_goto_menu();
	}
}

void
settings_editor_process_input(void)
{
	int i;
	unsigned int temp_int = 10;
	if (IsKeyPressed(KEY_DOWN)) {
		if (settings_index < 3) {
			settings_index++;
		}
	}
	if (IsKeyPressed(KEY_UP)) {
		if (settings_index) {
			settings_index--;
		}
	}
	for (i = 0; i < 10; i++) {
		if (IsKeyPressed(KEY_ZERO + i)) {
			temp_int = i;
		}
	}
	if (temp_int != 10) {
		switch (settings_index) {
		case 0: /* LOOP TICK */
			temp_int += 10 * loop_tick;
			if (temp_int < 65536) {
				loop_tick = temp_int;
			}
			break;
		case 1: /* BPM */
			temp_int += 10 * bpm;
			if (bpm < 255) {
				bpm = temp_int;
			}
			break;
		case 2: /* SUBDIVISION */
			temp_int += 10 * subdivision;
			if (temp_int < 255) {
				subdivision = temp_int;
			}
			break;
		}
	}
	if (IsKeyPressed(KEY_BACKSPACE)) {
		switch (settings_index) {
		case 0: /* LOOP TICK */
			loop_tick /= 10;
			break;
		case 1: /* BPM */
			bpm /= 10;
			break;
		case 2: /* SUBDIVISION */
			subdivision /= 10;
			break;
		}
	}
	if (IsKeyPressed(KEY_ESCAPE)) {
		close_settings_menu();
	}
}

void
sample_editor_process_input(void)
{
	int i;
	uint32_t temp_int = 10;
	struct sample *cur_sample = &samples[sample_index];
	if (IsKeyPressed(KEY_DOWN)) {
		if (sample_item < 14)
			sample_item++;
	}
	if (IsKeyPressed(KEY_UP)) {
		if (sample_item)
			sample_item--;
	}
	if (IsKeyPressed(KEY_LEFT)) {
		sample_index -= 1 + 9 * (IsKeyDown(KEY_LEFT_SHIFT));
	}
	if (IsKeyPressed(KEY_RIGHT)) {
		sample_index += 1 + 9 * (IsKeyDown(KEY_LEFT_SHIFT));
	}
	if (IsKeyPressed(KEY_ESCAPE)) {
		close_sample_editor();
	}

	for (i = 0; i < 10; i++) {
		if (IsKeyPressed(KEY_ZERO + i)) {
			temp_int = i;
		}
	}
	if (temp_int != 10) {
		switch (sample_item) {
		case 0: /* ID */
			temp_int += 10 * cur_sample->id;
			if (temp_int < 256) {
				cur_sample->id = temp_int;
			}
			break;
		case 1: /* LOOP */
			if (temp_int)
				cur_sample->loop = 1;
			else
				cur_sample->loop = 0;
			break;
		case 2: /* LOOP_START */
			temp_int += 10 * cur_sample->loop_start;
			if (temp_int < cur_sample->frame_count) {
				cur_sample->loop_start = temp_int;
			}
			break;
		case 3: /* PANNING */
			temp_int += 10 * cur_sample->panning;
			if (temp_int < 65536) {
				cur_sample->panning = temp_int;
			}
			break;
		case 4: /* VOLUME */
			temp_int += 10 * cur_sample->volume;
			if (temp_int < 65536) {
				cur_sample->volume = temp_int;
			}
			break;
		case 5: /* RELATIVE_NOTE */
			temp_int += 10 * cur_sample->relative_note;
			if (temp_int < 256) {
				cur_sample->relative_note = temp_int;
			}
			break;
		case 6: /* RANGE_START */
			temp_int += 10 * cur_sample->range_start;
			if (temp_int < 256) {
				cur_sample->range_start = temp_int;
			}
			break;
		case 7: /* RANGE_END */
			temp_int += 10 * cur_sample->range_end;
			if (temp_int < 256) {
				cur_sample->range_end = temp_int;
			}
			break;
		case 8: /* ENVELOPE */
			if (temp_int)
				cur_sample->envelope = 1;
			else
				cur_sample->envelope = 0;
			break;
		case 9: /* PREDELAY */
			temp_int += 10 * cur_sample->predelay;
			if (temp_int < 65536) {
				cur_sample->predelay = temp_int;
			}
			break;
		case 10: /* ATTACK */
			temp_int += 10 * cur_sample->attack;
			if (temp_int < 65536) {
				cur_sample->attack = temp_int;
			}
			break;
		case 11: /* HOLD */
			temp_int += 10 * cur_sample->hold;
			if (temp_int < 65536) {
				cur_sample->hold = temp_int;
			}
			break;
		case 12: /* DECAY */
			temp_int += 10 * cur_sample->decay;
			if (temp_int < 65536) {
				cur_sample->decay = temp_int;
			}
			break;
		case 13: /* SUSTAIN */
			temp_int += 10 * cur_sample->sustain;
			if (temp_int < 65536) {
				cur_sample->sustain = temp_int;
			}
			break;
		case 14: /* FADEOUT */
			temp_int += 10 * cur_sample->fadeout;
			if (temp_int < 65536) {
				cur_sample->fadeout = temp_int;
			}
			break;
		}
	}
	if (IsKeyPressed(KEY_BACKSPACE)) {
		switch (sample_item) {
		case 0: /* ID */
			cur_sample->id /= 10;
			break;
		case 1: /* LOOP */
			cur_sample->loop = 0;
			break;
		case 2: /* LOOP_START */
			cur_sample->loop_start /= 10;
			break;
		case 3: /* PANNING */
			cur_sample->panning /= 10;
			break;
		case 4: /* VOLUME */
			cur_sample->volume /= 10;
			break;
		case 5: /* RELATIVE_NOTE */
			cur_sample->relative_note /= 10;
			break;
		case 6: /* RANGE_START */
			cur_sample->range_start /= 10;
			break;
		case 7: /* RANGE_END */
			cur_sample->range_end /= 10;
			break;
		case 8: /* ENVELOPE */
			cur_sample->envelope = 0;
			break;
		case 9: /* PREDELAY */
			cur_sample->predelay /= 10;
			break;
		case 10: /* ATTACK */
			cur_sample->attack /= 10;
			break;
		case 11: /* HOLD */
			cur_sample->hold /= 10;
			break;
		case 12: /* DECAY */
			cur_sample->decay /= 10;
			break;
		case 13: /* SUSTAIN */
			cur_sample->sustain /= 10;
			break;
		case 14: /* FADEOUT */
			cur_sample->fadeout /= 10;
			break;
		}
	}
	if (IsKeyPressed(KEY_DELETE)) {
		delete_sample();
	}
}



void
no_module_menu_process_input(void)
{
	if (IsKeyPressed(KEY_O)) {
		open_file_dialogue(0);
	}
	if (IsKeyPressed(KEY_N)) {
		new_module();
	}
}


void
process_input(void)
{
	int i;
	uint32_t temp_int;
	switch (current_menu) {
	case OPEN_MODULE_MENU: /* FALLTHROUGH */
	case OPEN_SAMPLE_MENU:
		open_file_menu_process_input();
		break;
	case SAVE_MODULE_MENU:
		sm_menu_process_input();
		break;
	case SAVE_INSTRUMENT_MENU:
		si_menu_process_input();
		break;
	case SAMPLE_EDITOR_MENU:
		sample_editor_process_input();
		break;
	case MODULE_RECORDING_MENU:
		recording_menu_process_input();
		break;
	case NO_MODULE_MENU:
		no_module_menu_process_input();
		break;
	case MODULE_EDITOR_MENU:
		module_editor_process_input();
		break;
	case MODULE_GOTO_MENU:
		goto_menu_process_input();
		break;
	case MODULE_SETTINGS_MENU:
		settings_editor_process_input();
		break;
	}
}

void
draw_chantick(struct chantick *tick, int x, int y)
{
	char to_char_buffer[32];
	uint8_t note;

	DrawRectangleLines(x, y, 90, 40, BLACK);
	if (tick->note) {
		note = tick->note - 1;
		switch (note % 12) {
		case 0:
			snprintf(to_char_buffer, 32, "C%d %d", note /12,
				tick->instrument);
			break;
		case 1:
			snprintf(to_char_buffer, 32, "C#%d %d", note /12,
				tick->instrument);
			break;
		case 2:
			snprintf(to_char_buffer, 32, "D%d %d", note /12,
				tick->instrument);
			break;
		case 3:
			snprintf(to_char_buffer, 32, "D#%d %d", note /12,
				tick->instrument);
			break;
		case 4:
			snprintf(to_char_buffer, 32, "E%d %d", note /12,
				tick->instrument);
			break;
		case 5:
			snprintf(to_char_buffer, 32, "F%d %d", note /12,
				tick->instrument);
			break;
		case 6:
			snprintf(to_char_buffer, 32, "F#%d %d", note /12,
				tick->instrument);
			break;
		case 7:
			snprintf(to_char_buffer, 32, "G%d %d", note /12,
				tick->instrument);
			break;
		case 8:
			snprintf(to_char_buffer, 32, "G#%d %d", note /12,
				tick->instrument);
			break;
		case 9:
			snprintf(to_char_buffer, 32, "A%d %d", note /12,
				tick->instrument);
			break;
		case 10:
			snprintf(to_char_buffer, 32, "A#%d %d", note /12,
				tick->instrument);
			break;
		case 11:
			snprintf(to_char_buffer, 32, "B%d %d", note /12,
				tick->instrument);
			break;
		}
		DrawText(to_char_buffer, x + 5, y, 20, BLACK);
	} else if (tick->new_note) {
		DrawText("STOP", x + 5, y, 20, RED);
	}

	snprintf(to_char_buffer, 32, "%d: %d", tick->command,
		tick->param);
	DrawRectangleLines(x, y + 20, 90, 20, BLUE);
	DrawText(to_char_buffer, x + 5, y + 20, 20, BLUE);
}

void
draw_module_editor(void)
{
	struct chantick *work_chantick = cur_chantick;
	int work_channel = 0, work_tick = 0;
	char to_char_buffer[32];
	const int center_x = (screenWidth / 2) - 45;
	const int center_y = (screenHeight / 2) - 20;
	ClearBackground(GRAY);
	DrawRectangle(center_x, center_y, 90, 40, RAYWHITE);

	while (work_chantick->up)
		work_chantick = work_chantick->up;
	while (work_chantick->left)
		work_chantick = work_chantick->left;

	while (work_chantick) {
		while (true)  {
			draw_chantick(work_chantick,
				center_x + (90 * (work_channel - channel)),
				center_y + (40 * (work_tick - tick)));
			if (work_chantick->right) {
				work_chantick = work_chantick->right;
				work_channel++;
			} else {
				break;
			}
		}
		work_chantick = work_chantick->down;
		if (work_chantick) {
			work_tick++;
			work_channel = 0;
			while (work_chantick->left)
				work_chantick = work_chantick->left;
		}
	}

	snprintf(to_char_buffer, 32, "Tick: %d/%d",
		tick + 1, tick_count);
	DrawText(to_char_buffer, 10, 10, 20, BLACK);

	snprintf(to_char_buffer, 32, "Channel: %d/%d",
		channel + 1, channel_count);
	DrawText(to_char_buffer, 10, 30, 20, BLACK);
	snprintf(to_char_buffer, 32, "Instrument: %d",
		instrument_index);
	DrawText(to_char_buffer, 10, 50, 20, BLACK);
	snprintf(to_char_buffer, 32, "Octave: %d",
		octave);
	DrawText(to_char_buffer, 10, 70, 20, BLACK);

	if (current_menu == MODULE_RECORDING_MENU)
		DrawText("RECORDING", 10, 90, 20, RED);
}

void
draw_sample_info(struct sample *cur_sample)
{
	char to_char_buffer[32];

	snprintf(to_char_buffer, 32, "#%d", sample_index);
	DrawText(to_char_buffer, 40, 30, 20, BLACK);
	snprintf(to_char_buffer, 32, "Instrument ID: %d", cur_sample->id);
	DrawText(to_char_buffer, 40, 50, 20, BLACK);
	snprintf(to_char_buffer, 32, "Loop? %d", cur_sample->loop);
	DrawText(to_char_buffer, 40, 70, 20, BLACK);
	snprintf(to_char_buffer, 32, "Loop Start: %d", cur_sample->loop_start);
	DrawText(to_char_buffer, 40, 90, 20, BLACK);
	snprintf(to_char_buffer, 32, "Panning: %d", cur_sample->panning);
	DrawText(to_char_buffer, 40, 110, 20, BLACK);
	snprintf(to_char_buffer, 32, "Volume: %d", cur_sample->volume);
	DrawText(to_char_buffer, 40, 130, 20, BLACK);
	snprintf(to_char_buffer, 32, "Relative Note: %d",
		cur_sample->relative_note);
	DrawText(to_char_buffer, 40, 150, 20, BLACK);
	snprintf(to_char_buffer, 32, "Key Range Start: %d",
		cur_sample->range_start);
	DrawText(to_char_buffer, 40, 170, 20, BLACK);
	snprintf(to_char_buffer, 32, "Key Range End: %d",
		cur_sample->range_end);
	DrawText(to_char_buffer, 40, 190, 20, BLACK);
	snprintf(to_char_buffer, 32, "Envelope? %d", cur_sample->envelope);
	DrawText(to_char_buffer, 40, 210, 20, BLACK);
	snprintf(to_char_buffer, 32, "Predelay: %d", cur_sample->predelay);
	DrawText(to_char_buffer, 40, 230, 20, BLACK);
	snprintf(to_char_buffer, 32, "Attack: %d", cur_sample->attack);
	DrawText(to_char_buffer, 40, 250, 20, BLACK);
	snprintf(to_char_buffer, 32, "Hold: %d", cur_sample->hold);
	DrawText(to_char_buffer, 40, 270, 20, BLACK);
	snprintf(to_char_buffer, 32, "Decay: %d", cur_sample->decay);
	DrawText(to_char_buffer, 40, 290, 20, BLACK);
	snprintf(to_char_buffer, 32, "Sustain: %d", cur_sample->sustain);
	DrawText(to_char_buffer, 40, 310, 20, BLACK);
	snprintf(to_char_buffer, 32, "Fadeout: %d", cur_sample->fadeout);
	DrawText(to_char_buffer, 40, 330, 20, BLACK);

	snprintf(to_char_buffer, 32, "OGG? %d", cur_sample->ogg);
	DrawText(to_char_buffer, 300, 50, 20, BLACK);
	snprintf(to_char_buffer, 32, "Data Length: %d",
		cur_sample->data_length);
	DrawText(to_char_buffer, 300, 70, 20, BLACK);
	snprintf(to_char_buffer, 32, "Sample Count: %d",
		cur_sample->frame_count);
	DrawText(to_char_buffer, 300, 90, 20, BLACK);
	snprintf(to_char_buffer, 32, "Sample Rate: %d",
		cur_sample->sample_rate);
	DrawText(to_char_buffer, 300, 110, 20, BLACK);
	snprintf(to_char_buffer, 32, "Sixteen Bit? %d",
		cur_sample->sixteen_bit);
	DrawText(to_char_buffer, 300, 130, 20, BLACK);
	snprintf(to_char_buffer, 32, "Channels: %d", cur_sample->channels);
	DrawText(to_char_buffer, 300, 150, 20, BLACK);
}

void
draw_sample_editor(void)
{
	struct sample *cur_sample = &samples[sample_index];

	DrawText("Sample Editor", 10, 10, 20, BLACK);
	draw_sample_info(cur_sample);
	DrawText("-->", 10, 50 + sample_item * 20, 20, BLACK);
}

void
draw_goto_menu(void)
{
	char to_char_buffer[32];
	snprintf(to_char_buffer, 32, "GOTO: %d", dest_tick);
	DrawText(to_char_buffer, 10, 10, 20, BLACK);
	snprintf(to_char_buffer, 32, "Currently %d of %d", tick, tick_count);
	DrawText(to_char_buffer, 10, 30, 20, BLACK);
}

void
draw_module_settings(void)
{
	char to_char_buffer[32];

	DrawText("Module Settings:", 10, 10, 20, BLACK);
	snprintf(to_char_buffer, 32, "Loop Tick: %d", loop_tick);
	DrawText(to_char_buffer, 40, 30, 20, BLACK);
	snprintf(to_char_buffer, 32, "BPM: %d", bpm);
	DrawText(to_char_buffer, 40, 50, 20, BLACK);
	snprintf(to_char_buffer, 32, "Subdivision: %d", subdivision);
	DrawText(to_char_buffer, 40, 70, 20, BLACK);
	DrawText("-->", 10, 30 + settings_index * 20, 20, BLACK);

}

int
main()
{
	char to_char_buffer[32];

	InitWindow(screenWidth, screenHeight, "HM_WRITER");

	SetTargetFPS(60);
	SetExitKey(KEY_NULL);

	while (!WindowShouldClose())
	{
		process_input();
		BeginDrawing();

			ClearBackground(RAYWHITE);
			switch (current_menu) {
			case OPEN_MODULE_MENU: /* FALLTHROUGH */
			case OPEN_SAMPLE_MENU:
				DrawText(GetFileName(
					file_list.paths[file_index]),
					10, 10, 20, BLACK);
				snprintf(to_char_buffer, 32, "%d/%d",
					file_index + 1,file_list.count);
				DrawText(to_char_buffer, 10, 30, 20, BLACK);
				break;
			case SAVE_MODULE_MENU:
				DrawText("Save Module:" , 10, 10, 20, BLACK);
				DrawText(name, 10, 30, 20, BLACK);
				break;
			case SAVE_INSTRUMENT_MENU:
				snprintf(to_char_buffer, 32, "Instrument: %d",
					instrument_index);
				DrawText(to_char_buffer, 10, 30, 20, BLACK);
				DrawText(save_file_name, 10, 30, 20, BLACK);
				break;
			case SAMPLE_EDITOR_MENU:
				draw_sample_editor();
				break;
			case NO_MODULE_MENU:
				DrawText("Open Module (O)", 10, 10, 20, BLACK);
				DrawText("New Module (N)", 10, 30, 20, BLACK);
				break;
			case MODULE_GOTO_MENU:
				draw_goto_menu();
				break;
			case MODULE_SETTINGS_MENU:
				draw_module_settings();
				break;
			case MODULE_RECORDING_MENU: /* FALLTHROUGH */
			case MODULE_EDITOR_MENU:
				draw_module_editor();
				break;
			}

		EndDrawing();
	}

	CloseWindow();
	return 0;
}
