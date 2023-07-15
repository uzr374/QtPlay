#pragma once

#include <QMatrix4x4>
#include <QString>

extern "C" {
#include <libavutil/common.h>
#include <libavutil/pixfmt.h>
}

constexpr auto osdVShaderSrc =
    R"(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

constexpr auto osdFShaderSrc =
    R"(
varying vec2 vTexCoord;
uniform sampler2D uTex;

void main()
{
    gl_FragColor = texture2D(uTex, vTexCoord);
}
)";

constexpr auto vShaderSrc =
    R"glsl(
attribute vec2 aPos;
attribute vec2 texC;

varying vec2 texCoord;

void main()
{
	gl_Position = vec4(aPos, 0.0, 1.0);
	texCoord = texC;
}
)glsl";

constexpr auto fShaderSrc =
    R"glsl(
varying vec2 texCoord;

uniform sampler2D Ytex;

#ifdef NV12
uniform sampler2D UVtex;
#else
uniform sampler2D Utex, Vtex;
#endif

uniform mat3 YUVtoRGBmat;
uniform float uBlack;

void main()
{
#ifdef HAS_GL_RG
#ifdef NV12 //NV12 format family(NV12, 24)
	vec3 yuv = vec3(
    texture2D(Ytex, texCoord).r,
    texture2D(UVtex, texCoord).r,
    texture2D(UVtex, texCoord).g
	);
#elif defined NV21 //NV21 format family(NV21, 42)
	vec3 yuv = vec3(
    texture2D(Ytex, texCoord).r,
    texture2D(UVtex, texCoord).g,
    texture2D(UVtex, texCoord).r
	);
#else //YUV formats
	vec3 yuv = vec3(
    texture2D(Ytex, texCoord).r,
    texture2D(Utex, texCoord).r,
    texture2D(Vtex, texCoord).r
	);
#endif
#else
#ifdef NV12 //NV12 format family(NV12, 24)
	vec3 yuv = vec3(
    texture2D(Ytex, texCoord).r,
    texture2D(UVtex, texCoord).r,
    texture2D(UVtex, texCoord).a
	);
#elif defined NV21 //NV21 format family(NV21, 42)
	vec3 yuv = vec3(
    texture2D(Ytex, texCoord).r,
    texture2D(UVtex, texCoord).a,
    texture2D(UVtex, texCoord).r
	);
#else //YUV formats
	vec3 yuv = vec3(
    texture2D(Ytex, texCoord).r,
    texture2D(Utex, texCoord).r,
    texture2D(Vtex, texCoord).r
	);
#endif
#endif

	yuv -= vec3(uBlack, vec2(128.0 / 255.0));

    gl_FragColor = vec4(clamp(YUVtoRGBmat * yuv, 0.0, 1.0), 1.0f);
}
)glsl";

static bool isNV12Family(AVPixelFormat fmt) {
  return fmt == AV_PIX_FMT_NV12 || fmt == AV_PIX_FMT_NV24;
}

static bool isNV21Family(AVPixelFormat fmt) {
  return fmt == AV_PIX_FMT_NV21 || fmt == AV_PIX_FMT_NV42;
}

static bool isNV12or21(AVPixelFormat fmt) {
  return fmt == AV_PIX_FMT_NV12 || fmt == AV_PIX_FMT_NV21;
}

static bool isNV24or42(AVPixelFormat fmt) {
  return fmt == AV_PIX_FMT_NV24 || fmt == AV_PIX_FMT_NV42;
}

struct LumaCoefficients {
  float cR, cG, cB;
};

LumaCoefficients getLumaCoeff(AVColorSpace colorSpace) {
  switch (colorSpace) {
    case AVCOL_SPC_BT709:
      return {0.2126f, 0.7152f, 0.0722f};

    case AVCOL_SPC_SMPTE170M:
      return {0.299f, 0.587f, 0.114f};

    case AVCOL_SPC_SMPTE240M:
      return {0.212f, 0.701f, 0.087f};

    case AVCOL_SPC_BT2020_CL:
    case AVCOL_SPC_BT2020_NCL:
      return {0.2627f, 0.6780f, 0.0593f};

    default:
      break;
  }
  return {0.299f, 0.587f, 0.114f};  // AVCOL_SPC_BT470BG
}

QMatrix4x4 getYUVtoRGBmatrix(AVColorSpace colorSpace, bool limited) {
  const auto lumaCoeff = getLumaCoeff(colorSpace);

  const float bscale = 0.5f / (lumaCoeff.cB - 1.0f);
  const float rscale = 0.5f / (lumaCoeff.cR - 1.0f);

  auto mat = QMatrix4x4(lumaCoeff.cR, lumaCoeff.cG, lumaCoeff.cB, 0.0f,
                        bscale * lumaCoeff.cR, bscale * lumaCoeff.cG, 0.5f,
                        0.0f, 0.5f, rscale * lumaCoeff.cG,
                        rscale * lumaCoeff.cB, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f)
                 .inverted();

  if (limited) mat *= 255.0f / (235.0f - 16.0f);

  return mat;
}

QPointF calculateDisplayRect(int scr_width, int scr_height, int pic_width,
                             int pic_height, AVRational aspect_ratio) {
  if (av_cmp_q(aspect_ratio, av_make_q(0, 1)) <= 0)
    aspect_ratio = av_make_q(1, 1);

  aspect_ratio = av_mul_q(aspect_ratio, av_make_q(pic_width, pic_height));

  /* XXX: we suppose the screen has a 1.0 pixel ratio */
  int64_t height = scr_height;
  int64_t width = av_rescale(height, aspect_ratio.num, aspect_ratio.den) & ~1;

  if (width > scr_width) {
    width = scr_width;
    height = av_rescale(width, aspect_ratio.den, aspect_ratio.num) & ~1;
  }

  const int64_t x = (scr_width - width) / 2;
  const int64_t y = (scr_height - height) / 2;
  width = std::max(width, 1LL);
  height = std::max(height, 1LL);

  const qreal X = width / (double)scr_width;
  const qreal Y = height / (double)scr_height;

  return QPointF(X, Y);
}
