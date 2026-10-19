/*
 * wasm_gbc.c
 *
 * Wrapper de binjgb (Game Boy + Game Boy Color, MIT) para WebAssembly.
 * Expone al JavaScript un puñado de funciones simples: cargar ROM, correr
 * un frame, leer pantalla/audio, botones y save RAM.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "binjgb/common.h"
#include "binjgb/emulator.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define AUDIO_FREQUENCY 44100
#define AUDIO_FRAMES 512 /* frames de audio por buffer (~11ms a 44.1kHz) */
/* Bytes máximos de audio que puede acumular un frame de vídeo: sobra. */
#define AUDIO_OUT_CAP (AUDIO_FRAMES * 2 * 8)

static Emulator *emu = NULL;
static uint8_t *rom_buf = NULL;
static size_t rom_size = 0;

static JoypadButtons buttons;

static uint8_t audio_out[AUDIO_OUT_CAP];
static size_t audio_out_len = 0;

static uint8_t *save_buf = NULL;
static size_t save_size = 0;

static void joypad_cb(JoypadButtons *joyp, void *user_data)
{
	(void)user_data;
	*joyp = buttons;
}

/* Reserva el buffer donde JS copia los bytes del .gb/.gbc. */
EMSCRIPTEN_KEEPALIVE
uint8_t *em_alloc_rom(uint32_t size)
{
	free(rom_buf);
	rom_buf = malloc(size);
	rom_size = size;
	return rom_buf;
}

/* 0 = OK, 1 = error al crear el emulador (ROM no válido/no soportado). */
EMSCRIPTEN_KEEPALIVE
int em_load_rom(void)
{
	if (emu) {
		emulator_delete(emu);
		emu = NULL;
	}

	EmulatorInit init;
	memset(&init, 0, sizeof(init));
	init.rom.data = rom_buf;
	init.rom.size = rom_size;
	init.audio_frequency = AUDIO_FREQUENCY;
	init.audio_frames = AUDIO_FRAMES;
	init.random_seed = 0xcabba6e5;
	init.builtin_palette = 0;
	init.force_dmg = FALSE;
	init.cgb_color_curve = CGB_COLOR_CURVE_SAMEBOY_EMULATE_HARDWARE;

	emu = emulator_new(&init);
	if (!emu)
		return 1;

	emulator_set_joypad_callback(emu, joypad_cb, NULL);

	/* Reserva el buffer de save RAM (puede ser 0 si el cartucho no tiene). */
	FileData fd;
	emulator_init_ext_ram_file_data(emu, &fd);
	save_size = fd.size;
	free(fd.data);
	free(save_buf);
	save_buf = save_size ? calloc(1, save_size) : NULL;

	return 0;
}

/* Corre exactamente un frame de vídeo (y acumula el audio que salga). */
EMSCRIPTEN_KEEPALIVE
void em_run_frame(void)
{
	if (!emu)
		return;

	audio_out_len = 0;
	Ticks target = emulator_get_ticks(emu) + PPU_FRAME_TICKS;

	for (;;) {
		EmulatorEvent ev = emulator_run_until(emu, target);

		if (ev & EMULATOR_EVENT_AUDIO_BUFFER_FULL) {
			AudioBuffer *ab = emulator_get_audio_buffer(emu);
			size_t n = (size_t)(ab->position - ab->data);
			if (audio_out_len + n <= AUDIO_OUT_CAP) {
				memcpy(audio_out + audio_out_len, ab->data, n);
				audio_out_len += n;
			}
		}
		if (ev & EMULATOR_EVENT_UNTIL_TICKS)
			break;
		if (ev & EMULATOR_EVENT_INVALID_OPCODE)
			break;
	}
}

/* Pantalla: 160x144 píxeles RGBA (bytes R,G,B,A), listo para ImageData. */
EMSCRIPTEN_KEEPALIVE
uint32_t *em_get_framebuffer(void)
{
	return emu ? (uint32_t *)*emulator_get_frame_buffer(emu) : NULL;
}

/* Audio: bytes sin signo, estéreo intercalado (L,R,L,R...), 44100 Hz. */
EMSCRIPTEN_KEEPALIVE
uint8_t *em_get_audio_buffer(void) { return audio_out; }

EMSCRIPTEN_KEEPALIVE
int em_get_audio_size(void) { return (int)audio_out_len; }

EMSCRIPTEN_KEEPALIVE
int em_get_audio_sample_rate(void) { return AUDIO_FREQUENCY; }

/* Botones: 0=A 1=B 2=SELECT 3=START 4=RIGHT 5=LEFT 6=UP 7=DOWN */
EMSCRIPTEN_KEEPALIVE
void em_set_button(int button, int pressed)
{
	Bool v = pressed ? TRUE : FALSE;
	switch (button) {
	case 0: buttons.A = v; break;
	case 1: buttons.B = v; break;
	case 2: buttons.select = v; break;
	case 3: buttons.start = v; break;
	case 4: buttons.right = v; break;
	case 5: buttons.left = v; break;
	case 6: buttons.up = v; break;
	case 7: buttons.down = v; break;
	default: break;
	}
}

/* --- Partidas guardadas (RAM con batería) --- */

EMSCRIPTEN_KEEPALIVE
uint32_t em_get_save_size(void) { return (uint32_t)save_size; }

/* Devuelve 1 si el juego ha escrito en la RAM del cartucho desde la última
 * vez que se preguntó (para guardar solo cuando hace falta). */
EMSCRIPTEN_KEEPALIVE
int em_save_updated(void)
{
	return emu && emulator_was_ext_ram_updated(emu) ? 1 : 0;
}

/* Copia la RAM actual a un buffer interno y devuelve su puntero. */
EMSCRIPTEN_KEEPALIVE
uint8_t *em_read_save(void)
{
	if (!emu || !save_buf)
		return NULL;
	FileData fd;
	fd.data = save_buf;
	fd.size = save_size;
	emulator_write_ext_ram(emu, &fd);
	return save_buf;
}

/* Reserva un buffer para que JS deje ahí una partida guardada. */
EMSCRIPTEN_KEEPALIVE
uint8_t *em_alloc_save(uint32_t size)
{
	free(save_buf);
	save_buf = calloc(1, size ? size : 1);
	save_size = size;
	return save_buf;
}

/* Carga en el emulador la partida que JS dejó en el buffer. */
EMSCRIPTEN_KEEPALIVE
int em_write_save(void)
{
	if (!emu || !save_buf)
		return 1;
	FileData fd;
	fd.data = save_buf;
	fd.size = save_size;
	return emulator_read_ext_ram(emu, &fd) == OK ? 0 : 1;
}
