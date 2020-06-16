#pragma once

#include "tic.h"
#include "ticapi.h"

#include <3ds.h>
#include <citro3d.h>

typedef struct {
	C3D_Tex tex;
	u8 kd[8];
	u32 kd_count;
} tic_n3ds_keyboard;

void n3ds_keyboard_init(tic_n3ds_keyboard *kbd);
void n3ds_keyboard_free(tic_n3ds_keyboard *kbd);
void n3ds_keyboard_draw(tic_n3ds_keyboard *kbd);
void n3ds_keyboard_update(tic_n3ds_keyboard *kbd, tic_mem *tic, char *chcode);