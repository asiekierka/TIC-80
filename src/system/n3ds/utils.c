#include <stdlib.h>

#include <3ds.h>
#include <citro3d.h>
#include <png.h>

#include "utils.h"

static u32 next_power_of_two(u32 v) {
	if (v < 8) v = 8;
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v + 1;
}

bool ctr_load_png(C3D_Tex* tex, const char* name, texture_location loc)
{
  png_structp png;
  png_infop png_info;
  png_bytep *png_rows;
  FILE *fp;
  int width, height, color_type, color_bpp, i, data_format, data_bpp;
  u32 *data;

  fp = fopen(name, "rb");
  if (!fp) return false;

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png) return false;

  png_info = png_create_info_struct(png);
  if(!png_info)
  {
    png_destroy_read_struct(&png, NULL, NULL);
    return false;
  }

  if(setjmp(png_jmpbuf(png)))
  {
    png_destroy_read_struct(&png, &png_info, NULL);
    return false;
  }

  png_init_io(png, fp);
  png_read_info(png, png_info);
  png_get_IHDR(png, png_info, &width, &height, &color_bpp, &color_type, NULL, NULL, NULL);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);
  if ((color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) && color_bpp < 8)
    png_set_expand_gray_1_2_4_to_8(png);
  if (png_get_valid(png, png_info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);
  else if (color_type == PNG_COLOR_TYPE_RGB)
    png_set_filler(png, 0xFF, PNG_FILLER_BEFORE);
  if (color_bpp == 16)
    png_set_strip_16(png);
  else if (color_bpp < 8)
    png_set_packing(png);
  if (color_type != PNG_COLOR_TYPE_GRAY && color_type != PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_bgr(png);
  if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
    png_set_swap_alpha(png);

  data_format = (color_type == PNG_COLOR_TYPE_GRAY ? GPU_L8 :
	  (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ? GPU_LA8 :
        GPU_RGBA8
      ));
  data_bpp = (color_type == PNG_COLOR_TYPE_GRAY ? 1 :
    (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ? 2 : 4)
  );
  if (loc == TEXTURE_TARGET_VRAM)
    C3D_TexInitVRAM(tex, next_power_of_two(width), next_power_of_two(height), data_format);
  else
    C3D_TexInit(tex, next_power_of_two(width), next_power_of_two(height), data_format);
  data = linearAlloc(tex->width * tex->height * data_bpp);
  png_rows = malloc(sizeof(png_bytep) * height);
  for (i = 0; i < height; i++) {
    png_rows[i] = (png_bytep) (((unsigned char *) data) + (tex->width * i * data_bpp));
  }
  png_read_image(png, png_rows);

  GSPGPU_FlushDataCache(data, tex->width * tex->height * data_bpp);

  C3D_SyncDisplayTransfer(data, GX_BUFFER_DIM(tex->width, tex->height),
    tex->data, GX_BUFFER_DIM(tex->width, tex->height),
    GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
    GX_TRANSFER_IN_FORMAT(data_format) | GX_TRANSFER_OUT_FORMAT(data_format)
    | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

  linearFree(data);
  free(png_rows);
  png_destroy_read_struct(&png, &png_info, NULL);

  return true;
}