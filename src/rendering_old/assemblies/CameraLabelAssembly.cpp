#include "rendering_old/assemblies/CameraLabelAssembly.h"
#include "rendering_old/drawables/overlays/CameraLabel.h"

#include "rendering/utility/gl/GLTexture.h"

//#include <QImage>
//#include <QPixmap>

#include <sstream>
#include <vector>

namespace
{

/**
 * @brief Create shared pointer to texture from 2D image with pre-multiplied RGBA format,
 * uint8_t per pixel component.
 *
 * @param size Size of image
 * @param data Pointer to image buffer
 *
 * @return Shared pointer to texture
 */
std::shared_ptr<GLTexture> createTexture2d(const glm::i64vec2& size, const void* data)
{
  auto texture = std::make_shared<GLTexture>(tex::Target::Texture2D);
  texture->generate();

  texture->setSize(glm::uvec3{size.x, size.y, 1});

  texture->setData(
    0,
    tex::SizedInternalFormat::RGBA8_UNorm,
    tex::BufferPixelFormat::RGBA,
    tex::BufferPixelDataType::UInt8,
    data
  );

  //    static const glm::vec4 sk_transparentBlack{ 0.0f, 0.0f, 0.0f, 0.0f };
  //    texture->setBorderColor( sk_transparentBlack );
  //    texture->setWrapMode( tex::WrapMode::ClampToBorder );

  // Clamp to edge, since clamping to black border will change the color of the slide edges
  texture->setWrapMode(tex::WrapMode::ClampToEdge);

  texture->setAutoGenerateMipmaps(true);
  texture->setMinificationFilter(tex::MinificationFilter::Linear);
  texture->setMagnificationFilter(tex::MagnificationFilter::Linear);

  return texture;
}

} // namespace

const std::string CameraLabelAssembly::smk_lettersImagePath(":/letters/");
const std::array<std::string, 6> CameraLabelAssembly::smk_labels{"S", "P", "I", "R", "A", "L"};

CameraLabelAssembly::CameraLabelAssembly(
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  GetterType<glm::mat4> activeSubjectToWorldProvider
)
  : m_shaderActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_activeSubjectToWorldProvider(activeSubjectToWorldProvider)
  ,

  m_root(nullptr)
{
}

void CameraLabelAssembly::initialize()
{
  std::array<std::weak_ptr<GLTexture>, 6> weakLetterTextures;

  for (uint i = 0; i < 6; ++i)
  {
    const auto& label = smk_labels[i];

    std::ostringstream ss;
    ss << smk_lettersImagePath << label << ".png";

    QPixmap pixmap(ss.str().c_str());
    const QImage image = pixmap.toImage().convertToFormat(
      QImage::Format::Format_RGBA8888_Premultiplied
    );
    const glm::i64vec2 dims{image.size().width(), image.size().height()};

    m_letterTextures[i] = createTexture2d(dims, image.bits());
    weakLetterTextures[i] = m_letterTextures[i];
  }

  std::ostringstream ss;
  ss << "CameraLabelAssembly_#" << numCreated() << std::ends;

  m_root = std::make_shared<CameraLabel>(
    ss.str(), m_shaderActivator, m_uniformsProvider, m_activeSubjectToWorldProvider, weakLetterTextures
  );
}

void CameraLabelAssembly::setActiveSubjectToWorldProvider(
  GetterType<std::optional<glm::mat4> > provider
)
{
  if (m_root)
  {
    m_root->setSubjectToWorldProvider(provider);
  }
}

std::weak_ptr<DrawableBase> CameraLabelAssembly::getRoot(const SceneType& type)
{
  switch (type)
  {
  case SceneType::ReferenceImage2d:
  case SceneType::SlideStack2d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack3d:
  {
    return std::static_pointer_cast<DrawableBase>(m_root);
  }
  default:
  case SceneType::None:
  {
    return {};
  }
  }
}
