/*=============================================================================
  gles_layer.cpp : OpenGL ES 3.0 translation layer, phase 0.

  Phase 0 only routes the function table: a synthesized driver report so the
  renderer picks its ARB-program feature path, a few state-free forwards, and
  logging stubs for the rest. See GLES-PORT-PLAN.md.
=============================================================================*/
#include "RenderPCH.h"
#include "../GL_Renderer.h"
#include "gles_layer.h"

#include <map>
#include <string>
#include <set>

//////////////////////////////////////////////////////////////////////////
// Stubs: one per entry in GLFuncs.h, generated with the X-macro.
//////////////////////////////////////////////////////////////////////////

static std::map<std::string, unsigned>& StubHits()
{
  static std::map<std::string, unsigned> hits;
  return hits;
}

static void GLES_StubHit(const char* name)
{
  unsigned& n = StubHits()[name];
  if (n++ == 0)
  {
    if (iLog) iLog->Log("GLES: unimplemented %s called\n", name);
    else printf("GLES: unimplemented %s called\n", name);
  }
}

#define GL_EXT(name)
#define GL_PROC(ext,ret,func,parms) static ret __stdcall stub_##func parms { GLES_StubHit(#func); return (ret)0; }
#include "../GLFuncs.h"
#undef GL_EXT
#undef GL_PROC

struct SNamedProc { const char* name; void* proc; };

// Built with statements: GLFuncs.h has typedefs between its GL_PROC lines.
static std::map<std::string, void*>& StubTable()
{
  static std::map<std::string, void*> table;
  if (table.empty())
  {
#define GL_EXT(name)
#define GL_PROC(ext,ret,func,parms) table[#func] = (void*)stub_##func;
#include "../GLFuncs.h"
#undef GL_EXT
#undef GL_PROC
  }
  return table;
}

//////////////////////////////////////////////////////////////////////////
// Native ES entry points the layer uses itself.
//////////////////////////////////////////////////////////////////////////

typedef const GLubyte* (*PFN_es_glGetString)(GLenum);
typedef void (*PFN_es_glGetIntegerv)(GLenum, GLint*);
typedef void (*PFN_es_glGetFloatv)(GLenum, GLfloat*);
typedef GLenum (*PFN_es_glGetError)();
typedef void (*PFN_es_glEnable)(GLenum);
typedef void (*PFN_es_glDisable)(GLenum);
typedef GLboolean (*PFN_es_glIsEnabled)(GLenum);
typedef void (*PFN_es_glClearDepthf)(GLfloat);
typedef void (*PFN_es_glDepthRangef)(GLfloat, GLfloat);

static PFN_es_glGetString   es_glGetString;
static PFN_es_glGetIntegerv es_glGetIntegerv;
static PFN_es_glGetFloatv   es_glGetFloatv;
static PFN_es_glGetError    es_glGetError;
static PFN_es_glEnable      es_glEnable;
static PFN_es_glDisable     es_glDisable;
static PFN_es_glIsEnabled   es_glIsEnabled;
static PFN_es_glClearDepthf es_glClearDepthf;
static PFN_es_glDepthRangef es_glDepthRangef;

static std::string sNativeExtensions;
static std::string sExtensions;
static std::string sVendor, sRenderer, sVersion;
static bool sNativeQueried;

static bool HasNativeExt(const char* name)
{
  return sNativeExtensions.find(name) != std::string::npos;
}

// Lazy because the function table is filled before the context exists.
static void QueryNative()
{
  if (sNativeQueried || !es_glGetString)
    return;
  sNativeQueried = true;
  const char* ext = (const char*)es_glGetString(GL_EXTENSIONS);
  sNativeExtensions = ext ? ext : "";
  sVendor   = std::string("GLES layer / ") + (const char*)es_glGetString(GL_VENDOR);
  sRenderer = (const char*)es_glGetString(GL_RENDERER);
  sVersion  = std::string("2.1 (GLES layer on ") + (const char*)es_glGetString(GL_VERSION) + ")";

  // What the layer emulates, plus what the ES driver really has. The renderer
  // picks its ARB-program (PS 2.0 class) path from this list.
  sExtensions =
    "GL_ARB_multitexture GL_ARB_texture_env_combine GL_EXT_texture_env_combine GL_EXT_texture_env_add "
    "GL_ARB_texture_cube_map GL_EXT_texture_cube_map GL_ARB_vertex_program GL_ARB_fragment_program "
    "GL_ARB_vertex_buffer_object GL_EXT_stencil_two_side GL_EXT_stencil_wrap GL_NV_occlusion_query "
    "GL_ARB_texture_compression GL_SGIS_generate_mipmap GL_EXT_texture3D GL_EXT_bgra "
    "GL_EXT_secondary_color GL_EXT_draw_range_elements GL_EXT_texture_lod_bias GL_EXT_texture_rectangle ";
  if (HasNativeExt("GL_EXT_texture_compression_s3tc") || HasNativeExt("GL_ANGLE_texture_compression_dxt"))
    sExtensions += "GL_EXT_texture_compression_s3tc ";
  if (HasNativeExt("GL_EXT_texture_filter_anisotropic"))
    sExtensions += "GL_EXT_texture_filter_anisotropic ";
}

//////////////////////////////////////////////////////////////////////////
// Implemented entry points.
//////////////////////////////////////////////////////////////////////////

static const GLubyte* __stdcall gles_glGetString(GLenum name)
{
  QueryNative();
  switch (name)
  {
    case GL_VENDOR:     return (const GLubyte*)sVendor.c_str();
    case GL_RENDERER:   return (const GLubyte*)sRenderer.c_str();
    case GL_VERSION:    return (const GLubyte*)sVersion.c_str();
    case GL_EXTENSIONS: return (const GLubyte*)sExtensions.c_str();
    case 0x8874 /* GL_PROGRAM_ERROR_STRING_ARB */: return (const GLubyte*)"";
  }
  return es_glGetString ? es_glGetString(name) : (const GLubyte*)"";
}

// Legacy queries answered by the layer; everything else goes to the driver.
static bool LegacyGetInteger(GLenum pname, GLint* params)
{
  switch (pname)
  {
    case GL_MAX_LIGHTS:                  params[0] = 8; return true;
    case GL_MAX_CLIP_PLANES:             params[0] = 6; return true;
    case GL_MAX_MODELVIEW_STACK_DEPTH:   params[0] = 32; return true;
    case GL_MAX_PROJECTION_STACK_DEPTH:  params[0] = 8; return true;
    case GL_MAX_TEXTURE_STACK_DEPTH:     params[0] = 8; return true;
    case 0x84E2 /* GL_MAX_TEXTURE_UNITS_ARB */:
    case 0x8871 /* GL_MAX_TEXTURE_COORDS_ARB */: params[0] = 8; return true;
    case GL_RED_BITS: case GL_GREEN_BITS: case GL_BLUE_BITS: case GL_ALPHA_BITS: params[0] = 8; return true;
    case GL_DEPTH_BITS:                  params[0] = 24; return true;
    case GL_STENCIL_BITS:                params[0] = 8; return true;
    case GL_FOG:                         params[0] = 0; return true;
    case 0x864B /* GL_PROGRAM_ERROR_POSITION_ARB */: params[0] = -1; return true;
    case GL_MODELVIEW_MATRIX: case GL_PROJECTION_MATRIX:
      for (int i = 0; i < 16; i++) params[i] = (i % 5 == 0) ? 1 : 0;
      return true;
  }
  return false;
}

static void __stdcall gles_glGetIntegerv(GLenum pname, GLint* params)
{
  if (LegacyGetInteger(pname, params))
    return;
  if (es_glGetIntegerv)
    es_glGetIntegerv(pname, params);
}

static void __stdcall gles_glGetFloatv(GLenum pname, GLfloat* params)
{
  GLint iv[16];
  switch (pname)
  {
    case GL_CURRENT_COLOR:
      params[0] = params[1] = params[2] = params[3] = 1.0f;
      return;
    case GL_MODELVIEW_MATRIX: case GL_PROJECTION_MATRIX:
      for (int i = 0; i < 16; i++) params[i] = (i % 5 == 0) ? 1.0f : 0.0f;
      return;
  }
  if (LegacyGetInteger(pname, iv))
  {
    params[0] = (GLfloat)iv[0];
    return;
  }
  if (es_glGetFloatv)
    es_glGetFloatv(pname, params);
}

static GLenum __stdcall gles_glGetError()
{
  return es_glGetError ? es_glGetError() : GL_NO_ERROR;
}

// Legacy enables are remembered for the fixed-function emulation; ES caps are forwarded.
static std::set<GLenum> sLegacyEnabled;

static bool IsNativeCap(GLenum cap)
{
  switch (cap)
  {
    case GL_BLEND: case GL_CULL_FACE: case GL_DEPTH_TEST: case GL_STENCIL_TEST:
    case GL_SCISSOR_TEST: case GL_DITHER: case GL_POLYGON_OFFSET_FILL:
    case 0x809E /* GL_SAMPLE_ALPHA_TO_COVERAGE */: case 0x80A0 /* GL_SAMPLE_COVERAGE */:
      return true;
  }
  return false;
}

static void __stdcall gles_glEnable(GLenum cap)
{
  if (IsNativeCap(cap)) { if (es_glEnable) es_glEnable(cap); }
  else sLegacyEnabled.insert(cap);
}

static void __stdcall gles_glDisable(GLenum cap)
{
  if (IsNativeCap(cap)) { if (es_glDisable) es_glDisable(cap); }
  else sLegacyEnabled.erase(cap);
}

static GLboolean __stdcall gles_glIsEnabled(GLenum cap)
{
  if (IsNativeCap(cap)) return es_glIsEnabled ? es_glIsEnabled(cap) : GL_FALSE;
  return sLegacyEnabled.count(cap) ? GL_TRUE : GL_FALSE;
}

static void __stdcall gles_glClearDepth(GLclampd depth)
{
  if (es_glClearDepthf) es_glClearDepthf((GLfloat)depth);
}

static void __stdcall gles_glDepthRange(GLclampd zNear, GLclampd zFar)
{
  if (es_glDepthRangef) es_glDepthRangef((GLfloat)zNear, (GLfloat)zFar);
}

static const SNamedProc sImplTable[] = {
  { "glGetString",   (void*)gles_glGetString },
  { "glGetIntegerv", (void*)gles_glGetIntegerv },
  { "glGetFloatv",   (void*)gles_glGetFloatv },
  { "glGetError",    (void*)gles_glGetError },
  { "glEnable",      (void*)gles_glEnable },
  { "glDisable",     (void*)gles_glDisable },
  { "glIsEnabled",   (void*)gles_glIsEnabled },
  { "glClearDepth",  (void*)gles_glClearDepth },
  { "glDepthRange",  (void*)gles_glDepthRange },
  { NULL, NULL }
};

//////////////////////////////////////////////////////////////////////////
// Straight forwards: identical semantics in ES 3.0. Renderer name -> ES name.
//////////////////////////////////////////////////////////////////////////

struct SForward { const char* name; const char* esName; };

static const SForward sForwardTable[] = {
  { "glClear", "glClear" }, { "glClearColor", "glClearColor" }, { "glClearStencil", "glClearStencil" },
  { "glViewport", "glViewport" }, { "glScissor", "glScissor" },
  { "glBlendFunc", "glBlendFunc" }, { "glDepthFunc", "glDepthFunc" }, { "glDepthMask", "glDepthMask" },
  { "glColorMask", "glColorMask" }, { "glCullFace", "glCullFace" }, { "glFrontFace", "glFrontFace" },
  { "glStencilFunc", "glStencilFunc" }, { "glStencilOp", "glStencilOp" }, { "glStencilMask", "glStencilMask" },
  { "glFinish", "glFinish" }, { "glFlush", "glFlush" }, { "glPolygonOffset", "glPolygonOffset" },
  { "glGenTextures", "glGenTextures" }, { "glDeleteTextures", "glDeleteTextures" },
  { "glLineWidth", "glLineWidth" }, { "glActiveTextureARB", "glActiveTexture" },
  { NULL, NULL }
};

//////////////////////////////////////////////////////////////////////////

bool GLES_Init()
{
  es_glGetString   = (PFN_es_glGetString)SDL_GL_GetProcAddress("glGetString");
  es_glGetIntegerv = (PFN_es_glGetIntegerv)SDL_GL_GetProcAddress("glGetIntegerv");
  es_glGetFloatv   = (PFN_es_glGetFloatv)SDL_GL_GetProcAddress("glGetFloatv");
  es_glGetError    = (PFN_es_glGetError)SDL_GL_GetProcAddress("glGetError");
  es_glEnable      = (PFN_es_glEnable)SDL_GL_GetProcAddress("glEnable");
  es_glDisable     = (PFN_es_glDisable)SDL_GL_GetProcAddress("glDisable");
  es_glIsEnabled   = (PFN_es_glIsEnabled)SDL_GL_GetProcAddress("glIsEnabled");
  es_glClearDepthf = (PFN_es_glClearDepthf)SDL_GL_GetProcAddress("glClearDepthf");
  es_glDepthRangef = (PFN_es_glDepthRangef)SDL_GL_GetProcAddress("glDepthRangef");
  sNativeQueried = false;
  if (!es_glGetString)
  {
    iLog->Log("GLES: SDL_GL_GetProcAddress(glGetString) failed: %s\n", SDL_GetError());
    return false;
  }
  return true;
}

void* GLES_GetProcAddress(const char* name)
{
  for (const SNamedProc* p = sImplTable; p->name; p++)
    if (!strcmp(p->name, name))
      return p->proc;
  for (const SForward* f = sForwardTable; f->name; f++)
    if (!strcmp(f->name, name))
    {
      void* proc = (void*)SDL_GL_GetProcAddress(f->esName);
      if (proc)
        return proc;
      iLog->Log("GLES: driver lacks %s (for %s), using stub\n", f->esName, name);
      break;
    }
  std::map<std::string, void*>::iterator it = StubTable().find(name);
  if (it != StubTable().end())
    return it->second;
  iLog->Log("GLES: %s is not in GLFuncs.h\n", name);
  return NULL;
}

void GLES_ReportUnimplemented()
{
  std::map<std::string, unsigned>& hits = StubHits();
  iLog->Log("GLES: %d unimplemented entry points were called:\n", (int)hits.size());
  for (std::map<std::string, unsigned>::iterator it = hits.begin(); it != hits.end(); ++it)
    iLog->Log("GLES:   %-40s %u\n", it->first.c_str(), it->second);
}

void GLES_SetContextAttributes()
{
#ifdef __APPLE__
  // macOS has no ES; SDL loads ANGLE's EGL/GLESv2 from FARCRY_ANGLE_DIR or next to the executable.
  const char* dir = getenv("FARCRY_ANGLE_DIR");
  std::string base = dir ? std::string(dir) + "/" : std::string(SDL_GetBasePath());
  static std::string egl, gles;
  egl = base + "libEGL.dylib";
  gles = base + "libGLESv2.dylib";
  SDL_SetHint(SDL_HINT_EGL_LIBRARY, egl.c_str());
  SDL_SetHint(SDL_HINT_OPENGL_LIBRARY, gles.c_str());
  iLog->Log("GLES: ANGLE from %s\n", base.c_str());
#endif
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}
