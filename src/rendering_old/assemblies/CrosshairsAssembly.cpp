#include "rendering_old/assemblies/CrosshairsAssembly.h"
#include "rendering/utility/CreateGLObjects.h"
#include "rendering_old/drawables/Crosshairs.h"

#include "common/Exception.hpp"
#include "rendering_old/records/MeshGpuRecord.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace
{

// 2D scene crosshairs have a pointy cone tip that is 10% of the length of
// the total cylinder. They do not have a fixed world-space diameter,
// meaning that they keep constant size as the view is zoomed.
static constexpr double sk_coneToCylinderRatio2d = 0.10;
static constexpr bool sk_isFixedDiameter2d = false;

// 3D scene crosshairs have a fixed world-space diameter,
// so they change size as the view is zoomed.
static constexpr double sk_coneToCylinderRatio3d = 1.0;
static constexpr bool sk_isFixedDiameter3d = true;

} // namespace

CrosshairsAssembly::CrosshairsAssembly(
  ShaderProgramActivatorType shaderProgramActivator, UniformsProviderType uniformsProvider
)
  : m_shaderActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_crosshairs2d(nullptr)
  , m_meshGpuRecord2d(nullptr)
  ,

  m_crosshairs3d(nullptr)
  , m_meshGpuRecord3d(nullptr)
{
}

void CrosshairsAssembly::initialize()
{
  /// @todo put ss name creation in constructor and save as members
  std::ostringstream ss;
  ss << "CrosshairsAssembly_#" << numCreated() << std::ends;

  std::ostringstream ss2D;
  ss2D << ss.str() << "_Crosshairs2D";

  std::ostringstream ss3D;
  ss3D << ss.str() << "_Crosshairs3D";

  // Convert unique_ptr to shared_ptr:
  m_meshGpuRecord2d = gpuhelper::createCrosshairMeshGpuRecord(sk_coneToCylinderRatio2d);
  m_meshGpuRecord3d = gpuhelper::createCrosshairMeshGpuRecord(sk_coneToCylinderRatio3d);

  m_crosshairs2d = std::make_shared<Crosshairs>(
    ss2D.str(), m_shaderActivator, m_uniformsProvider, m_meshGpuRecord2d, sk_isFixedDiameter2d
  );

  m_crosshairs3d = std::make_shared<Crosshairs>(
    ss3D.str(), m_shaderActivator, m_uniformsProvider, m_meshGpuRecord3d, sk_isFixedDiameter3d
  );
}

std::weak_ptr<DrawableBase> CrosshairsAssembly::getRoot(const SceneType& type)
{
  switch (type)
  {
  // All the views with orthonormal camera projections show the "2D crosshairs":
  case SceneType::ReferenceImage2d:
  case SceneType::SlideStack2d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  {
    return std::static_pointer_cast<DrawableBase>(m_crosshairs2d);
  }
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack3d:
  {
    return std::static_pointer_cast<DrawableBase>(m_crosshairs3d);
  }
  default:
  case SceneType::None:
  {
    return {};
  }
  }

  return {};
}

void CrosshairsAssembly::setCrosshairs2dLength(float lengthInMm)
{
  if (m_crosshairs2d)
  {
    m_crosshairs2d->setLength(lengthInMm);
  }
}

void CrosshairsAssembly::setCrosshairs3dLength(float lengthInMm)
{
  if (m_crosshairs3d)
  {
    m_crosshairs3d->setLength(lengthInMm);
  }
}
