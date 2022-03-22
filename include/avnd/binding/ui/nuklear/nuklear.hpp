#pragma once

/* nuklear - 1.32.0 - public domain */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <time.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL4_IMPLEMENTATION
#define NK_KEYSTATE_BASED_INPUT
#include <nuklear.h>
#include "nuklear_glfw_gl4.h"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024




static inline int nk_tab (struct nk_context *ctx, const char *title, int active)
{
  const struct nk_user_font *f = ctx->style.font;
  float text_width = f->width(f->userdata, f->height, title, nk_strlen(title));
  float widget_width = text_width + 3 * ctx->style.button.padding.x;
  nk_layout_row_push(ctx, widget_width);
  struct nk_style_item c = ctx->style.button.normal;
  if (active) {ctx->style.button.normal = ctx->style.button.active;}
  int r = nk_button_label (ctx, title);
  ctx->style.button.normal = c;
  return r;
}
