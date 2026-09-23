/*=============================================================================
  gles_vertex.cpp : current attributes, immediate mode, client arrays, VBOs, draws.
=============================================================================*/
#include "gles_internal.h"

#define IM_UNITS 4

struct SImVertex
{
  float pos[4];
  float normal[3];
  float color[4];
  float color2[3];
  float tex[IM_UNITS][4];
};

static std::vector<SImVertex> sIm;
static std::vector<GLushort> sIndices;

//////////////////////////////////////////////////////////////////////////
// Current attributes
//////////////////////////////////////////////////////////////////////////

static void SetColor(float r, float g, float b, float a) { g_es.color[0] = r; g_es.color[1] = g; g_es.color[2] = b; g_es.color[3] = a; }

static void __stdcall gles_glColor3f(GLfloat r, GLfloat g, GLfloat b) { SetColor(r, g, b, 1); }
static void __stdcall gles_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { SetColor(r, g, b, a); }
static void __stdcall gles_glColor3fv(const GLfloat* v) { SetColor(v[0], v[1], v[2], 1); }
static void __stdcall gles_glColor4fv(const GLfloat* v) { SetColor(v[0], v[1], v[2], v[3]); }
static void __stdcall gles_glColor3d(GLdouble r, GLdouble g, GLdouble b) { SetColor((float)r, (float)g, (float)b, 1); }
static void __stdcall gles_glColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a) { SetColor((float)r, (float)g, (float)b, (float)a); }
static void __stdcall gles_glColor3ub(GLubyte r, GLubyte g, GLubyte b) { SetColor(r/255.f, g/255.f, b/255.f, 1); }
static void __stdcall gles_glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { SetColor(r/255.f, g/255.f, b/255.f, a/255.f); }
static void __stdcall gles_glColor3ubv(const GLubyte* v) { SetColor(v[0]/255.f, v[1]/255.f, v[2]/255.f, 1); }
static void __stdcall gles_glColor4ubv(const GLubyte* v) { SetColor(v[0]/255.f, v[1]/255.f, v[2]/255.f, v[3]/255.f); }

static void __stdcall gles_glNormal3f(GLfloat x, GLfloat y, GLfloat z) { g_es.normal[0] = x; g_es.normal[1] = y; g_es.normal[2] = z; }
static void __stdcall gles_glNormal3fv(const GLfloat* v) { gles_glNormal3f(v[0], v[1], v[2]); }

static void __stdcall gles_glSecondaryColor3fEXT(GLfloat r, GLfloat g, GLfloat b) { g_es.color2[0] = r; g_es.color2[1] = g; g_es.color2[2] = b; }
static void __stdcall gles_glSecondaryColor3fvEXT(const GLfloat* v) { gles_glSecondaryColor3fEXT(v[0], v[1], v[2]); }
static void __stdcall gles_glSecondaryColor3ubvEXT(const GLubyte* v) { gles_glSecondaryColor3fEXT(v[0]/255.f, v[1]/255.f, v[2]/255.f); }

static void SetTex(int unit, float s, float t, float r, float q)
{
  if (unit < 0 || unit >= GLES_MAX_UNITS) return;
  g_es.texcoord[unit][0] = s; g_es.texcoord[unit][1] = t; g_es.texcoord[unit][2] = r; g_es.texcoord[unit][3] = q;
}

static void __stdcall gles_glTexCoord1f(GLfloat s) { SetTex(0, s, 0, 0, 1); }
static void __stdcall gles_glTexCoord2f(GLfloat s, GLfloat t) { SetTex(0, s, t, 0, 1); }
static void __stdcall gles_glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) { SetTex(0, s, t, r, 1); }
static void __stdcall gles_glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) { SetTex(0, s, t, r, q); }
static void __stdcall gles_glTexCoord2fv(const GLfloat* v) { SetTex(0, v[0], v[1], 0, 1); }
static void __stdcall gles_glTexCoord3fv(const GLfloat* v) { SetTex(0, v[0], v[1], v[2], 1); }
static void __stdcall gles_glTexCoord4fv(const GLfloat* v) { SetTex(0, v[0], v[1], v[2], v[3]); }
static void __stdcall gles_glTexCoord2d(GLdouble s, GLdouble t) { SetTex(0, (float)s, (float)t, 0, 1); }

static void __stdcall gles_glMultiTexCoord1fARB(GLenum u, GLfloat s) { SetTex(u - ES_TEXTURE0, s, 0, 0, 1); }
static void __stdcall gles_glMultiTexCoord2fARB(GLenum u, GLfloat s, GLfloat t) { SetTex(u - ES_TEXTURE0, s, t, 0, 1); }
static void __stdcall gles_glMultiTexCoord3fARB(GLenum u, GLfloat s, GLfloat t, GLfloat r) { SetTex(u - ES_TEXTURE0, s, t, r, 1); }
static void __stdcall gles_glMultiTexCoord4fARB(GLenum u, GLfloat s, GLfloat t, GLfloat r, GLfloat q) { SetTex(u - ES_TEXTURE0, s, t, r, q); }
static void __stdcall gles_glMultiTexCoord2fvARB(GLenum u, const GLfloat* v) { SetTex(u - ES_TEXTURE0, v[0], v[1], 0, 1); }
static void __stdcall gles_glMultiTexCoord3fvARB(GLenum u, const GLfloat* v) { SetTex(u - ES_TEXTURE0, v[0], v[1], v[2], 1); }
static void __stdcall gles_glMultiTexCoord4fvARB(GLenum u, const GLfloat* v) { SetTex(u - ES_TEXTURE0, v[0], v[1], v[2], v[3]); }

//////////////////////////////////////////////////////////////////////////
// Immediate mode
//////////////////////////////////////////////////////////////////////////

static void __stdcall gles_glBegin(GLenum mode)
{
  g_es.inBegin = true;
  g_es.beginMode = mode;
  sIm.clear();
}

static void Emit(float x, float y, float z, float w)
{
  if (!g_es.inBegin) return;
  SImVertex v;
  v.pos[0] = x; v.pos[1] = y; v.pos[2] = z; v.pos[3] = w;
  memcpy(v.normal, g_es.normal, sizeof(v.normal));
  memcpy(v.color, g_es.color, sizeof(v.color));
  memcpy(v.color2, g_es.color2, sizeof(v.color2));
  for (int i = 0; i < IM_UNITS; i++) memcpy(v.tex[i], g_es.texcoord[i], sizeof(v.tex[i]));
  sIm.push_back(v);
}

static void __stdcall gles_glVertex2f(GLfloat x, GLfloat y) { Emit(x, y, 0, 1); }
static void __stdcall gles_glVertex3f(GLfloat x, GLfloat y, GLfloat z) { Emit(x, y, z, 1); }
static void __stdcall gles_glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { Emit(x, y, z, w); }
static void __stdcall gles_glVertex2fv(const GLfloat* v) { Emit(v[0], v[1], 0, 1); }
static void __stdcall gles_glVertex3fv(const GLfloat* v) { Emit(v[0], v[1], v[2], 1); }
static void __stdcall gles_glVertex4fv(const GLfloat* v) { Emit(v[0], v[1], v[2], v[3]); }
static void __stdcall gles_glVertex2i(GLint x, GLint y) { Emit((float)x, (float)y, 0, 1); }
static void __stdcall gles_glVertex3i(GLint x, GLint y, GLint z) { Emit((float)x, (float)y, (float)z, 1); }
static void __stdcall gles_glVertex2d(GLdouble x, GLdouble y) { Emit((float)x, (float)y, 0, 1); }
static void __stdcall gles_glVertex3d(GLdouble x, GLdouble y, GLdouble z) { Emit((float)x, (float)y, (float)z, 1); }
static void __stdcall gles_glVertex3dv(const GLdouble* v) { Emit((float)v[0], (float)v[1], (float)v[2], 1); }

//////////////////////////////////////////////////////////////////////////
// Attribute state shadow: ANGLE's per-call cost makes redundant pointer/enable/constant calls the
// dominant expense, so only changes are issued.
//////////////////////////////////////////////////////////////////////////

struct SAttribShadow
{
  bool enabled;
  GLuint buffer; GLint size; GLenum type; GLboolean norm; GLsizei stride; const void* ptr;
  bool constValid; float constant[4];
};
static SAttribShadow sAttr[16];
static GLuint sBoundArrayBuffer;

static void NativeBindArrayBuffer(GLuint id)
{
  if (sBoundArrayBuffer == id) return;
  sBoundArrayBuffer = id;
  es_glBindBuffer(ES_ARRAY_BUFFER, id);
}

static void SetAttribArray(int slot, GLuint buffer, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* ptr)
{
  SAttribShadow& a = sAttr[slot];
  // Client memory (buffer 0) is read at draw time, so the same pointer with new contents is fine.
  if (a.enabled && a.buffer == buffer && a.size == size && a.type == type && a.norm == norm && a.stride == stride && a.ptr == ptr)
    return;
  NativeBindArrayBuffer(buffer);
  if (!a.enabled) { es_glEnableVertexAttribArray(slot); a.enabled = true; }
  es_glVertexAttribPointer(slot, size, type, norm, stride, ptr);
  a.buffer = buffer; a.size = size; a.type = type; a.norm = norm; a.stride = stride; a.ptr = ptr;
}

static void SetAttribConstant(int slot, float x, float y, float z, float w)
{
  SAttribShadow& a = sAttr[slot];
  if (a.enabled) { es_glDisableVertexAttribArray(slot); a.enabled = false; }
  if (a.constValid && a.constant[0] == x && a.constant[1] == y && a.constant[2] == z && a.constant[3] == w) return;
  es_glVertexAttrib4f(slot, x, y, z, w);
  a.constValid = true;
  a.constant[0] = x; a.constant[1] = y; a.constant[2] = z; a.constant[3] = w;
}

// The renderer's own glBindBufferARB goes through the layer too; keep the shadow in step.
void GLES_NoteArrayBufferBound(GLuint id) { sBoundArrayBuffer = id; }

// Immediate-mode vertices stream through an orphaned VBO: client arrays cost a buffer allocation
// per draw on ANGLE. Quads use a static index pattern.
#define IM_VBO_BYTES (2 * 1024 * 1024)
#define IM_MAX_QUADS 16000
static GLuint sImVBO, sImOffset = IM_VBO_BYTES, sQuadIBO;

// Unsynchronized writes into a ring that is orphaned when it wraps: measured on ANGLE at 163 fps against
// 35 fps for glBufferSubData into the same ring and 124 fps for glBufferData per draw.
// FARCRY_GLES_IMMODE selects the other two for comparison: 0 subdata, 1 bufferdata per draw.
static GLuint UploadImmediate(const SImVertex* v, int n)
{
  SESTimer timer(GLES_T_IMUPLOAD);
  static int mode = getenv("FARCRY_GLES_IMMODE") ? atoi(getenv("FARCRY_GLES_IMMODE")) : 2;
  GLuint bytes = n * sizeof(SImVertex);
  if (!sImVBO) es_glGenBuffers(1, &sImVBO);
  NativeBindArrayBuffer(sImVBO);
  if (mode == 1)
  {
    es_glBufferData(ES_ARRAY_BUFFER, bytes, v, ES_STREAM_DRAW);
    return 0;
  }
  if (sImOffset + bytes > IM_VBO_BYTES)
  {
    es_glBufferData(ES_ARRAY_BUFFER, IM_VBO_BYTES, NULL, ES_STREAM_DRAW);  // orphan
    sImOffset = 0;
  }
  GLuint off = sImOffset;
  if (mode == 2)
  {
    void* p = es_glMapBufferRange(ES_ARRAY_BUFFER, off, bytes, ES_MAP_WRITE_BIT | ES_MAP_UNSYNCHRONIZED_BIT | ES_MAP_INVALIDATE_RANGE_BIT);
    if (p) { memcpy(p, v, bytes); es_glUnmapBuffer(ES_ARRAY_BUFFER); }
    else es_glBufferSubData(ES_ARRAY_BUFFER, off, bytes, v);
  }
  else
    es_glBufferSubData(ES_ARRAY_BUFFER, off, bytes, v);
  sImOffset += (bytes + 15) & ~15u;
  return off;
}

static void BindQuadIndices()
{
  if (!sQuadIBO)
  {
    std::vector<GLushort> idx(IM_MAX_QUADS * 6);
    for (int q = 0; q < IM_MAX_QUADS; q++)
    {
      GLushort b = (GLushort)(q * 4);
      idx[q*6+0] = b; idx[q*6+1] = b + 1; idx[q*6+2] = b + 2; idx[q*6+3] = b; idx[q*6+4] = b + 2; idx[q*6+5] = b + 3;
    }
    es_glGenBuffers(1, &sQuadIBO);
    es_glBindBuffer(ES_ELEMENT_ARRAY_BUFFER, sQuadIBO);
    es_glBufferData(ES_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLushort), &idx[0], ES_STATIC_DRAW);
  }
  else
    es_glBindBuffer(ES_ELEMENT_ARRAY_BUFFER, sQuadIBO);
}

static void SetAttribArrays(const SImVertex* v, int n)
{
  const GLsizei stride = sizeof(SImVertex);
  GLuint off = UploadImmediate(v, n);
  #define IMOFF(field) ((const void*)(uintptr_t)(off + offsetof(SImVertex, field)))
  SetAttribArray(GLES_ATTR_POS, sImVBO, 4, GL_FLOAT, GL_FALSE, stride, IMOFF(pos));
  SetAttribArray(GLES_ATTR_NORMAL, sImVBO, 3, GL_FLOAT, GL_FALSE, stride, IMOFF(normal));
  SetAttribArray(GLES_ATTR_COLOR, sImVBO, 4, GL_FLOAT, GL_FALSE, stride, IMOFF(color));
  SetAttribArray(GLES_ATTR_COLOR2, sImVBO, 3, GL_FLOAT, GL_FALSE, stride, IMOFF(color2));
  SetAttribArray(GLES_ATTR_TEX0 + 0, sImVBO, 4, GL_FLOAT, GL_FALSE, stride, IMOFF(tex[0]));
  SetAttribArray(GLES_ATTR_TEX0 + 1, sImVBO, 4, GL_FLOAT, GL_FALSE, stride, IMOFF(tex[1]));
  SetAttribArray(GLES_ATTR_TEX0 + 2, sImVBO, 4, GL_FLOAT, GL_FALSE, stride, IMOFF(tex[2]));
  SetAttribArray(GLES_ATTR_TEX0 + 3, sImVBO, 4, GL_FLOAT, GL_FALSE, stride, IMOFF(tex[3]));
  #undef IMOFF
  for (int i = IM_UNITS; i < GLES_MAX_UNITS; i++)
    SetAttribConstant(GLES_ATTR_TEX0 + i, 0, 0, 0, 1);
}

// Quads become an indexed triangle list; the rest map straight onto ES primitives.
static void DrawImmediate()
{
  SESTimer timer(GLES_T_IMMEDIATE);
  int n = (int)sIm.size();
  if (n == 0) return;
  GLenum mode = g_es.beginMode;
  if (!GLES_PrepareDraw()) return;
  SetAttribArrays(&sIm[0], n);
  switch (mode)
  {
    case GL_QUADS:
    {
      int quads = n / 4;
      if (quads > IM_MAX_QUADS) quads = IM_MAX_QUADS;
      BindQuadIndices();
      es_glDrawElements(GL_TRIANGLES, quads * 6, GL_UNSIGNED_SHORT, 0);
      break;
    }
    case GL_QUAD_STRIP: es_glDrawArrays(GL_TRIANGLE_STRIP, 0, n); break;
    case GL_POLYGON:    es_glDrawArrays(GL_TRIANGLE_FAN, 0, n); break;
    case GL_LINE_LOOP:  es_glDrawArrays(GL_LINE_LOOP, 0, n); break;
    default:            es_glDrawArrays(mode, 0, n); break;
  }
}

static void __stdcall gles_glEnd()
{
  if (!g_es.inBegin) return;
  g_es.inBegin = false;
  DrawImmediate();
  sIm.clear();
  static bool debug = getenv("FARCRY_GLES_DEBUG") != NULL;
  if (debug)
  {
    static int errors;
    GLenum err = es_glGetError();
    if (err && errors++ < 20) GLES_Log("GLES: GL error 0x%x after immediate draw", err);
  }
}

void GLES_FlushImmediate() {}

//////////////////////////////////////////////////////////////////////////
// Client arrays and buffers
//////////////////////////////////////////////////////////////////////////

static SESArray* ArrayFor(GLenum array)
{
  switch (array)
  {
    case GL_VERTEX_ARRAY: return &g_es.vertexArray;
    case GL_NORMAL_ARRAY: return &g_es.normalArray;
    case GL_COLOR_ARRAY:  return &g_es.colorArray;
    case GL_TEXTURE_COORD_ARRAY: return &g_es.texcoordArray[g_es.clientActiveUnit];
    case ES_SECONDARY_COLOR_ARRAY: return &g_es.color2Array;
  }
  return NULL;
}

static void __stdcall gles_glEnableClientState(GLenum array) { SESArray* a = ArrayFor(array); if (a) a->enabled = true; }
static void __stdcall gles_glDisableClientState(GLenum array) { SESArray* a = ArrayFor(array); if (a) a->enabled = false; }

static void SetPointer(SESArray& a, GLint size, GLenum type, GLsizei stride, const void* p)
{
  a.size = size; a.type = type; a.stride = stride; a.pointer = p; a.buffer = g_es.arrayBuffer;
}

static void __stdcall gles_glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p) { SetPointer(g_es.vertexArray, size, type, stride, p); }
static void __stdcall gles_glNormalPointer(GLenum type, GLsizei stride, const GLvoid* p) { SetPointer(g_es.normalArray, 3, type, stride, p); }
static void __stdcall gles_glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p) { SetPointer(g_es.colorArray, size, type, stride, p); }
static void __stdcall gles_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* p) { SetPointer(g_es.texcoordArray[g_es.clientActiveUnit], size, type, stride, p); }
static void __stdcall gles_glSecondaryColorPointerEXT(GLint size, GLenum type, GLsizei stride, const GLvoid* p) { SetPointer(g_es.color2Array, size, type, stride, p); }

static void __stdcall gles_glGenBuffersARB(GLsizei n, GLuint* ids) { es_glGenBuffers(n, ids); }
static void __stdcall gles_glDeleteBuffersARB(GLsizei n, const GLuint* ids)
{
  for (GLsizei i = 0; i < n; i++) g_esBuffers.erase(ids[i]);
  es_glDeleteBuffers(n, ids);
}
static void __stdcall gles_glBindBufferARB(GLenum target, GLuint id)
{
  if (target == ES_ARRAY_BUFFER) { g_es.arrayBuffer = id; GLES_NoteArrayBufferBound(id); }
  else if (target == ES_ELEMENT_ARRAY_BUFFER) g_es.elementBuffer = id;
  es_glBindBuffer(target, id);
}
static void __stdcall gles_glBufferDataARB(GLenum target, GLsizeiptrARB size, const GLvoid* data, GLenum usage)
{
  GLuint id = (target == ES_ARRAY_BUFFER) ? g_es.arrayBuffer : g_es.elementBuffer;
  g_esBuffers[id] = (GLuint)size;
  es_glBufferData(target, size, data, usage);
}
static void __stdcall gles_glBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid* data) { es_glBufferSubData(target, offset, size, data); }
static void* __stdcall gles_glMapBufferARB(GLenum target, GLenum access)
{
  SESTimer timer(GLES_T_MAP);
  GLuint id = (target == ES_ARRAY_BUFFER) ? g_es.arrayBuffer : g_es.elementBuffer;
  GLuint size = g_esBuffers[id];
  if (!size) return NULL;
  return es_glMapBufferRange(target, 0, size, ES_MAP_WRITE_BIT);
}
static GLboolean __stdcall gles_glUnmapBufferARB(GLenum target) { return es_glUnmapBuffer(target); }
static void __stdcall gles_glLockArraysEXT(GLint, GLsizei) {}
static void __stdcall gles_glUnlockArraysEXT() {}

static bool ArrayActive(const SESArray& a) { return a.enabled && (a.pointer != NULL || a.buffer != 0); }

static bool SetupClientArrays()
{
  if (!ArrayActive(g_es.vertexArray)) return false;
  SetAttribArray(GLES_ATTR_POS, g_es.vertexArray.buffer, g_es.vertexArray.size, g_es.vertexArray.type, GL_FALSE, g_es.vertexArray.stride, g_es.vertexArray.pointer);
  if (ArrayActive(g_es.normalArray)) SetAttribArray(GLES_ATTR_NORMAL, g_es.normalArray.buffer, 3, g_es.normalArray.type, GL_TRUE, g_es.normalArray.stride, g_es.normalArray.pointer);
  else SetAttribConstant(GLES_ATTR_NORMAL, g_es.normal[0], g_es.normal[1], g_es.normal[2], 0);
  if (ArrayActive(g_es.colorArray)) SetAttribArray(GLES_ATTR_COLOR, g_es.colorArray.buffer, g_es.colorArray.size, g_es.colorArray.type, GL_TRUE, g_es.colorArray.stride, g_es.colorArray.pointer);
  else SetAttribConstant(GLES_ATTR_COLOR, g_es.color[0], g_es.color[1], g_es.color[2], g_es.color[3]);
  if (ArrayActive(g_es.color2Array)) SetAttribArray(GLES_ATTR_COLOR2, g_es.color2Array.buffer, g_es.color2Array.size, g_es.color2Array.type, GL_TRUE, g_es.color2Array.stride, g_es.color2Array.pointer);
  else SetAttribConstant(GLES_ATTR_COLOR2, g_es.color2[0], g_es.color2[1], g_es.color2[2], 0);
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    const SESArray& t = g_es.texcoordArray[i];
    if (ArrayActive(t)) SetAttribArray(GLES_ATTR_TEX0 + i, t.buffer, t.size, t.type, GL_FALSE, t.stride, t.pointer);
    else SetAttribConstant(GLES_ATTR_TEX0 + i, g_es.texcoord[i][0], g_es.texcoord[i][1], g_es.texcoord[i][2], g_es.texcoord[i][3]);
  }
  NativeBindArrayBuffer(g_es.arrayBuffer);
  return true;
}

static std::vector<GLuint> sQuadIdx;

// Expand quad indices to triangles. Index data may live in the element buffer (then it is an offset) or in memory.
static const void* ExpandQuads(GLsizei count, GLenum type, const void* indices, GLsizei& outCount, GLenum& outType)
{
  sQuadIdx.clear();
  const GLubyte* p8 = (const GLubyte*)indices;
  if (g_es.elementBuffer)
  {
    GLES_Log("GLES: GL_QUADS from an element buffer is not supported yet");
    outCount = 0;
    return NULL;
  }
  for (GLsizei q = 0; q + 3 < count; q += 4)
  {
    GLuint v[4];
    for (int k = 0; k < 4; k++)
      v[k] = (type == GL_UNSIGNED_SHORT) ? ((const GLushort*)p8)[q+k] : (type == GL_UNSIGNED_INT) ? ((const GLuint*)p8)[q+k] : p8[q+k];
    sQuadIdx.push_back(v[0]); sQuadIdx.push_back(v[1]); sQuadIdx.push_back(v[2]);
    sQuadIdx.push_back(v[0]); sQuadIdx.push_back(v[2]); sQuadIdx.push_back(v[3]);
  }
  outCount = (GLsizei)sQuadIdx.size();
  outType = GL_UNSIGNED_INT;
  return sQuadIdx.empty() ? NULL : &sQuadIdx[0];
}

static void __stdcall gles_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
  SESTimer timer(GLES_T_DRAWELEM);
  if (!SetupClientArrays()) return;
  if (!GLES_PrepareDraw()) return;
  es_glBindBuffer(ES_ELEMENT_ARRAY_BUFFER, g_es.elementBuffer);
  if (mode == GL_QUADS)
  {
    GLsizei n; GLenum t;
    const void* idx = ExpandQuads(count, type, indices, n, t);
    if (idx) { es_glBindBuffer(ES_ELEMENT_ARRAY_BUFFER, 0); es_glDrawElements(GL_TRIANGLES, n, t, idx); }
    return;
  }
  if (mode == GL_QUAD_STRIP) mode = GL_TRIANGLE_STRIP;
  else if (mode == GL_POLYGON) mode = GL_TRIANGLE_FAN;
  es_glDrawElements(mode, count, type, indices);
}

static void __stdcall gles_glDrawRangeElementsEXT(GLenum mode, GLuint, GLuint, GLsizei count, GLenum type, const GLvoid* indices) { gles_glDrawElements(mode, count, type, indices); }

static void __stdcall gles_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
  if (!SetupClientArrays()) return;
  if (!GLES_PrepareDraw()) return;
  if (mode == GL_QUADS)
  {
    sQuadIdx.clear();
    for (GLsizei q = 0; q + 3 < count; q += 4)
    {
      GLuint b = first + q;
      sQuadIdx.push_back(b); sQuadIdx.push_back(b+1); sQuadIdx.push_back(b+2);
      sQuadIdx.push_back(b); sQuadIdx.push_back(b+2); sQuadIdx.push_back(b+3);
    }
    if (sQuadIdx.empty()) return;
    es_glBindBuffer(ES_ELEMENT_ARRAY_BUFFER, 0);
    es_glDrawElements(GL_TRIANGLES, (GLsizei)sQuadIdx.size(), GL_UNSIGNED_INT, &sQuadIdx[0]);
    return;
  }
  if (mode == GL_QUAD_STRIP) mode = GL_TRIANGLE_STRIP;
  else if (mode == GL_POLYGON) mode = GL_TRIANGLE_FAN;
  es_glDrawArrays(mode, first, count);
}

void GLES_RegisterVertex(std::map<std::string, void*>& t)
{
#define REG(name) t[#name] = (void*)gles_##name;
  REG(glColor3f) REG(glColor4f) REG(glColor3fv) REG(glColor4fv) REG(glColor3d) REG(glColor4d)
  REG(glColor3ub) REG(glColor4ub) REG(glColor3ubv) REG(glColor4ubv)
  REG(glNormal3f) REG(glNormal3fv)
  REG(glSecondaryColor3fEXT) REG(glSecondaryColor3fvEXT) REG(glSecondaryColor3ubvEXT)
  REG(glTexCoord1f) REG(glTexCoord2f) REG(glTexCoord3f) REG(glTexCoord4f) REG(glTexCoord2fv) REG(glTexCoord3fv) REG(glTexCoord4fv) REG(glTexCoord2d)
  REG(glMultiTexCoord1fARB) REG(glMultiTexCoord2fARB) REG(glMultiTexCoord3fARB) REG(glMultiTexCoord4fARB)
  REG(glMultiTexCoord2fvARB) REG(glMultiTexCoord3fvARB) REG(glMultiTexCoord4fvARB)
  REG(glBegin) REG(glEnd)
  REG(glVertex2f) REG(glVertex3f) REG(glVertex4f) REG(glVertex2fv) REG(glVertex3fv) REG(glVertex4fv)
  REG(glVertex2i) REG(glVertex3i) REG(glVertex2d) REG(glVertex3d) REG(glVertex3dv)
  REG(glEnableClientState) REG(glDisableClientState)
  REG(glVertexPointer) REG(glNormalPointer) REG(glColorPointer) REG(glTexCoordPointer) REG(glSecondaryColorPointerEXT)
  REG(glGenBuffersARB) REG(glDeleteBuffersARB) REG(glBindBufferARB) REG(glBufferDataARB) REG(glBufferSubDataARB)
  REG(glMapBufferARB) REG(glUnmapBufferARB) REG(glLockArraysEXT) REG(glUnlockArraysEXT)
  REG(glDrawElements) REG(glDrawRangeElementsEXT) REG(glDrawArrays)
#undef REG
}
