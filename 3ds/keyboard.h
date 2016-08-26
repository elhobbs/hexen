#pragma once

#define	KEY_AUX1			207
#define	KEY_AUX2			208
#define	KEY_AUX3			209
#define	KEY_AUX4			210
#define	KEY_AUX5			211
#define	KEY_AUX6			212
#define	KEY_AUX7			213
#define	KEY_AUX8			214

#define KEY_TAB        9
#define KEY_CAPSLOCK   0xba                                        // phares
#define KEY_RCTRL      (0x80+0x1d)
#define KEY_RALT       (0x80+0x38)
#define KEY_RSHIFT     (0x80+0x36)

typedef struct {
	int x, y, type, dx, dy, key;
	char *text, *shift_text;
} sregion_t;

extern sregion_t key_button_array[];

void keyboard_draw();
void keyboard_input();
