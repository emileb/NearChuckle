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

static void SetAttribArrays(const SImVertex* base, int count)
{
  es_glBindBuffer(ES_ARRAY_BUFFER, 0);
  const GLsizei stride = sizeof(SImVertex);
  es_glEnableVertexAttribArray(GLES_ATTR_POS);
  es_glVertexAttribPointer(GLES_ATTR_POS, 4, GL_FLOAT, GL_FALSE, stride, base->pos);
  es_glEnableVertexAttribArray(GLES_ATTR_NORMAL);
  es_glVertexAttribPointer(GLES_ATTR_NORMAL, 3, GL_FLOAT, GL_FALSE, stride, base->normal);
  es_glEnableVertexAttribArray(GLES_ATTR_COLOR);
  es_glVertexAttribPointer(GLES_ATTR_COLOR, 4, GL_FLOAT, GL_FALSE, stride, base->color);
  es_glEnableVertexAttribArray(GLES_ATTR_COLOR2);
  es_glVertexAttribPointer(GLES_ATTR_COLOR2, 3, GL_FLOAT, GL_FALSE, stride, base->color2);
  for (int i = 0; i < IM_UNITS; i++)
  {
    es_glEnableVertexAttribArray(GLES_ATTR_TEX0 + i);
    es_glVertexAttribPointer(GLES_ATTR_TEX0 + i, 4, GL_FLOAT, GL_FALSE, stride, base->tex[i]);
  }
  for (int i = IM_UNITS; i < GLES_MAX_UNITS; i++)
  {
    es_glDisableVertexAttribArray(GLES_ATTR_TEX0 + i);
    es_glVertexAttrib4f(GLES_ATTR_TEX0 + i, 0, 0, 0, 1);
  }
}

// Quads become an indexed triangle list; the rest map straight onto ES primitives.
static void DrawImmediate()
{
  int n = (int)sIm.size();
  static int logged;
  if (logged < 3) { logged++; GLES_Log("GLES: DrawImmediate mode 0x%x with %d vertices", g_es.beginMode, n); }
  if (n == 0) return;
  GLenum mode = g_es.beginMode;
  if (!GLES_PrepareDraw()) return;
  SetAttribArrays(&sIm[0], n);
  es_glBindBuffer(ES_ELEMENT_ARRAY_BUFFER, 0);
  switch (mode)
  {
    case GL_QUADS:
    {
      sIndices.clear();
      for (int q = 0; q + 3 < n; q += 4)
      {
        sIndices.push_back(q); sIndices.push_back(q+1); sIndices.push_back(q+2);
        sIndices.push_back(q); sIndices.push_back(q+2); sIndices.push_back(q+3);
      }
      es_glDrawElements(GL_TRIANGLES, (GLsizei)sIndices.size(), GL_UNSIGNED_SHORT, &sIndices[0]);
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
  if (target == ES_ARRAY_BUFFER) g_es.arrayBuffer = id;
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
  GLuint id = (target == ES_ARRAY_BUFFER) ? g_es.arrayBuffer : g_es.elementBuffer;
  GLuint size = g_esBuffers[id];
  if (!size) return NULL;
  return es_glMapBufferRange(target, 0, size, ES_MAP_WRITE_BIT);
}
static GLboolean __stdcall gles_glUnmapBufferARB(GLenum target) { return es_glUnmapBuffer(target); }
static void __stdcall gles_glLockArraysEXT(GLint, GLsizei) {}
static void __stdcall gles_glUnlockArraysEXT() {}

static void BindArray(int slot, const SESArray& a, bool normalized)
{
  if (a.enabled && (a.pointer != NULL || a.buffer != 0))
  {
    es_glBindBuffer(ES_ARRAY_BUFFER, a.buffer);
    es_glEnableVertexAttribArray(slot);
    es_glVertexAttribPointer(slot, a.size, a.type, normalized, a.stride, a.pointer);
  }
  else
    es_glDisableVertexAttribArray(slot);
}

static bool SetupClientArrays()
{
  if (!g_es.vertexArray.enabled || (!g_es.vertexArray.pointer && !g_es.vertexArray.buffer)) return false;
  BindArray(GLES_ATTR_POS, g_es.vertexArray, false);
  BindArray(GLES_ATTR_NORMAL, g_es.normalArray, true);
  BindArray(GLES_ATTR_COLOR, g_es.colorArray, true);
  BindArray(GLES_ATTR_COLOR2, g_es.color2Array, true);
  for (int i = 0; i < GLES_MAX_UNITS; i++) BindArray(GLES_ATTR_TEX0 + i, g_es.texcoordArray[i], false);
  if (!g_es.normalArray.enabled) es_glVertexAttrib4f(GLES_ATTR_NORMAL, g_es.normal[0], g_es.normal[1], g_es.normal[2], 0);
  if (!g_es.colorArray.enabled) es_glVertexAttrib4f(GLES_ATTR_COLOR, g_es.color[0], g_es.color[1], g_es.color[2], g_es.color[3]);
  if (!g_es.color2Array.enabled) es_glVertexAttrib4f(GLES_ATTR_COLOR2, g_es.color2[0], g_es.color2[1], g_es.color2[2], 0);
  for (int i = 0; i < GLES_MAX_UNITS; i++)
    if (!g_es.texcoordArray[i].enabled)
      es_glVertexAttrib4f(GLES_ATTR_TEX0 + i, g_es.texcoord[i][0], g_es.texcoord[i][1], g_es.texcoord[i][2], g_es.texcoord[i][3]);
  es_glBindBuffer(ES_ARRAY_BUFFER, g_es.arrayBuffer);
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
