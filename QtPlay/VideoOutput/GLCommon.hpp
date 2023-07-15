#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <algorithm>
#include <array>
#include <memory>
#include <mutex>

#include "../AVWrappers/Frame.hpp"

enum FlipType {
  NO_FLIP = 0,
  FLIP_HORIZONTAL,
  FLIP_VERTICAL,
  FLIP_HV,
  FLIP_COUNT,
};

using TexCoordMat = std::array<GLfloat, 8>;
inline constexpr TexCoordMat g_texCoordMap[FLIP_COUNT] = {
    /* Note: follow the indicies of FlipType enum */
    // Normal display
    {
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
    },

    // HFlip
    {
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
    },

    // VFlip
    {
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
    },

    // HFlip + VFlip
    {
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
    }};

inline void copyTexCoordMat(const TexCoordMat& src, TexCoordMat& dst) {
  std::copy(src.begin(), src.end(), dst.data());
}

inline void permuteTexCoordMap(TexCoordMat& to_permute, FlipType type) {
  switch (type) {
    case FLIP_HORIZONTAL: {
      std::swap(to_permute[0], to_permute[4]);
      std::swap(to_permute[1], to_permute[5]);
      std::swap(to_permute[2], to_permute[6]);
      std::swap(to_permute[3], to_permute[7]);
    } break;

    case FLIP_VERTICAL: {
      std::swap(to_permute[0], to_permute[2]);
      std::swap(to_permute[1], to_permute[3]);
      std::swap(to_permute[4], to_permute[6]);
      std::swap(to_permute[5], to_permute[7]);
    } break;

    case FLIP_HV: {
      permuteTexCoordMap(to_permute, FLIP_VERTICAL);
      permuteTexCoordMap(to_permute, FLIP_HORIZONTAL);
    } break;

    default:
      break;
  }
}

class GLCommon : public QOpenGLFunctions {
 private:
  static constexpr int maxPlaneCount = 4;
  static constexpr auto first_shader_line = "#line 1\n";

  int glVerMinor = 0, glVerMajor = 0, glVer = 0;
  QString glVerStr;

  /* Video rendering context */
  GLint coordLoc = -1, texCoordLoc = -1;
  std::array<GLenum, 4> texFmts{};
  std::unique_ptr<QOpenGLShaderProgram> videoProgram = nullptr;
  QMatrix3x3 YUVtoRGBmat;
  std::array<GLuint, maxPlaneCount> textures{};
  AVRational lastSar = {0, 1};
  int lastW = 0, lastH = 0, lastFmt = -2, windowW = 0, windowH = 0,
      lastColorspace = -2, planeCount = 0;
  bool m_limited = false, nv12 = false;
  double m_zoom = 1.0;

  /* OSD context */
  std::mutex osd_mutex;
  Frame osdPict;
  GLuint osdTexture = 0;
  int lastOSDW = 0, lastOSDH = 0;
  std::unique_ptr<QOpenGLShaderProgram> osdProgram = nullptr;
  int osdTexCoordLoc = -1, osdVerticesLoc = -1;

  /*
  Frame double-buffering.
  Allows to take advantage of parallelism in multi-threaded environment.
  Concept: while one of the frames is being rendered, another is always
  avaliable for operation. Initial state: first frame is writable, second is
  readable. If frame is submitted into the buffer, then frames in pair exchange
  their roles(first_readable flag is negated) and frame_pending flag is set to
  'true'. This introduces some additional latency that is not significant and
  can be ignored.
  */
  std::mutex video_pool_mutex;
  Frame m_framePair[2];
  bool first_readable = false, frame_pending = false, is_opened = false;

 public:
  GLCommon();
  virtual ~GLCommon();

  void zoom(int sign);
  bool setVideoData(const Frame& frame);
  bool setVideoData(Frame&& frame);
  bool pushVideoData(Frame& frame);
  void setOSDImage(const Frame& osd);
  void setOpened(bool opened);

 protected:
  void doUpdateGL();
  bool doInitGL();
  void reset();
  void onResize(int newW, int newH);
  void resetTextures();
  void resetShaderProgram();
  void initTextures(const Frame& frame);
  bool initShaderProgram(const Frame& frame);
  bool compileShaderProgram(const Frame& frame);
  void removeOSD();

 private:
  GLfloat rectVertices[8] = {
      -1.0f, -1.0f,  // 0. Left-bottom
      +1.0f, -1.0f,  // 1. Right-bottom
      -1.0f, +1.0f,  // 2. Left-top
      +1.0f, +1.0f,  // 3. Right-top
  };
};
