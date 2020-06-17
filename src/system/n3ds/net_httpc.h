#pragma once

#include "tic.h"
#include "ticapi.h"

#include <3ds.h>

typedef struct {
	LightLock *tick_lock;
} tic_n3ds_net;

void n3ds_net_init(tic_n3ds_net *net, LightLock *tick_lock);
void n3ds_net_free(tic_n3ds_net *net);
void n3ds_net_get(tic_n3ds_net *net, const char *url, HttpGetCallback callback, void *calldata);
void* n3ds_net_get_sync(tic_n3ds_net *net, const char *url, s32 *size);