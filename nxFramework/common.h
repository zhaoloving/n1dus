#pragma once

#include <string.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <unistd.h>
#include <switch.h>
#include <threads.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL_ttf.h>

#include "SDL/SDLRender.h"
#include "SDL/SDLHelper.h"

#define wait(msec) svcSleepThread(10000000 * (s64)msec)

#define ASSERT_OK(rc_out, desc) if (R_FAILED(rc_out)) { char msg[256] = {0}; snprintf(msg, 256-1, "%s:%u: %s.  Error code: 0x%08x\n", __func__, __LINE__, desc, rc_out); throw std::runtime_error(msg); }

#ifdef DEBUG
#define LOG(f_, ...)            do {    printf("%s:%u: \t", __func__, __LINE__);                \
                                        printf((f_), ##__VA_ARGS__);                            \
                                    } while(0)

#define LOG_DEBUG(f_, ...)      do {    printf("%s:%u: \t", __func__, __LINE__);                \
                                        printf((f_), ##__VA_ARGS__);                            \
                                    } while(0)

#define LOG_FILENAME(f_, ...)   do {    printf("%s:%s:%u: \t", __FILE__, __func__, __LINE__);   \
                                        printf((f_), ##__VA_ARGS__);                            \
                                    } while(0)
#else
#define LOG(f_, ...) ;
#define LOG_DEBUG(f_, ...) ;
#define LOG_FILENAME(f_, ...) ;
#endif

#define ROOT_PATH "/"

// TODO: write a simple thread pool/factory mechanism
struct WorkerThread
{
    bool    running = false;
    thrd_t  thread;
};
static WorkerThread workerThread;

void printBytes(u8 *bytes, size_t size, bool includeHeader);
void displayOpenFiles();
int  openFileCount();

