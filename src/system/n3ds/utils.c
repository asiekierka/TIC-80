#include <stdlib.h>

#include <3ds.h>
#include <citro3d.h>
#include <png.h>

#include "utils.h"

static int npot(int v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

bool ctr_load_png(C3D_Tex* tex, const char* name, texture_location loc)
{
    png_image img;
    u32 *data;

    memset(&img, 0, sizeof(png_image));
    img.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_file(&img, name)) {
        return false;
    }
    
    img.format = PNG_FORMAT_ABGR;
    
    if (loc == TEXTURE_TARGET_VRAM) {
        C3D_TexInitVRAM(tex, npot(img.width), npot(img.height), GPU_RGBA8);
    } else {
        C3D_TexInit(tex, npot(img.width), npot(img.height), GPU_RGBA8);
    }
    data = linearAlloc(tex->width * tex->height * sizeof(u32));
    
    if (!png_image_finish_read(&img, NULL, data, tex->width * sizeof(u32), NULL)) {
        linearFree(data);
        C3D_TexDelete(tex);
        return false;
    }

    GSPGPU_FlushDataCache(data, tex->width * tex->height * sizeof(u32));

    C3D_SyncDisplayTransfer(data, GX_BUFFER_DIM(tex->width, tex->height),
        tex->data, GX_BUFFER_DIM(tex->width, tex->height),
        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GPU_RGBA8) | GX_TRANSFER_OUT_FORMAT(GPU_RGBA8)
        | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

    linearFree(data);
    png_image_free(&img);
    return true;
}