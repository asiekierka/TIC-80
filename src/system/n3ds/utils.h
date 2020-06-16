#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <png.h>

typedef enum {
	TEXTURE_TARGET_RAM,
	TEXTURE_TARGET_VRAM
} texture_location;

bool ctr_load_png(C3D_Tex* tex, const char *name, texture_location loc);