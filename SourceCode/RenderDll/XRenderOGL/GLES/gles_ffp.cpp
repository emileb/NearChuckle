/*=============================================================================
  gles_ffp.cpp : fixed-function emulation. Generates a GLSL ES 3.00 program
  for the current texenv / alpha test / fog state and binds it before a draw.
=============================================================================*/
#include "gles_internal.h"
#include <stdio.h>

enum { UNIT_OFF = 0, UNIT_2D, UNIT_RECT, UNIT_CUBE, UNIT_3D };

struct SFFPKey
{
  unsigned char unitType[GLES_MAX_UNITS];
  unsigned char texMatrix[GLES_MAX_UNITS];
  STexEnv env[GLES_MAX_UNITS];
  unsigned char alphaTest, alphaFunc;
  unsigned char fogMode;   // 0 off, 1 linear, 2 exp, 3 exp2

  bool operator<(const SFFPKey& o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};

struct SFFPProgram
{
  GLuint prog;
  GLint uMVP, uTexMat[GLES_MAX_UNITS], uEnvColor[GLES_MAX_UNITS], uRectScale[GLES_MAX_UNITS];
  GLint uAlphaRef, uFogColor, uFogParams, uMV;
};

static std::map<SFFPKey, SFFPProgram> sPrograms;
static SFFPProgram* sCurrent;
static GLuint sCurrentProg;

static const char* UnitSampler(int type)
{
  switch (type) { case UNIT_CUBE: return "samplerCube"; case UNIT_3D: return "sampler3D"; default: return "sampler2D"; }
}

static void AppendArg(std::string& s, const SFFPKey& k, int unit, GLenum src, GLenum op, bool alpha)
{
  char buf[64];
  std::string v;
  if (src == GL_TEXTURE) { sprintf(buf, "t%d", unit); v = buf; }
  else if (src >= ES_TEXTURE0 && src < ES_TEXTURE0 + GLES_MAX_UNITS) { sprintf(buf, "t%d", (int)(src - ES_TEXTURE0)); v = buf; }
  else if (src == ES_CONSTANT) { sprintf(buf, "u_envColor[%d]", unit); v = buf; }
  else if (src == ES_PRIMARY_COLOR) v = "v_color";
  else v = "prev"; // GL_PREVIOUS
  switch (op)
  {
    case GL_SRC_COLOR:           s += alpha ? v + ".a" : v + ".rgb"; break;
    case GL_ONE_MINUS_SRC_COLOR: s += alpha ? "(1.0-" + v + ".a)" : "(vec3(1.0)-" + v + ".rgb)"; break;
    case GL_SRC_ALPHA:           s += alpha ? v + ".a" : "vec3(" + v + ".a)"; break;
    case GL_ONE_MINUS_SRC_ALPHA: s += alpha ? "(1.0-" + v + ".a)" : "vec3(1.0-" + v + ".a)"; break;
    default:                     s += alpha ? v + ".a" : v + ".rgb"; break;
  }
}

static void AppendCombine(std::string& s, const SFFPKey& k, int unit, GLenum func, const GLenum* src, const GLenum* op, float scale, bool alpha)
{
  std::string a[3];
  for (int i = 0; i < 3; i++) AppendArg(a[i], k, unit, src[i], op[i], alpha);
  std::string r;
  switch (func)
  {
    case GL_REPLACE:      r = a[0]; break;
    case GL_MODULATE:     r = a[0] + "*" + a[1]; break;
    case GL_ADD:          r = a[0] + "+" + a[1]; break;
    case ES_ADD_SIGNED:   r = a[0] + "+" + a[1] + (alpha ? "-0.5" : "-vec3(0.5)"); break;
    case ES_INTERPOLATE:  r = a[0] + "*" + a[2] + "+" + a[1] + "*" + (alpha ? "(1.0-" + a[2] + ")" : "(vec3(1.0)-" + a[2] + ")"); break;
    case ES_SUBTRACT:     r = a[0] + "-" + a[1]; break;
    case ES_DOT3_RGB: case ES_DOT3_RGBA:
      r = alpha ? a[0] : "vec3(4.0*dot(" + a[0] + "-vec3(0.5)," + a[1] + "-vec3(0.5)))"; break;
    default:              r = a[0] + "*" + a[1]; break;
  }
  char buf[32];
  if (scale != 1.0f) { sprintf(buf, "*%.1f", scale); r = "(" + r + ")" + buf; }
  s += r;
}

static std::string BuildVertexShader(const SFFPKey& k)
{
  std::string s = "#version 300 es\n";
  s += "in vec4 a_pos; in vec4 a_color; in vec3 a_normal; in vec3 a_color2;\n";
  s += "uniform mat4 u_mvp; uniform mat4 u_mv;\n";
  s += "out vec4 v_color; out vec3 v_color2; out float v_eyez;\n";
  char buf[256];
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    if (k.unitType[i] == UNIT_OFF) continue;
    sprintf(buf, "in vec4 a_tex%d; out vec4 v_tex%d; uniform mat4 u_texMat%d;\n", i, i, i);
    s += buf;
  }
  s += "void main(){\n gl_Position = u_mvp * a_pos;\n v_color = a_color; v_color2 = a_color2;\n";
  s += " v_eyez = -(u_mv * a_pos).z;\n";
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    if (k.unitType[i] == UNIT_OFF) continue;
    if (k.texMatrix[i]) sprintf(buf, " v_tex%d = u_texMat%d * a_tex%d;\n", i, i, i);
    else sprintf(buf, " v_tex%d = a_tex%d;\n", i, i);
    s += buf;
  }
  s += "}\n";
  return s;
}

static std::string BuildFragmentShader(const SFFPKey& k)
{
  std::string s = "#version 300 es\nprecision highp float;\nprecision highp sampler3D;\n";
  s += "in vec4 v_color; in vec3 v_color2; in float v_eyez;\n";
  s += "uniform vec4 u_envColor[8]; uniform vec2 u_rectScale[8]; uniform float u_alphaRef;\n";
  s += "uniform vec4 u_fogColor; uniform vec4 u_fogParams;\nout vec4 fragColor;\n";
  char buf[256];
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    if (k.unitType[i] == UNIT_OFF) continue;
    sprintf(buf, "in vec4 v_tex%d; uniform %s u_tex%d;\n", i, UnitSampler(k.unitType[i]), i);
    s += buf;
  }
  s += "void main(){\n vec4 prev = v_color;\n";
  // Sample every enabled unit first so crossbar sources (TEXTUREn) can reference them.
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    switch (k.unitType[i])
    {
      case UNIT_2D:   sprintf(buf, " vec4 t%d = textureProj(u_tex%d, vec3(v_tex%d.xy, v_tex%d.w));\n", i, i, i, i); break;
      case UNIT_RECT: sprintf(buf, " vec4 t%d = texture(u_tex%d, v_tex%d.xy / v_tex%d.w * u_rectScale[%d]);\n", i, i, i, i, i); break;
      case UNIT_CUBE: sprintf(buf, " vec4 t%d = texture(u_tex%d, v_tex%d.xyz);\n", i, i, i); break;
      case UNIT_3D:   sprintf(buf, " vec4 t%d = texture(u_tex%d, v_tex%d.xyz);\n", i, i, i); break;
      default: continue;
    }
    s += buf;
  }
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    if (k.unitType[i] == UNIT_OFF) continue;
    const STexEnv& e = k.env[i];
    sprintf(buf, "t%d", i);
    std::string t = buf;
    std::string rgb, a;
    if (e.mode == ES_COMBINE)
    {
      AppendCombine(rgb, k, i, e.combineRGB, e.srcRGB, e.opRGB, e.scaleRGB, false);
      if (e.combineRGB == ES_DOT3_RGBA) a = "(" + rgb + ").r";
      else AppendCombine(a, k, i, e.combineAlpha, e.srcAlpha, e.opAlpha, e.scaleAlpha, true);
    }
    else switch (e.mode)
    {
      case GL_REPLACE: rgb = t + ".rgb"; a = t + ".a"; break;
      case GL_DECAL:   rgb = "mix(prev.rgb, " + t + ".rgb, " + t + ".a)"; a = "prev.a"; break;
      case GL_BLEND:   sprintf(buf, "mix(prev.rgb, u_envColor[%d].rgb, %s.rgb)", i, t.c_str()); rgb = buf; a = "prev.a*" + t + ".a"; break;
      case GL_ADD:     rgb = "prev.rgb+" + t + ".rgb"; a = "prev.a*" + t + ".a"; break;
      default:         rgb = "prev.rgb*" + t + ".rgb"; a = "prev.a*" + t + ".a"; break; // MODULATE
    }
    s += " prev = clamp(vec4(" + rgb + ", " + a + "), 0.0, 1.0);\n";
  }
  s += " vec4 c = prev;\n";
  if (k.fogMode)
  {
    if (k.fogMode == 1) s += " float f = clamp((u_fogParams.y - v_eyez) * u_fogParams.z, 0.0, 1.0);\n";
    else if (k.fogMode == 2) s += " float f = clamp(exp(-u_fogParams.w * v_eyez), 0.0, 1.0);\n";
    else s += " float f = clamp(exp(-u_fogParams.w * u_fogParams.w * v_eyez * v_eyez), 0.0, 1.0);\n";
    s += " c.rgb = mix(u_fogColor.rgb, c.rgb, f);\n";
  }
  if (k.alphaTest)
  {
    const char* cmp = NULL;
    switch (k.alphaFunc)
    {
      case GL_NEVER: cmp = "false"; break;
      case GL_LESS: cmp = "c.a < u_alphaRef"; break;
      case GL_EQUAL: cmp = "c.a == u_alphaRef"; break;
      case GL_LEQUAL: cmp = "c.a <= u_alphaRef"; break;
      case GL_GREATER: cmp = "c.a > u_alphaRef"; break;
      case GL_NOTEQUAL: cmp = "c.a != u_alphaRef"; break;
      case GL_GEQUAL: cmp = "c.a >= u_alphaRef"; break;
      default: cmp = "true"; break;
    }
    s += std::string(" if (!(") + cmp + ")) discard;\n";
  }
  s += " fragColor = c;\n}\n";
  return s;
}

static GLuint Compile(GLenum type, const std::string& src)
{
  GLuint sh = es_glCreateShader(type);
  const char* p = src.c_str();
  es_glShaderSource(sh, 1, &p, NULL);
  es_glCompileShader(sh);
  GLint ok = 0;
  es_glGetShaderiv(sh, ES_COMPILE_STATUS, &ok);
  if (!ok)
  {
    char log[2048];
    es_glGetShaderInfoLog(sh, sizeof(log), NULL, log);
    GLES_Log("GLES: FFP %s shader failed:\n%s\n--- source ---\n%s", type == ES_VERTEX_SHADER ? "vertex" : "fragment", log, src.c_str());
  }
  return sh;
}

static SFFPProgram Build(const SFFPKey& k)
{
  SFFPProgram p;
  memset(&p, 0, sizeof(p));
  std::string vsrc = BuildVertexShader(k), fsrc = BuildFragmentShader(k);
  static bool debug = getenv("FARCRY_GLES_DEBUG") != NULL;
  if (debug) GLES_Log("GLES: FFP program #%d sources:\n%s\n%s", (int)sPrograms.size() + 1, vsrc.c_str(), fsrc.c_str());
  GLuint vs = Compile(ES_VERTEX_SHADER, vsrc);
  GLuint fs = Compile(ES_FRAGMENT_SHADER, fsrc);
  p.prog = es_glCreateProgram();
  es_glAttachShader(p.prog, vs);
  es_glAttachShader(p.prog, fs);
  es_glBindAttribLocation(p.prog, GLES_ATTR_POS, "a_pos");
  es_glBindAttribLocation(p.prog, GLES_ATTR_NORMAL, "a_normal");
  es_glBindAttribLocation(p.prog, GLES_ATTR_COLOR, "a_color");
  es_glBindAttribLocation(p.prog, GLES_ATTR_COLOR2, "a_color2");
  char buf[32];
  for (int i = 0; i < GLES_MAX_UNITS; i++) { sprintf(buf, "a_tex%d", i); es_glBindAttribLocation(p.prog, GLES_ATTR_TEX0 + i, buf); }
  es_glLinkProgram(p.prog);
  GLint ok = 0;
  es_glGetProgramiv(p.prog, ES_LINK_STATUS, &ok);
  if (!ok)
  {
    char log[2048];
    es_glGetProgramInfoLog(p.prog, sizeof(log), NULL, log);
    GLES_Log("GLES: FFP program link failed: %s", log);
  }
  es_glDeleteShader(vs);
  es_glDeleteShader(fs);
  es_glUseProgram(p.prog);
  sCurrentProg = p.prog;
  p.uMVP = es_glGetUniformLocation(p.prog, "u_mvp");
  p.uMV = es_glGetUniformLocation(p.prog, "u_mv");
  p.uAlphaRef = es_glGetUniformLocation(p.prog, "u_alphaRef");
  p.uFogColor = es_glGetUniformLocation(p.prog, "u_fogColor");
  p.uFogParams = es_glGetUniformLocation(p.prog, "u_fogParams");
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    sprintf(buf, "u_texMat%d", i); p.uTexMat[i] = es_glGetUniformLocation(p.prog, buf);
    sprintf(buf, "u_envColor[%d]", i); p.uEnvColor[i] = es_glGetUniformLocation(p.prog, buf);
    sprintf(buf, "u_rectScale[%d]", i); p.uRectScale[i] = es_glGetUniformLocation(p.prog, buf);
    sprintf(buf, "u_tex%d", i);
    GLint loc = es_glGetUniformLocation(p.prog, buf);
    if (loc >= 0) es_glUniform1i(loc, i);
  }
  return p;
}

static void BuildKey(SFFPKey& k)
{
  memset(&k, 0, sizeof(k));
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    GLenum target = 0;
    STextureObj* o = GLES_BoundTexture(i, target);
    if (!o) continue;
    switch (target)
    {
      case ES_TEXTURE_CUBE_MAP: k.unitType[i] = UNIT_CUBE; break;
      case ES_TEXTURE_3D: k.unitType[i] = UNIT_3D; break;
      case ES_TEXTURE_RECTANGLE_NV: k.unitType[i] = UNIT_RECT; break;
      default: k.unitType[i] = UNIT_2D; break;
    }
    k.texMatrix[i] = g_es.unit[i].texMatrixIdentity ? 0 : 1;
    k.env[i] = g_es.unit[i].env;
    memset(k.env[i].color, 0, sizeof(k.env[i].color)); // uniform, not part of the key
  }
  k.alphaTest = g_es.alphaTest && g_es.alphaFunc != GL_ALWAYS;
  k.alphaFunc = (unsigned char)(g_es.alphaFunc - GL_NEVER);
  if (g_es.fog) k.fogMode = (g_es.fogMode == GL_LINEAR) ? 1 : (g_es.fogMode == GL_EXP) ? 2 : 3;
}

bool GLES_PrepareDraw()
{
  static bool loggedARB;
  if ((g_es.vertexProgram || g_es.fragmentProgram) && !loggedARB)
  {
    loggedARB = true;
    GLES_Log("GLES: ARB programs are not translated yet; drawing with fixed function");
  }

  SFFPKey k;
  BuildKey(k);
  std::map<SFFPKey, SFFPProgram>::iterator it = sPrograms.find(k);
  if (it == sPrograms.end())
  {
    it = sPrograms.insert(std::make_pair(k, Build(k))).first;
    GLES_Log("GLES: FFP program #%d built", (int)sPrograms.size());
  }
  SFFPProgram& p = it->second;
  if (sCurrentProg != p.prog) { es_glUseProgram(p.prog); sCurrentProg = p.prog; }

  es_glUniformMatrix4fv(p.uMVP, 1, GL_FALSE, GLES_CurrentMVP());
  if (p.uMV >= 0) es_glUniformMatrix4fv(p.uMV, 1, GL_FALSE, g_es.modelview.m[g_es.modelview.depth]);
  if (k.alphaTest) es_glUniform1f(p.uAlphaRef, g_es.alphaRef);
  if (k.fogMode)
  {
    float range = g_es.fogEnd - g_es.fogStart;
    float params[4] = { g_es.fogStart, g_es.fogEnd, range != 0.0f ? 1.0f / range : 1.0f, g_es.fogDensity };
    es_glUniform4fv(p.uFogColor, 1, g_es.fogColor);
    es_glUniform4fv(p.uFogParams, 1, params);
  }
  for (int i = 0; i < GLES_MAX_UNITS; i++)
  {
    if (k.unitType[i] == UNIT_OFF) continue;
    GLenum target = 0;
    STextureObj* o = GLES_BoundTexture(i, target);
    es_glActiveTexture(ES_TEXTURE0 + i);
    es_glBindTexture(o->target, o->id);
    if (p.uEnvColor[i] >= 0) es_glUniform4fv(p.uEnvColor[i], 1, g_es.unit[i].env.color);
    if (k.texMatrix[i] && p.uTexMat[i] >= 0) es_glUniformMatrix4fv(p.uTexMat[i], 1, GL_FALSE, g_es.unit[i].texMatrix);
    if (k.unitType[i] == UNIT_RECT && p.uRectScale[i] >= 0)
      es_glUniform2f(p.uRectScale[i], o->width ? 1.0f / o->width : 1.0f, o->height ? 1.0f / o->height : 1.0f);
  }
  es_glActiveTexture(ES_TEXTURE0 + g_es.activeUnit);
  static bool debug = getenv("FARCRY_GLES_DEBUG") != NULL;
  if (debug)
  {
    static int errors;
    GLenum err = es_glGetError();
    if (err && errors++ < 20) GLES_Log("GLES: GL error 0x%x before draw (program #%d)", err, (int)std::distance(sPrograms.begin(), it) + 1);
  }
  return true;
}

void GLES_RegisterFFP(std::map<std::string, void*>&) {}
