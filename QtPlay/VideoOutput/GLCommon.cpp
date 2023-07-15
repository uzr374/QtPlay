#include "GLCommon.hpp"

#include <QOpenGLShaderProgram>

#include "../AVWrappers//Frame.hpp"
#include "GLShaders.hpp"
#include "../Common/QtPlayCommon.hpp"

#pragma comment(lib, "opengl32.lib")

GLCommon::GLCommon() : QOpenGLFunctions(), videoProgram(nullptr) {
  resetTextures();
  resetShaderProgram();
}

GLCommon::~GLCommon() { reset(); }

void GLCommon::setOpened(bool opened) {
  std::scoped_lock vl(video_pool_mutex);
  std::scoped_lock sl(osd_mutex);
  is_opened = opened;
}

void GLCommon::doUpdateGL() {
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  const auto video_pool_idx = [&] {
      std::unique_lock vlck(video_pool_mutex);

      if (!is_opened) {
          return -1;
      }

      if (frame_pending) {
          first_readable = !first_readable;
          frame_pending = false;
      }

      return first_readable ? 0 : 1;
  }();

  if (video_pool_idx < 0) return;

  auto& m_frame = m_framePair[video_pool_idx];

  if (!m_frame.isValid()) return;

  if (lastW != m_frame.width() || lastH != m_frame.height() ||
      m_frame.pixFmt() != lastFmt || av_cmp_q(m_frame.sar(), lastSar)) {
    resetTextures();
    resetShaderProgram();
    lastW = m_frame.width();
    lastH = m_frame.height();
    lastFmt = m_frame.pixFmt();
    lastSar = m_frame.sar();
    initTextures(m_frame);
    initShaderProgram(m_frame);
  }

  if (videoProgram) {
    const auto coord =
        calculateDisplayRect(windowW, windowH, lastW, lastH, lastSar);
    const float fX = coord.x() * m_zoom, fY = coord.y() * m_zoom;

    rectVertices[0] = -fX;
    rectVertices[1] = -fY;
    rectVertices[2] = fX;
    rectVertices[3] = -fY;
    rectVertices[4] = -fX;
    rectVertices[5] = fY;
    rectVertices[6] = fX;
    rectVertices[7] = fY;

    videoProgram->setAttributeArray(coordLoc, rectVertices, 2);
    videoProgram->setAttributeArray(texCoordLoc, g_texCoordMap[NO_FLIP].data(),
                                    2);

    videoProgram->enableAttributeArray(coordLoc);
    videoProgram->enableAttributeArray(texCoordLoc);

    if (lastColorspace != m_frame.colorspace() ||
        m_frame.limited() != m_limited) {
      lastColorspace = m_frame.colorspace();
      m_limited = m_frame.limited();
      YUVtoRGBmat = getYUVtoRGBmatrix(m_frame.colorspace(), m_limited)
                        .toGenericMatrix<3, 3>();

      videoProgram->bind();
      videoProgram->setUniformValue("YUVtoRGBmat", YUVtoRGBmat);
      videoProgram->setUniformValue("uBlack",
                                    m_limited ? 16.0f / 255.0f : 0.0f);
      videoProgram->release();
    }

    if (!m_frame.uploaded) {
      for (int i = 0; i < planeCount; ++i) {
        const int w = m_frame.width(i);
        const int h = m_frame.height(i);
        const int linesize = m_frame.linesize(i);
        bool correctLinesize = false;

        if (nv12 && i != 0) {
          correctLinesize =
              ((m_frame.linesize(0) >> m_frame.chromaShiftW()) == w);
        } else {
          correctLinesize = (w == linesize);
        }

        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);

        if (correctLinesize) {
          glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, texFmts[i],
                          GL_UNSIGNED_BYTE, m_frame.data(i));
        } else {
          const uint8_t* data = m_frame.data(i);
          for (int row = 0; row < h; ++row) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, w, 1, texFmts[i],
                            GL_UNSIGNED_BYTE, data);
            data += linesize;
          }
        }
      }

      m_frame.uploaded = true;
    } else {
      for (int i = 0; i < planeCount; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
      }
    }

    if (videoProgram->bind()) {
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      videoProgram->release();
    }

    videoProgram->disableAttributeArray(coordLoc);
    videoProgram->disableAttributeArray(texCoordLoc);

    for (int i = 0; i < planeCount; ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, 0);
    }

    std::scoped_lock osdl(osd_mutex);
    if (osdPict.isValid()) {
      if (!osdTexture) {
        glGenTextures(1, &osdTexture);
      }

      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_2D, osdTexture);

      if (!osdPict.uploaded) {
        if (osdPict.width() != lastOSDW || osdPict.height() != lastOSDH) {
          glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

          // set the texture wrapping parameters
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
          // set texture filtering parameters
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          // allocate texture and buffers
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, osdPict.width(),
                       osdPict.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
          lastOSDW = osdPict.width();
          lastOSDH = osdPict.height();
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, osdPict.width(),
                        osdPict.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                        osdPict.data(0));

        osdPict.uploaded = true;
      }

      auto osdTexCoords = g_texCoordMap[NO_FLIP];
      osdProgram->setAttributeArray(osdVerticesLoc, rectVertices, 2);
      osdProgram->setAttributeArray(osdTexCoordLoc, osdTexCoords.data(), 2);

      osdProgram->enableAttributeArray(osdVerticesLoc);
      osdProgram->enableAttributeArray(osdTexCoordLoc);

      glEnable(GL_BLEND);
      if (osdProgram->bind()) {
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        osdProgram->release();
      }
      glDisable(GL_BLEND);

      osdProgram->disableAttributeArray(osdVerticesLoc);
      osdProgram->disableAttributeArray(osdTexCoordLoc);

      glBindTexture(GL_TEXTURE_2D, 0);
    }
  }
}

bool GLCommon::doInitGL() {
  initializeOpenGLFunctions();

  bool isOk = false;
  const QString vshaderSrc = QString(osdVShaderSrc).prepend(first_shader_line);
  const QString fshaderSrc = QString(osdFShaderSrc).prepend(first_shader_line);

  osdProgram = std::make_unique<QOpenGLShaderProgram>();
  osdProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vshaderSrc);
  osdProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fshaderSrc);

  if (osdProgram->link()) {
    if (osdProgram->bind()) {
      osdProgram->setUniformValue("uTex", 3);
      osdVerticesLoc = osdProgram->attributeLocation("aPosition");
      osdTexCoordLoc = osdProgram->attributeLocation("aTexCoord");
      osdProgram->release();
      isOk = true;
    }
  }

  /* Set OpenGL parameters */
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_DITHER);

  glGetIntegerv(GL_MAJOR_VERSION, &glVerMajor);
  glGetIntegerv(GL_MINOR_VERSION, &glVerMinor);

  if (!glVerMajor) {
    glVerStr = (const char*)glGetString(GL_VERSION);
    const int dotIdx = glVerStr.indexOf('.');
    if (dotIdx > 0) {
      const int vIdx = glVerStr.lastIndexOf(' ', dotIdx);
      if (std::sscanf(glVerStr.mid(vIdx < 0 ? 0 : vIdx).toLatin1().constData(),
                      "%d.%d", &glVerMajor, &glVerMinor) != 2) {
        glVerMajor = glVerMinor = 0;
      }
    }
  }

  if (glVerMajor) {
    glVer = glVerMajor * 10 + glVerMinor;
    glVer *= 10;
  }

  return isOk;
}

bool GLCommon::setVideoData(const Frame& frame) {
  std::scoped_lock vl(video_pool_mutex);
  if (!is_opened) return false;
  const auto uploaded = !frame_pending;
  m_framePair[first_readable ? 1 : 0] = frame;
  frame_pending = true;

  return uploaded;
}

bool GLCommon::setVideoData(Frame&& frame) {
  std::scoped_lock vl(video_pool_mutex);
  if (!is_opened) return false;
  const auto uploaded = !frame_pending;
  m_framePair[first_readable ? 1 : 0] = std::move(frame);
  frame_pending = true;

  return uploaded;
}

void GLCommon::resetTextures() {
  if (planeCount > 0) {
    glDeleteTextures(planeCount, textures.data());
  }

  std::fill(texFmts.begin(), texFmts.end(), 0);
  std::fill(textures.begin(), textures.end(), 0);

  planeCount = 0;
  lastW = lastH = 0;
  lastSar = {0, 1};
  lastColorspace = -2;
  lastFmt = -2;
  m_limited = false;
}

void GLCommon::resetShaderProgram() {
  videoProgram = nullptr;
  planeCount = 0;
  lastW = lastH = 0;
  lastSar = {0, 1};
  lastColorspace = -2;
  m_limited = false;
  YUVtoRGBmat = QMatrix3x3();
  lastOSDW = lastOSDH = 0;
  if (osdTexture) {
    glDeleteTextures(1, &osdTexture);
  }
}

void GLCommon::reset() {
  resetTextures();
  resetShaderProgram();
  setOpened(false);
  for (auto& fr : m_framePair) fr.clear();
  osdPict.clear();
  first_readable = frame_pending = false;
}

void GLCommon::onResize(int newW, int newH) {
  windowW = newW;
  windowH = newH;
}

void GLCommon::zoom(int sign) {
  constexpr float zoomMin = 0.5f;
  constexpr float zoomMax = 2.0f;
  constexpr float zoomStep = 0.01f;

  if ((m_zoom >= zoomMax && sign > 0) || (m_zoom <= zoomMin && sign < 0)) {
    return;
  }

  if (sign == 0) {
    m_zoom = 1.0f;
  } else {
    m_zoom += zoomStep * sign;
  }
}

void GLCommon::initTextures(const Frame& frame) {
  const auto pixFmt = frame.pixFmt();
  nv12 = isNV12Family(pixFmt) || isNV21Family(pixFmt);
  std::fill(texFmts.begin(), texFmts.end(), 0);
  planeCount = nv12 ? 2 : 3;
  for (int i = 0; i < planeCount; ++i) {
    if (nv12 && (i != 0)) {
#ifdef GL_RG
      texFmts[i] = GL_RG;
#elif defined GL_LUMINANCE_ALPHA
      texFmts[i] = GL_LUMINANCE_ALPHA;
#endif
    } else {
#ifdef GL_RED
      texFmts[i] = GL_RED;
#elif defined GL_LUMINANCE
      texFmts[i] = GL_LUMINANCE;
#endif
    }
  }

  glGenTextures(planeCount, textures.data());

  for (int i = 0; i < planeCount; ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, textures[i]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // allocate texture and buffers

    glTexImage2D(GL_TEXTURE_2D, 0, texFmts[i], frame.width(i), frame.height(i),
                 0, texFmts[i], GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

bool GLCommon::initShaderProgram(const Frame& frame) {
  nv12 = isNV12Family(frame.pixFmt()) || isNV21Family(frame.pixFmt());
  compileShaderProgram(frame);
  if (videoProgram->bind()) {
    coordLoc = videoProgram->attributeLocation("aPos");
    texCoordLoc = videoProgram->attributeLocation("texC");
    videoProgram->setAttributeArray(coordLoc, rectVertices, 2);
    videoProgram->setAttributeArray(texCoordLoc, g_texCoordMap[NO_FLIP].data(),
                                    2);
    videoProgram->release();
    return true;
  }

  return false;
}

bool GLCommon::compileShaderProgram(const Frame& frame) {
  bool isOk = false;
  const auto vshaderSrc = QString(::vShaderSrc).prepend(first_shader_line);
  auto fshaderSrc = QString(::fShaderSrc);

  if (nv12) {
    if (isNV12Family(frame.pixFmt()))
      fshaderSrc.prepend("#define NV12\n");
    else if (isNV21Family(frame.pixFmt()))
      fshaderSrc.prepend("#define NV21\n");
  }

#ifdef GL_RG
  fshaderSrc.prepend("#define HAS_GL_RG\n");
#endif
  fshaderSrc.prepend(first_shader_line);

  videoProgram = std::make_unique<QOpenGLShaderProgram>();
  videoProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vshaderSrc);
  videoProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fshaderSrc);

  if (videoProgram->link()) {
    if (videoProgram->bind()) {
      videoProgram->setUniformValue("Ytex", 0);

      if (nv12) {
        videoProgram->setUniformValue("UVtex", 1);
      } else {
        videoProgram->setUniformValue("Utex", 1);
        videoProgram->setUniformValue("Vtex", 2);
      }
      videoProgram->release();
      isOk = true;
    }
  }

  return isOk;
}

void GLCommon::setOSDImage(const Frame& osd) {
  std::scoped_lock sl(osd_mutex);
  if (!is_opened) return;
  osdPict = osd;
}

void GLCommon::removeOSD() {
  std::scoped_lock sl(osd_mutex);
  osdPict.clear();
}

bool GLCommon::pushVideoData(Frame& fr) {
  if (!is_opened) return false;
  const auto uploaded = !frame_pending;
  fr.moveTo(m_framePair[first_readable ? 1 : 0]);
  frame_pending = true;

  return uploaded;
}
