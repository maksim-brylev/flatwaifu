/* Copyright (C) 2020 SovietPony
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License ONLY.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h> // srand exit
#include <string.h> // strcasecmp
#include <assert.h>
#include "system.h"
#include "input.h"

#include "player.h" // pl1 pl2
#include "menu.h" // G_keyf
#include "error.h" // logo
#include "monster.h" // nomon

#include "files.h" // F_startup F_addwad F_initwads F_allocres
#include "config.h" // CFG_args CFG_load CFG_save
#include "args.h" // ARG_parse
#include "memory.h" // M_startup
#include "game.h" // G_init G_act
#include "sound.h" // S_init S_done
#include "music.h" // S_initmusic S_updatemusic S_donemusic
#include "render.h" // R_init R_draw R_done

#include "common/cp866.h"

#define TITLE_STR "Doom 2D (SDL2)"

static Uint32 ticks;
static int quit = 0;
static SDL_Window *window;
static SDL_GLContext context;
static SDL_Surface *surf;
static videomode_t vlist;

static const cfg_t arg[] = {
  {"file", NULL, Y_FILES},
  {"cheat", &cheat, Y_SW_ON},
//  {"vga", &shot_vga, Y_SW_ON},
//  {"musvol", &mus_vol, Y_WORD},
  {"mon", &nomon, Y_SW_OFF},
  {"warp", &_warp, Y_BYTE},
//  {"config", NULL, cfg_file, Y_STRING},
  {NULL, NULL, 0} // end
};

static const cfg_t cfg[] = {
//  {"screenshot", &shot_vga, Y_SW_ON},
//  {"music_volume", &mus_vol, Y_WORD},
//  {"music_random", &music_random, Y_SW_ON},
//  {"music_time", &music_time, Y_DWORD},
//  {"music_fade", &music_fade, Y_DWORD},
  {"pl1_left", &pl1.kl, Y_KEY},
  {"pl1_right",&pl1.kr, Y_KEY},
  {"pl1_up", &pl1.ku, Y_KEY},
  {"pl1_down", &pl1.kd, Y_KEY},
  {"pl1_jump", &pl1.kj, Y_KEY},
  {"pl1_fire", &pl1.kf, Y_KEY},
  {"pl1_next", &pl1.kwr, Y_KEY},
  {"pl1_prev", &pl1.kwl, Y_KEY},
  {"pl1_use", &pl1.kp, Y_KEY},
  {"pl2_left", &pl2.kl, Y_KEY},
  {"pl2_right", &pl2.kr, Y_KEY},
  {"pl2_up", &pl2.ku, Y_KEY},
  {"pl2_down", &pl2.kd, Y_KEY},
  {"pl2_jump", &pl2.kj, Y_KEY},
  {"pl2_fire", &pl2.kf, Y_KEY},
  {"pl2_next", &pl2.kwr, Y_KEY},
  {"pl2_prev", &pl2.kwl, Y_KEY},
  {"pl2_use", &pl2.kp, Y_KEY},
  {NULL, NULL, 0} // end
};

static void CFG_args (int argc, char **argv) {
  const cfg_t *list[] = { arg, R_args(), S_args(), MUS_args() };
  ARG_parse(argc, argv, 4, list);
}

static void CFG_load (void) {
  const cfg_t *list[] = { cfg, R_conf(), S_conf(), MUS_conf() };
  CFG_read_config("default.cfg", 4, list);
  CFG_read_config("doom2d.cfg", 4, list);
}

static void CFG_save (void) {
  const cfg_t *list[] = { cfg, R_conf(), S_conf(), MUS_conf() };
  CFG_update_config("doom2d.cfg", "doom2d.cfg", 4, list, "generated by doom2d, do not modify");
}

/* --- error.h --- */

void logo (const char *s, ...) {
  va_list ap;
  va_start(ap, s);
  vprintf(s, ap);
  va_end(ap);
  fflush(stdout);
}

void logo_gas (int cur, int all) {
  // stub
}

void ERR_failinit (char *s, ...) {
  va_list ap;
  va_start(ap, s);
  vprintf(s, ap);
  va_end(ap);
  puts("");
  abort()
}

void ERR_fatal (char *s, ...) {
  va_list ap;
  R_done();
  MUS_done();
  S_done();
  SDL_Quit();
  puts("\nCRITICAL ERROR:");
  va_start(ap, s);
  vprintf(s, ap);
  va_end(ap);
  puts("");
  abort();
}

void ERR_quit (void) {
  quit = 1;
}

/* --- system.h --- */

static int Y_resize_window (int w, int h, int fullscreen) {
  assert(w > 0);
  assert(h > 0);
  assert(window != NULL);
  if (surf != NULL) {
    if (surf->w != w || surf->h != h) {
      SDL_Surface *s = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
      if (s != NULL) {
        SDL_SetPaletteColors(s->format->palette, surf->format->palette->colors, 0, surf->format->palette->ncolors);
        SDL_FreeSurface(surf);
        surf = s;
      }
    }
  }
  SDL_SetWindowSize(window, w, h);
  Y_set_fullscreen(fullscreen);
  return 1;
}

int Y_set_videomode_opengl (int w, int h, int fullscreen) {
  assert(w > 0);
  assert(h > 0);
  Uint32 flags;
  SDL_Window *win;
  SDL_GLContext ctx;
  if (window != NULL && context != NULL) {
    Y_resize_window(w, h, fullscreen);
    win = window;
  } else {
    flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
    if (fullscreen) {
      flags = flags | SDL_WINDOW_FULLSCREEN;
    }
    // TODO set context version and type
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    win = SDL_CreateWindow(TITLE_STR, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
    if (win != NULL) {
      ctx = SDL_GL_CreateContext(win);
      if (ctx != NULL) {
        Y_unset_videomode();
        window = win;
        context = ctx;
        SDL_GL_MakeCurrent(window, context);
      } else {
        SDL_DestroyWindow(win);
        win = NULL;
      }
    }
  }
  if (win == NULL) {
    logo("Y_set_videomode_opengl: error: %s\n", SDL_GetError());
  }
  return win != NULL;
}

int Y_set_videomode_software (int w, int h, int fullscreen) {
  assert(w > 0);
  assert(h > 0);
  Uint32 flags;
  SDL_Surface *s;
  SDL_Window *win;
  if (window != NULL && surf != NULL) {
    Y_resize_window(w, h, fullscreen);
    win = window;
  } else {
    flags = SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
      flags = flags | SDL_WINDOW_FULLSCREEN;
    }
    win = SDL_CreateWindow(TITLE_STR, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
    if (win != NULL) {
      s = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
      if (s != NULL) {
        Y_unset_videomode();
        window = win;
        surf = s;
      } else {
        SDL_DestroyWindow(win);
        win = NULL;
      }
    }
  }
  if (win == NULL) {
    logo("Y_set_videomode_software: error: %s\n", SDL_GetError());
  }
  return win != NULL;
}

void Y_get_videomode (int *w, int *h) {
  if (window != NULL) {
    SDL_GetWindowSize(window, w, h);
  } else {
    *w = 0;
    *h = 0;
  }
}

int Y_videomode_setted (void) {
  return window != NULL;
}

void Y_unset_videomode (void) {
  if (window != NULL) {
    if (context != NULL) {
      SDL_GL_MakeCurrent(window, NULL);
      SDL_GL_DeleteContext(context);
      context = NULL;
    }
    if (surf != NULL) {
      SDL_FreeSurface(surf);
      surf = NULL;
    }
    SDL_DestroyWindow(window);
    window = NULL;
  }
}

static void init_videomode_list (void) {
  int i, j, k;
  SDL_DisplayMode m;
  int n = SDL_GetNumDisplayModes(0);
  if (vlist.modes != NULL) {
    free(vlist.modes);
    vlist.modes = NULL;
    vlist.n = 0;
  }
  if (n > 0) {
    vlist.modes = malloc(n * sizeof(videomode_size_t));
    if (vlist.modes != NULL) {
      j = 0;
      for (i = 0; i < n; i++) {
        SDL_GetDisplayMode(0, i, &m);
        k = 0;
        while (k < j && (m.w != vlist.modes[k].w || m.h != vlist.modes[k].h)) {
          k++;
        }
        if (k >= j) {
          vlist.modes[j] = (videomode_size_t) {
            .w = m.w,
            .h = m.h
          };
          j++;
        }
      }
      vlist.n = j;
    }
  }
}

const videomode_t *Y_get_videomode_list_opengl (int fullscreen) {
  init_videomode_list();
  return &vlist;
}

const videomode_t *Y_get_videomode_list_software (int fullscreen) {
  init_videomode_list();
  return &vlist;
}

void Y_set_fullscreen (int yes) {
  if (window != NULL) {
    SDL_SetWindowFullscreen(window, yes ? SDL_WINDOW_FULLSCREEN : 0);
  }
}

int Y_get_fullscreen (void) {
  return (window != NULL) && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
}

void Y_swap_buffers (void) {
  assert(window != NULL);
  assert(context != NULL);
  SDL_GL_SwapWindow(window);
}

void Y_get_buffer (byte **buf, int *w, int *h, int *pitch) {
  assert(window != NULL);
  assert(surf != NULL);
  *buf = surf->pixels;
  *w = surf->w;
  *h = surf->h;
  *pitch = surf->pitch;
}

void Y_set_vga_palette (byte *vgapal) {
  assert(window != NULL);
  assert(surf != NULL);
  int i;
  byte *p = vgapal;
  SDL_Color colors[256];
  for (i = 0; i < 256; i++) {
    colors[i] = (SDL_Color) {
      .r = p[0] * 255 / 63,
      .g = p[1] * 255 / 63,
      .b = p[2] * 255 / 63
    };
    p += 3;
  }
  SDL_SetPaletteColors(surf->format->palette, colors, 0, 256);
}

void Y_repaint_rect (int x, int y, int w, int h) {
  assert(window != NULL);
  assert(surf != NULL);
  SDL_Surface *s = SDL_GetWindowSurface(window);
  SDL_Rect r = (SDL_Rect) {
    .x = x,
    .y = y,
    .w = w,
    .h = h
  };
  SDL_BlitSurface(surf, &r, s, &r);
  SDL_UpdateWindowSurfaceRects(window, &r, 1);
}

void Y_repaint (void) {
  Y_repaint_rect(0, 0, surf->w, surf->h);
}

void Y_enable_text_input (void) {
  SDL_StartTextInput();
}

void Y_disable_text_input (void) {
  SDL_StopTextInput();
}

/* --- main --- */

static int sdl_to_key (int code) {
  switch (code) {
    case SDL_SCANCODE_0: return KEY_0;
    case SDL_SCANCODE_1: return KEY_1;
    case SDL_SCANCODE_2: return KEY_2;
    case SDL_SCANCODE_3: return KEY_3;
    case SDL_SCANCODE_4: return KEY_4;
    case SDL_SCANCODE_5: return KEY_5;
    case SDL_SCANCODE_6: return KEY_6;
    case SDL_SCANCODE_7: return KEY_7;
    case SDL_SCANCODE_8: return KEY_8;
    case SDL_SCANCODE_9: return KEY_9;
    case SDL_SCANCODE_A: return KEY_A;
    case SDL_SCANCODE_B: return KEY_B;
    case SDL_SCANCODE_C: return KEY_C;
    case SDL_SCANCODE_D: return KEY_D;
    case SDL_SCANCODE_E: return KEY_E;
    case SDL_SCANCODE_F: return KEY_F;
    case SDL_SCANCODE_G: return KEY_G;
    case SDL_SCANCODE_H: return KEY_H;
    case SDL_SCANCODE_I: return KEY_I;
    case SDL_SCANCODE_J: return KEY_J;
    case SDL_SCANCODE_K: return KEY_K;
    case SDL_SCANCODE_L: return KEY_L;
    case SDL_SCANCODE_M: return KEY_M;
    case SDL_SCANCODE_N: return KEY_N;
    case SDL_SCANCODE_O: return KEY_O;
    case SDL_SCANCODE_P: return KEY_P;
    case SDL_SCANCODE_Q: return KEY_Q;
    case SDL_SCANCODE_R: return KEY_R;
    case SDL_SCANCODE_S: return KEY_S;
    case SDL_SCANCODE_T: return KEY_T;
    case SDL_SCANCODE_U: return KEY_U;
    case SDL_SCANCODE_V: return KEY_V;
    case SDL_SCANCODE_W: return KEY_W;
    case SDL_SCANCODE_X: return KEY_X;
    case SDL_SCANCODE_Y: return KEY_Y;
    case SDL_SCANCODE_Z: return KEY_Z;
    case SDL_SCANCODE_RETURN: return KEY_RETURN;
    case SDL_SCANCODE_ESCAPE: return KEY_ESCAPE;
    case SDL_SCANCODE_BACKSPACE: return KEY_BACKSPACE;
    case SDL_SCANCODE_TAB: return KEY_TAB;
    case SDL_SCANCODE_SPACE: return KEY_SPACE;
    case SDL_SCANCODE_MINUS: return KEY_MINUS;
    case SDL_SCANCODE_EQUALS: return KEY_EQUALS;
    case SDL_SCANCODE_LEFTBRACKET: return KEY_LEFTBRACKET;
    case SDL_SCANCODE_RIGHTBRACKET: return KEY_RIGHTBRACKET;
    case SDL_SCANCODE_BACKSLASH: return KEY_BACKSLASH;
    case SDL_SCANCODE_SEMICOLON: return KEY_SEMICOLON;
    case SDL_SCANCODE_APOSTROPHE: return KEY_APOSTROPHE;
    case SDL_SCANCODE_GRAVE: return KEY_GRAVE;
    case SDL_SCANCODE_COMMA: return KEY_COMMA;
    case SDL_SCANCODE_PERIOD: return KEY_PERIOD;
    case SDL_SCANCODE_SLASH: return KEY_SLASH;
    case SDL_SCANCODE_CAPSLOCK: return KEY_CAPSLOCK;
    case SDL_SCANCODE_F1: return KEY_F1;
    case SDL_SCANCODE_F2: return KEY_F2;
    case SDL_SCANCODE_F3: return KEY_F3;
    case SDL_SCANCODE_F4: return KEY_F4;
    case SDL_SCANCODE_F5: return KEY_F5;
    case SDL_SCANCODE_F6: return KEY_F6;
    case SDL_SCANCODE_F7: return KEY_F7;
    case SDL_SCANCODE_F8: return KEY_F8;
    case SDL_SCANCODE_F9: return KEY_F9;
    case SDL_SCANCODE_F10: return KEY_F10;
    case SDL_SCANCODE_F11: return KEY_F11;
    case SDL_SCANCODE_F12: return KEY_F12;
    case SDL_SCANCODE_PRINTSCREEN: return KEY_PRINTSCREEN;
    case SDL_SCANCODE_SCROLLLOCK: return KEY_SCROLLLOCK;
    case SDL_SCANCODE_PAUSE: return KEY_PAUSE;
    case SDL_SCANCODE_INSERT: return KEY_INSERT;
    case SDL_SCANCODE_HOME: return KEY_HOME;
    case SDL_SCANCODE_PAGEUP: return KEY_PAGEUP;
    case SDL_SCANCODE_DELETE: return KEY_DELETE;
    case SDL_SCANCODE_END: return KEY_END;
    case SDL_SCANCODE_PAGEDOWN: return KEY_PAGEDOWN;
    case SDL_SCANCODE_RIGHT: return KEY_RIGHT;
    case SDL_SCANCODE_LEFT: return KEY_LEFT;
    case SDL_SCANCODE_DOWN: return KEY_DOWN;
    case SDL_SCANCODE_UP: return KEY_UP;
    case SDL_SCANCODE_NUMLOCKCLEAR: return KEY_NUMLOCK;
    case SDL_SCANCODE_KP_DIVIDE: return KEY_KP_DIVIDE;
    case SDL_SCANCODE_KP_MULTIPLY: return KEY_KP_MULTIPLY;
    case SDL_SCANCODE_KP_MINUS: return KEY_KP_MINUS;
    case SDL_SCANCODE_KP_PLUS: return KEY_KP_PLUS;
    case SDL_SCANCODE_KP_ENTER: return KEY_KP_ENTER;
    case SDL_SCANCODE_KP_0: return KEY_KP_0;
    case SDL_SCANCODE_KP_1: return KEY_KP_1;
    case SDL_SCANCODE_KP_2: return KEY_KP_2;
    case SDL_SCANCODE_KP_3: return KEY_KP_3;
    case SDL_SCANCODE_KP_4: return KEY_KP_4;
    case SDL_SCANCODE_KP_5: return KEY_KP_5;
    case SDL_SCANCODE_KP_6: return KEY_KP_6;
    case SDL_SCANCODE_KP_7: return KEY_KP_7;
    case SDL_SCANCODE_KP_8: return KEY_KP_8;
    case SDL_SCANCODE_KP_9: return KEY_KP_9;
    case SDL_SCANCODE_KP_PERIOD: return KEY_KP_PERIOD;
    case SDL_SCANCODE_SYSREQ: return KEY_SYSREQ;
    case SDL_SCANCODE_LCTRL: return KEY_LCTRL;
    case SDL_SCANCODE_LSHIFT: return KEY_LSHIFT;
    case SDL_SCANCODE_LALT: return KEY_LALT;
    case SDL_SCANCODE_LGUI: return KEY_LSUPER;
    case SDL_SCANCODE_RCTRL: return KEY_RCTRL;
    case SDL_SCANCODE_RSHIFT: return KEY_RSHIFT;
    case SDL_SCANCODE_RALT: return KEY_RALT;
    case SDL_SCANCODE_RGUI: return KEY_RSUPER;
    default: return KEY_UNKNOWN;
  }
}

static void window_event_handler (SDL_WindowEvent *ev) {
  switch (ev->event) {
    case SDL_WINDOWEVENT_RESIZED:
      R_set_videomode(ev->data1, ev->data2, Y_get_fullscreen());
      break;
    case SDL_WINDOWEVENT_CLOSE:
      ERR_quit();
      break;
  }
}

static int utf8_to_wchar (char *x) {
  int i = 0;
  byte *s = (byte*)x;
  if (s[0] < 0x80) {
    return s[0];
  } else if (s[0] < 0xE0) {
    if (s[0] - 192 >= 0 && s[1] >= 0x80 && s[1] < 0xE0) {
      i = (s[0] - 192) * 64 + s[1] - 128;
    }
  } else if (s[0] < 0xF0) {
    if (s[1] >= 0x80 && s[1] < 0xE0 && s[2] >= 0x80 && s[2] < 0xE0) {
      i = ((s[0] - 224) * 64 + s[1] - 128) * 64 + s[2] - 128;
    }
  }
  return i;
}

static void poll_events (void) {
  int key, down, uch, ch;
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_QUIT:
        ERR_quit();
        break;
      case SDL_WINDOWEVENT:
        if (ev.window.windowID == SDL_GetWindowID(window)) {
          window_event_handler(&ev.window);
        }
        break;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        down = ev.type == SDL_KEYDOWN;
        key = sdl_to_key(ev.key.keysym.scancode);
        I_press(key, down);
        GM_key(key, down);
        break;
      case SDL_TEXTINPUT:
        uch = utf8_to_wchar(ev.text.text);
        ch = cp866_utoc(uch);
        if (ch >= 0) {
          GM_input(ch);
        }
        break;
    }
  }
}

static void step (void) {
  poll_events();
  MUS_update();
  Uint32 t = SDL_GetTicks();
  if (t - ticks > DELAY) {
    ticks = t;
    G_act();
  }
  R_draw();
}

int main (int argc, char **argv) {
  CFG_args(argc, argv);
  logo("system: initialize SDL2\n");
  if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS) == -1) {
    logo("system: failed to init SDL2: %s\n", SDL_GetError());
    return 1;
  }
  // Player 1 defaults
  pl1.ku = KEY_KP_8;
  pl1.kd = KEY_KP_5;
  pl1.kl = KEY_KP_4;
  pl1.kr = KEY_KP_6;
  pl1.kf = KEY_PAGEDOWN;
  pl1.kj = KEY_DELETE;
  pl1.kwl = KEY_HOME;
  pl1.kwr = KEY_END;
  pl1.kp = KEY_KP_8;
  // Player 2 defaults
  pl2.ku = KEY_E;
  pl2.kd = KEY_D;
  pl2.kl = KEY_S;
  pl2.kr = KEY_F;
  pl2.kf = KEY_A;
  pl2.kj = KEY_Q;
  pl2.kwl = KEY_1;
  pl2.kwr = KEY_2;
  pl2.kp = KEY_E;
  srand(SDL_GetTicks());
  CFG_load();
  F_addwad("doom2d.wad");
  F_initwads();
  S_init();
  MUS_init();
  R_init();
  G_init();
  ticks = SDL_GetTicks();
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(step, 0, 1);
#else
  while (!quit) {
    step();
  }
#endif
  CFG_save();
  R_done();
  MUS_done();
  S_done();
  SDL_Quit();
  return 0;
}
