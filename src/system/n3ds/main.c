// MIT License

// Copyright (c) 2020 Adrian "asie" Siekierka

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include <sys/stat.h>

#include <3ds.h>
#include <citro3d.h>

#include "system.h"
#include "net.h"

#include "keyboard.h"
#include "utils.h"
#include "net_httpc.h"

// #define RENDER_CONSOLE_TOP
#define RENDER_KEYBOARD

static struct
{
    Studio* studio;
    char* clipboard;
#ifndef DISABLE_NETWORKING
    Net* net;
#endif

    struct
    {
        u32* tic_buf;
        C3D_Tex tic_tex;
        C3D_Mtx proj_top, proj_bottom;
        C3D_RenderTarget *target_top, *target_bottom;

        DVLB_s* shader_dvlb;
        shaderProgram_s shader_program;
        int shader_proj_mtx_loc;
        C3D_AttrInfo shader_attr;

        int scaled;
    } render;

    struct
    {
        int samples, buffer_size, curr_block;
        u8* buffer;
        ndspWaveBuf ndspBuf[2];
    } audio;

    tic_n3ds_keyboard keyboard;
#ifdef ENABLE_HTTPC
    tic_n3ds_net httpc;
#endif
    LightLock tick_lock;
} platform;

static const char n3ds_shader_shbin[289] = {
	0x44, 0x56, 0x4c, 0x42, 0x01, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00,
	0x00, 0x44, 0x56, 0x4c, 0x50, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00,
	0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x08,
	0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x4e, 0x01, 0xf0, 0x07, 0x4e, 0x02, 0x10, 0x20, 0x4e, 0x01, 0xf0,
	0x27, 0x4e, 0x03, 0x08, 0x02, 0x08, 0x04, 0x18, 0x02, 0x08, 0x05,
	0x28, 0x02, 0x08, 0x06, 0x38, 0x02, 0x08, 0x07, 0x10, 0x20, 0x4c,
	0x07, 0x10, 0x41, 0x4c, 0x00, 0x00, 0x00, 0x88, 0x6e, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xa1, 0x0a, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x4e, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68,
	0xc3, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0xc3, 0x06, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x62, 0xc3, 0x06, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x61, 0xc3, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6f, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x56, 0x4c, 0x45, 0x02,
	0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x6c,
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00,
	0x0b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x5f, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,
	0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x01, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00,
	0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x13,
	0x00, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e,
	0x00, 0x00, 0x00
};

static const int n3ds_shader_shbin_length = 289;

static void setClipboardText(const char* text)
{
    if(platform.clipboard)
    {
        free(platform.clipboard);
        platform.clipboard = NULL;
    }
}

static bool hasClipboardText()
{
    return platform.clipboard != NULL;
}

static char* getClipboardText()
{
    return platform.clipboard ? strdup(platform.clipboard) : NULL;
}

static void freeClipboardText(const char* text)
{
    free((void*) text);
}

static u64 getPerformanceCounter()
{
    return svcGetSystemTick();
}

static u64 getPerformanceFrequency()
{
    return SYSCLOCK_ARM11;
}

static void* httpGetSync(const char* url, s32* size)
{
#ifndef DISABLE_NETWORKING
    return netGetSync(platform.net, url, size);
#else
#ifdef ENABLE_HTTPC
    return n3ds_net_get_sync(&platform.httpc, url, size);
#endif
#endif
}

static void httpGet(const char* url, HttpGetCallback callback, void* calldata)
{
#ifndef DISABLE_NETWORKING
    netGet(platform.net, url, callback, calldata);
#else
#ifdef ENABLE_HTTPC
    n3ds_net_get(&platform.httpc, url, callback, calldata);
#endif
#endif
}

static void n3ds_file_dialog_load(file_dialog_load_callback callback, void* data)
{
}

static void n3ds_file_dialog_save(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode)
{
}


static void goFullscreen()
{
}

static void showMessageBox(const char* title, const char* message)
{
}

static void setWindowTitle(const char* title)
{
}

static void openSystemPath(const char* path)
{

}

static void preseed()
{
    srand(osGetTime());
    rand();
}

static void pollEvent()
{

}

static void updateConfig()
{

}

static void n3ds_draw_init(void)
{
    C3D_TexEnv *texEnv;

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    platform.render.target_top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGB8, GPU_RB_DEPTH16);
    platform.render.target_bottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGB8, GPU_RB_DEPTH16);
    C3D_RenderTargetClear(platform.render.target_top, C3D_CLEAR_ALL, 0, 0);
    C3D_RenderTargetClear(platform.render.target_bottom, C3D_CLEAR_ALL, 0, 0);
    C3D_RenderTargetSetOutput(platform.render.target_top, GFX_TOP, GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
    C3D_RenderTargetSetOutput(platform.render.target_bottom, GFX_BOTTOM, GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));

    C3D_TexInitVRAM(&platform.render.tic_tex, 256, 256, GPU_RGBA8);
    platform.render.tic_buf = linearAlloc(256 * 256 * 4);

    Mtx_OrthoTilt(&platform.render.proj_top, 0.0, 400.0, 0.0, 240.0, -1.0, 1.0, true);
    Mtx_OrthoTilt(&platform.render.proj_bottom, 0.0, 320.0, 0.0, 240.0, -1.0, 1.0, true);

    texEnv = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(texEnv, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvOpRgb(texEnv, 0, 0, 0);
    C3D_TexEnvOpAlpha(texEnv, 0, 0, 0);
    C3D_TexEnvFunc(texEnv, C3D_Both, GPU_MODULATE);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);

    platform.render.shader_dvlb = DVLB_ParseFile((u32 *) n3ds_shader_shbin, n3ds_shader_shbin_length);
    shaderProgramInit(&(platform.render.shader_program));
    shaderProgramSetVsh(&(platform.render.shader_program), &(platform.render.shader_dvlb->DVLE[0]));
    platform.render.shader_proj_mtx_loc = shaderInstanceGetUniformLocation(platform.render.shader_program.vertexShader, "projection");
    AttrInfo_Init(&(platform.render.shader_attr));

    AttrInfo_AddLoader(&(platform.render.shader_attr), 0, GPU_FLOAT, 3); // v0 = position
    AttrInfo_AddLoader(&(platform.render.shader_attr), 1, GPU_FLOAT, 3); // v1 = texcoord

    C3D_BindProgram(&(platform.render.shader_program));
    C3D_SetAttrInfo(&(platform.render.shader_attr));
}

static void n3ds_draw_exit(void)
{
    linearFree(platform.render.tic_buf);
    C3D_TexDelete(&platform.render.tic_tex);

    C3D_RenderTargetDelete(platform.render.target_top);
    C3D_RenderTargetDelete(platform.render.target_bottom);

    C3D_Fini();
}

void n3ds_draw_texture(C3D_Tex* tex, int x, int y, int tx, int ty, int width, int height, int twidth, int theight,
float cmul) {
    float txmin, tymin, txmax, tymax;
    txmin = (float) tx / tex->width;
    tymax = (float) ty / tex->height;
    txmax = (float) (tx+twidth) / tex->width;
    tymin = (float) (ty+theight) / tex->height;

    C3D_TexBind(0, tex);
    C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);
        C3D_ImmSendAttrib((float) x, (float) 240 - y - height, 0.0f, 0.0f);
        C3D_ImmSendAttrib((float) txmin, (float) tymin, cmul, 0.0f);

        C3D_ImmSendAttrib((float) x + width, (float) 240 - y - height, 0.0f, 0.0f);
        C3D_ImmSendAttrib((float) txmax, (float) tymin, cmul, 0.0f);

        C3D_ImmSendAttrib((float) x, (float) 240 - y, 0.0f, 0.0f);
        C3D_ImmSendAttrib((float) txmin, (float) tymax, cmul, 0.0f);

        C3D_ImmSendAttrib((float) x + width, (float) 240 - y, 0.0f, 0.0f);
        C3D_ImmSendAttrib((float) txmax, (float) tymax, cmul, 0.0f);
    C3D_ImmDrawEnd();
}

static void n3ds_copy_frame(void)
{
    u32 *in = platform.studio->tic->screen;

/*    for (int y = 0; y < TIC80_FULLHEIGHT; y++) {
        out = platform.render.tic_buf + (y * 256);
        for (int x = 0; x < TIC80_FULLWIDTH; x++, in++, out++) {
            *out = __builtin_bswap32(*in);
        }
        memcpy(out, in, sizeof(u32) * TIC80_FULLWIDTH);
    } */

    GSPGPU_FlushDataCache(in, 256 * TIC80_FULLHEIGHT * 4);
    C3D_SyncDisplayTransfer(
        in, GX_BUFFER_DIM(256, 256),
        platform.render.tic_tex.data, GX_BUFFER_DIM(256, 256),
        (GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
    );
    GSPGPU_FlushDataCache(platform.render.tic_tex.data, 256 * 256 * 4);
}

static void n3ds_draw_frame(void)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

#ifdef RENDER_KEYBOARD
    if (platform.keyboard.render_dirty) {
        C3D_FrameDrawOn(platform.render.target_bottom);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, platform.render.shader_proj_mtx_loc, &platform.render.proj_bottom);

        n3ds_keyboard_draw(&platform.keyboard);
        platform.keyboard.render_dirty = false;
    }
#endif

#ifndef RENDER_CONSOLE_TOP
    C3D_FrameDrawOn(platform.render.target_top);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, platform.render.shader_proj_mtx_loc, &platform.render.proj_top);
    C3D_RenderTargetClear(platform.render.target_top, C3D_CLEAR_ALL, 0, 0);

    if (platform.render.scaled > 0) {
        float sw = 400.0f;
        float sh = TIC80_FULLHEIGHT * 400.0f / TIC80_FULLWIDTH;
        int isw = (int) sw;
        int ish = (int) sh;

        n3ds_draw_texture(&platform.render.tic_tex,
            (400 - isw) >> 1, (240 - ish) >> 1,
            0, 0,
            isw, ish,
            TIC80_FULLWIDTH, TIC80_FULLHEIGHT,
            1.0f);
    } else {
        n3ds_draw_texture(&platform.render.tic_tex,
            (400 - TIC80_FULLWIDTH) >> 1, (240 - TIC80_FULLHEIGHT) >> 1,
            0, 0,
            TIC80_FULLWIDTH, TIC80_FULLHEIGHT,
            TIC80_FULLWIDTH, TIC80_FULLHEIGHT,
            1.0f);
    }
#endif

    C3D_FrameEnd(0);
}

void n3ds_sound_init(int sample_rate)
{
    float mix[12];
    int buffer_size;

    platform.audio.samples = platform.studio->tic->samples.size / (TIC_STEREO_CHANNELS * sizeof(u16));
    buffer_size = platform.audio.samples * (TIC_STEREO_CHANNELS * sizeof(u16));

    platform.audio.buffer = linearAlloc(buffer_size * 2);
    platform.audio.buffer_size = buffer_size;
    memset(platform.audio.buffer, 0, buffer_size * 2);

    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, sample_rate);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    platform.audio.ndspBuf[0].data_vaddr = &platform.audio.buffer[0];
    platform.audio.ndspBuf[0].nsamples = platform.audio.samples;
    platform.audio.ndspBuf[1].data_vaddr = &platform.audio.buffer[buffer_size];
    platform.audio.ndspBuf[1].nsamples = platform.audio.samples;

    ndspChnWaveBufAdd(0, &platform.audio.ndspBuf[0]);
    ndspChnWaveBufAdd(0, &platform.audio.ndspBuf[1]);

    platform.audio.curr_block = 0;
}

void n3ds_sound_exit(void)
{
    ndspExit();
    linearFree(platform.audio.buffer);
}

static System systemInterface = 
{
    .setClipboardText = setClipboardText,
    .hasClipboardText = hasClipboardText,
    .getClipboardText = getClipboardText,
    .freeClipboardText = freeClipboardText,

    .getPerformanceCounter = getPerformanceCounter,
    .getPerformanceFrequency = getPerformanceFrequency,

    .httpGetSync = httpGetSync,
    .httpGet = httpGet,

    .fileDialogLoad = n3ds_file_dialog_load,
    .fileDialogSave = n3ds_file_dialog_save,

    .goFullscreen = goFullscreen,
    .showMessageBox = showMessageBox,
    .setWindowTitle = setWindowTitle,

    .openSystemPath = openSystemPath,
    .preseed = preseed,
    .poll = pollEvent,
    .updateConfig = updateConfig,
};

static void keyboard_update(void) {
    n3ds_keyboard_update(&platform.keyboard, platform.studio->tic, &platform.studio->text);

    u32 kup = hidKeysUp();
    if (kup & KEY_SELECT) {
        platform.render.scaled = (platform.render.scaled + 1) % 3;

        if (platform.render.scaled > 1) {
            C3D_TexSetFilter(&platform.render.tic_tex, GPU_LINEAR, GPU_LINEAR);
        } else {
            C3D_TexSetFilter(&platform.render.tic_tex, GPU_NEAREST, GPU_LINEAR);
        }
    }
}

int main(int argc, char **argv) {
    char *backup_argv[] = { "/3ds/tic80/tic80.3dsx", 0 };
    int argc_used = argc;
    char **argv_used = argv;

    if (argc_used <= 0 || argv_used[0] == NULL || argv_used[0][0] == '\0') {
        mkdir("/3ds/tic80", S_IRWXU | S_IRWXG | S_IRWXO);
        argc_used = 1;
        argv_used = backup_argv;
    }

    osSetSpeedupEnable(1);

    gfxInitDefault();
    gfxSet3D(false);
#ifdef RENDER_CONSOLE_TOP
    consoleInit(GFX_TOP, NULL);
#else
#ifndef RENDER_KEYBOARD
    consoleInit(GFX_BOTTOM, NULL);
#endif
#endif
    romfsInit();

    memset(&platform, 0, sizeof(platform));
    LightLock_Init(&platform.tick_lock);

#ifdef ENABLE_HTTPC
    n3ds_net_init(&platform.httpc, &platform.tick_lock);
#endif
    
    n3ds_draw_init();
    n3ds_keyboard_init(&platform.keyboard);

#ifndef DISABLE_NETWORKING
    platform.net = createNet();
#endif
    platform.studio = studioInit(argc_used, argv_used, 48000, "./", &systemInterface);

    n3ds_sound_init(48000);

//    u64 time_start = 0;
//    u64 time_end = 0;

    while (aptMainLoop() && !platform.studio->quit) {
        LightLock_Lock(&platform.tick_lock);
#ifndef DISABLE_NETWORKING
        netTick(platform.net);
#endif
        keyboard_update();

        platform.studio->tick();
//        printf("ticking time = %.2f us\n", (time_end - time_start) / CPU_TICKS_PER_USEC);

        // append audio data

        ndspWaveBuf *waveBuf = &platform.audio.ndspBuf[platform.audio.curr_block];
        if (waveBuf->status == NDSP_WBUF_DONE) {
            u16 *audio_ptr = waveBuf->data_pcm16;
            memcpy(audio_ptr, platform.studio->tic->samples.buffer, platform.audio.buffer_size);
            DSP_FlushDataCache(audio_ptr, platform.audio.buffer_size);

            ndspChnWaveBufAdd(0, waveBuf);
            platform.audio.curr_block ^= 1;
        }

        n3ds_copy_frame();
        LightLock_Unlock(&platform.tick_lock);

        n3ds_draw_frame();
    }

    n3ds_sound_exit();
    
    platform.studio->close();
#ifndef DISABLE_NETWORKING
    closeNet(platform.net);
#endif

    n3ds_keyboard_free(&platform.keyboard);
    n3ds_draw_exit();

#ifdef ENABLE_HTTPC
    n3ds_net_free(&platform.httpc);
#endif

    romfsExit();
    gfxExit();

    return 0;
};
