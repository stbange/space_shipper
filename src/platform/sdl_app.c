/* sdl_app.c - SDL3 + OpenGL platform layer: window, input, glyph blit.

   The game renders into an FrGrid; this layer uploads the grid as textured
   quads (one 8x8 atlas cell per character) and runs the fixed-timestep main
   loop. Works on Linux/Windows natively and on the web via emscripten.
*/
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../game/game.h"
#include "font.h"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <emscripten.h>
#else
/* Minimal GL 3.3 core definitions + entry points via SDL_GL_GetProcAddress,
 * avoiding dependence on system GL headers. */
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef int GLsizei;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef unsigned char GLboolean;
typedef char GLchar;

#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_STREAM_DRAW 0x88E0
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 0x0004
#define GL_FALSE 0x0
#define GL_TRUE 0x1
#define GL_TEXTURE0 0x84C0

typedef GLuint (*fn_glCreateShader)(GLenum);
typedef void (*fn_glShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
typedef void (*fn_glCompileShader)(GLuint);
typedef void (*fn_glGetShaderiv)(GLuint, GLenum, GLint *);
typedef void (*fn_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLuint (*fn_glCreateProgram)(void);
typedef void (*fn_glAttachShader)(GLuint, GLuint);
typedef void (*fn_glLinkProgram)(GLuint);
typedef void (*fn_glGetProgramiv)(GLuint, GLenum, GLint *);
typedef void (*fn_glUseProgram)(GLuint);
typedef GLint (*fn_glGetUniformLocation)(GLuint, const GLchar *);
typedef void (*fn_glUniform1i)(GLint, GLint);
typedef void (*fn_glUniform2f)(GLint, GLfloat, GLfloat);
typedef void (*fn_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef void (*fn_glEnableVertexAttribArray)(GLuint);
typedef void (*fn_glGenBuffers)(GLsizei, GLuint *);
typedef void (*fn_glBindBuffer)(GLenum, GLuint);
typedef void (*fn_glBufferData)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void (*fn_glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void *);
typedef void (*fn_glGenTextures)(GLsizei, GLuint *);
typedef void (*fn_glBindTexture)(GLenum, GLuint);
typedef void (*fn_glTexParameteri)(GLenum, GLenum, GLint);
typedef void (*fn_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                                const void *);
typedef void (*fn_glActiveTexture)(GLenum);
typedef void (*fn_glViewport)(GLint, GLint, GLsizei, GLsizei);
typedef void (*fn_glClear)(GLbitfield);
typedef void (*fn_glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*fn_glEnable)(GLenum);
typedef void (*fn_glBlendFunc)(GLenum, GLenum);
typedef void (*fn_glDrawArrays)(GLenum, GLint, GLsizei);
typedef void (*fn_glDeleteShader)(GLuint);

#define FR_GL_FN(name) static fn_##name p_##name;
FR_GL_FN(glCreateShader)
FR_GL_FN(glShaderSource)
FR_GL_FN(glCompileShader)
FR_GL_FN(glGetShaderiv)
FR_GL_FN(glGetShaderInfoLog)
FR_GL_FN(glCreateProgram)
FR_GL_FN(glAttachShader)
FR_GL_FN(glLinkProgram)
FR_GL_FN(glGetProgramiv)
FR_GL_FN(glUseProgram)
FR_GL_FN(glGetUniformLocation)
FR_GL_FN(glUniform1i)
FR_GL_FN(glUniform2f)
FR_GL_FN(glVertexAttribPointer)
FR_GL_FN(glEnableVertexAttribArray)
FR_GL_FN(glGenBuffers)
FR_GL_FN(glBindBuffer)
FR_GL_FN(glBufferData)
FR_GL_FN(glBufferSubData)
FR_GL_FN(glGenTextures)
FR_GL_FN(glBindTexture)
FR_GL_FN(glTexParameteri)
FR_GL_FN(glTexImage2D)
FR_GL_FN(glActiveTexture)
FR_GL_FN(glViewport)
FR_GL_FN(glClear)
FR_GL_FN(glClearColor)
FR_GL_FN(glEnable)
FR_GL_FN(glBlendFunc)
FR_GL_FN(glDrawArrays)
FR_GL_FN(glDeleteShader)
#undef FR_GL_FN

#define glCreateShader p_glCreateShader
#define glShaderSource p_glShaderSource
#define glCompileShader p_glCompileShader
#define glGetShaderiv p_glGetShaderiv
#define glGetShaderInfoLog p_glGetShaderInfoLog
#define glCreateProgram p_glCreateProgram
#define glAttachShader p_glAttachShader
#define glLinkProgram p_glLinkProgram
#define glGetProgramiv p_glGetProgramiv
#define glUseProgram p_glUseProgram
#define glGetUniformLocation p_glGetUniformLocation
#define glUniform1i p_glUniform1i
#define glUniform2f p_glUniform2f
#define glVertexAttribPointer p_glVertexAttribPointer
#define glEnableVertexAttribArray p_glEnableVertexAttribArray
#define glGenBuffers p_glGenBuffers
#define glBindBuffer p_glBindBuffer
#define glBufferData p_glBufferData
#define glBufferSubData p_glBufferSubData
#define glGenTextures p_glGenTextures
#define glBindTexture p_glBindTexture
#define glTexParameteri p_glTexParameteri
#define glTexImage2D p_glTexImage2D
#define glActiveTexture p_glActiveTexture
#define glViewport p_glViewport
#define glClear p_glClear
#define glClearColor p_glClearColor
#define glEnable p_glEnable
#define glBlendFunc p_glBlendFunc
#define glDrawArrays p_glDrawArrays
#define glDeleteShader p_glDeleteShader

static void load_gl_fns(void)
{
#define FR_GL_FN(name) p_##name = (fn_##name)SDL_GL_GetProcAddress(#name);
    FR_GL_FN(glCreateShader)
    FR_GL_FN(glShaderSource)
    FR_GL_FN(glCompileShader)
    FR_GL_FN(glGetShaderiv)
    FR_GL_FN(glGetShaderInfoLog)
    FR_GL_FN(glCreateProgram)
    FR_GL_FN(glAttachShader)
    FR_GL_FN(glLinkProgram)
    FR_GL_FN(glGetProgramiv)
    FR_GL_FN(glUseProgram)
    FR_GL_FN(glGetUniformLocation)
    FR_GL_FN(glUniform1i)
    FR_GL_FN(glUniform2f)
    FR_GL_FN(glVertexAttribPointer)
    FR_GL_FN(glEnableVertexAttribArray)
    FR_GL_FN(glGenBuffers)
    FR_GL_FN(glBindBuffer)
    FR_GL_FN(glBufferData)
    FR_GL_FN(glBufferSubData)
    FR_GL_FN(glGenTextures)
    FR_GL_FN(glBindTexture)
    FR_GL_FN(glTexParameteri)
    FR_GL_FN(glTexImage2D)
    FR_GL_FN(glActiveTexture)
    FR_GL_FN(glViewport)
    FR_GL_FN(glClear)
    FR_GL_FN(glClearColor)
    FR_GL_FN(glEnable)
    FR_GL_FN(glBlendFunc)
    FR_GL_FN(glDrawArrays)
    FR_GL_FN(glDeleteShader)
#undef FR_GL_FN
}
#endif

#define CELL_PX 8
#define MAX_QUADS (FR_GRID_CELLS * 2)
#define FLOATS_PER_VERT 8
#define VERTS_PER_QUAD 6

static FrGame game;
static SDL_Window *window;
static SDL_GLContext glctx;
static GLuint prog, vbo, atlas_tex;
static GLint u_origin, u_size;
static float *verts;
static int quit_requested;
static int vert_count;

typedef struct {
    uint8_t held[SDL_SCANCODE_COUNT];
    uint8_t pressed[SDL_SCANCODE_COUNT];
    float mdx, mdy;
} KeyState;

static KeyState keys;

static void set_key(SDL_Scancode sc, uint8_t down)
{
    if (sc < 0 || sc >= SDL_SCANCODE_COUNT) return;
    if (down && !keys.held[sc]) keys.pressed[sc] = 1;
    keys.held[sc] = down;
}

/* ---------- shaders ---------- */

static const char *VS =
#ifdef __EMSCRIPTEN__
    "#version 300 es\n"
    "precision highp float;\n"
#else
    "#version 330\n"
#endif
    "in vec2 a_pos;\n"
    "in vec2 a_uv;\n"
    "in vec4 a_col;\n"
    "uniform vec2 u_origin;\n"
    "uniform vec2 u_size;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_col;\n"
    "void main() {\n"
    "    vec2 c = vec2((a_pos.x - u_origin.x) / u_size.x * 2.0 - 1.0,\n"
    "                  1.0 - (a_pos.y - u_origin.y) / u_size.y * 2.0);\n"
    "    gl_Position = vec4(c, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "    v_col = a_col;\n"
    "}\n";

static const char *FS =
#ifdef __EMSCRIPTEN__
    "#version 300 es\n"
    "precision mediump float;\n"
#else
    "#version 330\n"
#endif
    "in vec2 v_uv;\n"
    "in vec4 v_col;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "    vec4 t = texture(u_tex, v_uv);\n"
    "    frag = vec4(v_col.rgb, v_col.a * t.a);\n"
    "}\n";

static GLuint compile(GLenum type, const char *src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetShaderInfoLog(sh, sizeof(log), &n, log);
        SDL_Log("shader error: %s", log);
    }
    return sh;
}

static void gl_init(void)
{
    GLuint vs = compile(GL_VERTEX_SHADER, VS);
    GLuint fs = compile(GL_FRAGMENT_SHADER, FS);
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) SDL_Log("program link failed");
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(prog);

    GLint u_tex = glGetUniformLocation(prog, "u_tex");
    u_origin = glGetUniformLocation(prog, "u_origin");
    u_size = glGetUniformLocation(prog, "u_size");
    glUniform1i(u_tex, 0);

    static uint8_t atlas[128 * 128 * 4];
    fr_font_atlas_rgba8(atlas);
    glGenTextures(1, &atlas_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_QUADS * VERTS_PER_QUAD * FLOATS_PER_VERT * sizeof(float),
                 NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERT * sizeof(float), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERT * sizeof(float),
                          (void *)(2 * sizeof(float)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, FLOATS_PER_VERT * sizeof(float),
                          (void *)(4 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/* ---------- grid blit ---------- */

static void push_quad(int *nv, float x, float y, float w, float h, int glyph,
                      const uint8_t col[4])
{
    float u0 = (float)(glyph % 16) * 8.0f / 128.0f;
    float v0 = (float)(glyph / 16) * 8.0f / 128.0f;
    float u1 = u0 + 8.0f / 128.0f;
    float v1 = v0 + 8.0f / 128.0f;
    float *v = verts + (*nv) * FLOATS_PER_VERT;
    float coords[6][4] = {
        {x, y, u0, v0},        {x + w, y, u1, v0},  {x + w, y + h, u1, v1},
        {x, y, u0, v0},        {x + w, y + h, u1, v1}, {x, y + h, u0, v1},
    };
    for (int i = 0; i < 6; i++) {
        float *q = v + i * FLOATS_PER_VERT;
        q[0] = coords[i][0];
        q[1] = coords[i][1];
        q[2] = coords[i][2];
        q[3] = coords[i][3];
        q[4] = (float)col[0];
        q[5] = (float)col[1];
        q[6] = (float)col[2];
        q[7] = (float)col[3];
    }
    *nv += 6;
}

static void grid_to_gl(void)
{
    int nv = 0;
    const FrGrid *g = &game.grid;
    for (int y = 0; y < g->h; y++) {
        for (int x = 0; x < g->w; x++) {
            int i = y * g->w + x;
            uint8_t bgc[4], fgc[4];
            fr_rgb332_to_rgba8(g->bg[i], bgc);
            fr_rgb332_to_rgba8(g->fg[i], fgc);
            float px = (float)x * CELL_PX;
            float py = (float)y * CELL_PX;
            push_quad(&nv, px, py, CELL_PX, CELL_PX, 255, bgc);
            if (g->glyph[i] != ' ') {
                push_quad(&nv, px, py, CELL_PX, CELL_PX, g->glyph[i], fgc);
            }
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(nv * FLOATS_PER_VERT * sizeof(float)), verts);
    vert_count = nv;
}

static void draw_frame(void)
{
    int ww, wh;
    SDL_GetWindowSizeInPixels(window, &ww, &wh);
    float scale_x = (float)ww / (game.grid.w * CELL_PX);
    float scale_y = (float)wh / (game.grid.h * CELL_PX);
    float scale = scale_x < scale_y ? scale_x : scale_y;
    float vw = game.grid.w * CELL_PX * scale;
    float vh = game.grid.h * CELL_PX * scale;
    float ox = ((float)ww - vw) * 0.5f;
    float oy = ((float)wh - vh) * 0.5f;
    glViewport(0, 0, ww, wh);
    glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    grid_to_gl();
    glUseProgram(prog);
    glUniform2f(u_origin, ox, oy);
    glUniform2f(u_size, vw, vh);
    glDrawArrays(GL_TRIANGLES, 0, vert_count);
}

/* ---------- input ---------- */

static void fill_input(FrInput *in)
{
    memset(in, 0, sizeof(*in));
    in->fwd = keys.held[SDL_SCANCODE_W];
    in->back = keys.held[SDL_SCANCODE_S];
    in->strafe_l = keys.held[SDL_SCANCODE_A];
    in->strafe_r = keys.held[SDL_SCANCODE_D];
    in->turn_l = keys.held[SDL_SCANCODE_LEFT];
    in->turn_r = keys.held[SDL_SCANCODE_RIGHT];
    in->rot_l = keys.held[SDL_SCANCODE_A];
    in->rot_r = keys.held[SDL_SCANCODE_D];
    in->up = keys.pressed[SDL_SCANCODE_UP];
    in->down = keys.pressed[SDL_SCANCODE_DOWN];
    in->interact = keys.pressed[SDL_SCANCODE_E];
    in->escape = keys.pressed[SDL_SCANCODE_ESCAPE];
    in->enter = (uint8_t)(keys.pressed[SDL_SCANCODE_RETURN] |
                          keys.pressed[SDL_SCANCODE_KP_ENTER]);
    in->buy = keys.pressed[SDL_SCANCODE_B];
    in->sell = keys.pressed[SDL_SCANCODE_S];
    in->key_1 = keys.pressed[SDL_SCANCODE_1];
    in->key_2 = keys.pressed[SDL_SCANCODE_2];
    in->key_3 = keys.pressed[SDL_SCANCODE_3];
    in->key_4 = keys.pressed[SDL_SCANCODE_4];
    in->key_5 = keys.pressed[SDL_SCANCODE_5];
    in->mouse_dx = keys.mdx;
    in->mouse_dy = keys.mdy;
    keys.mdx = 0;
    keys.mdy = 0;
    memset(keys.pressed, 0, sizeof(keys.pressed));
}

static void poll_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            quit_requested = 1;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (!e.key.repeat) set_key(e.key.scancode, 1);
            break;
        case SDL_EVENT_KEY_UP:
            set_key(e.key.scancode, 0);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            keys.mdx += (float)e.motion.xrel;
            keys.mdy += (float)e.motion.yrel;
            break;
        default:
            break;
        }
    }
}

/* ---------- main loop ---------- */

static void frame(void)
{
    poll_events();
    FrInput in;
    fill_input(&in);
    fr_game_update(&game, &in, 1.0f / 60.0f);
    fr_game_render(&game);
    draw_frame();
    SDL_GL_SwapWindow(window);
    if (quit_requested) {
        game.running = 0;
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
    }
}

#ifdef __EMSCRIPTEN__
static void em_frame(void)
{
    frame();
}
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return 1;
    }
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window = SDL_CreateWindow("Space Shipper", FR_GRID_W * CELL_PX, FR_GRID_H * CELL_PX,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("window failed: %s", SDL_GetError());
        return 1;
    }
    glctx = SDL_GL_CreateContext(window);
    if (!glctx) {
        SDL_Log("GL context failed: %s", SDL_GetError());
        return 1;
    }
    SDL_SetWindowRelativeMouseMode(window, true);
#ifndef __EMSCRIPTEN__
    load_gl_fns();
#endif

    verts = malloc(MAX_QUADS * VERTS_PER_QUAD * FLOATS_PER_VERT * sizeof(float));
    if (!verts) return 1;

    gl_init();

    fr_game_init(&game, 0);
    char *pref = SDL_GetPrefPath("kilo", "space_shipper");
    if (pref) {
        snprintf(game.save_path, sizeof(game.save_path), "%ssave.json", pref);
        SDL_free(pref);
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(em_frame, 60, 1);
#else
    while (game.running && !quit_requested) {
        frame();
    }
#endif

    free(verts);
    SDL_GL_DestroyContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
