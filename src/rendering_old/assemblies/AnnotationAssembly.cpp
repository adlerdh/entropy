#include "rendering_old/assemblies/AnnotationAssembly.h"

#include "rendering_old/ShaderNames.h"
#include "rendering_old/common/MeshPolygonOffset.h"
#include "rendering_old/drawables/DynamicTransformation.h"
#include "rendering_old/drawables/Transformation.h"
#include "rendering_old/drawables/annotation/AnnotationExtrusion.h"
#include "rendering_old/drawables/annotation/AnnotationSlice.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "common/Exception.hpp"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <sstream>

namespace
{

static const glm::mat4 sk_ident{1.0f};

} // namespace

AnnotationAssembly::AnnotationAssembly(
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, uuids::uuid>
    annotationToWorldTxQuerier,
  QuerierType<std::optional<float>, uuids::uuid> annotationThicknessQuerier
)
  : m_shaderActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_annotationToWorldTxQuerier(annotationToWorldTxQuerier)
  , m_annotationThicknessQuerier(annotationThicknessQuerier)
  ,

  m_rootFor2dViews(nullptr)
  , m_rootFor3dViews(nullptr)
  ,

  m_annotations()
  , m_properties()
{
}

void AnnotationAssembly::initialize()
{
  //    if ( m_meshGpuRecordProvider )
  //    {
  //        m_meshRecord = std::make_shared<MeshRecord>( nullptr, m_meshGpuRecordProvider() );
  //    }
  //    else
  //    {
  //        throw_debug( "Unable to obtain mesh GPU record" );
  //    }

  std::ostringstream ss;
  ss << "AnnotationAssembly_Root2d_#" << numCreated() << std::ends;
  m_rootFor2dViews = std::make_shared<Transformation>(ss.str(), sk_ident);

  ss.str(std::string());
  ss << "AnnotationAssembly_Root3d_#" << numCreated() << std::ends;
  m_rootFor3dViews = std::make_shared<Transformation>(ss.str(), sk_ident);
}

std::weak_ptr<DrawableBase> AnnotationAssembly::getRoot(const SceneType& type)
{
  switch (type)
  {
  case SceneType::ReferenceImage2d:
  case SceneType::SlideStack2d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  {
    return std::static_pointer_cast<DrawableBase>(m_rootFor2dViews);
  }
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack3d:
  {
    return std::static_pointer_cast<DrawableBase>(m_rootFor3dViews);
  }
  default:
  case SceneType::None:
  {
    return {};
  }
  }

  return {};
}

void AnnotationAssembly::setAnnotationToWorldTxQuerier(
  QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, uuids::uuid> querier
)
{
  m_annotationToWorldTxQuerier = querier;
}

void AnnotationAssembly::setAnnotationThicknessQuerier(
  QuerierType<std::optional<float>, uuids::uuid> querier
)
{
  m_annotationThicknessQuerier = querier;
}

void AnnotationAssembly::setAnnotation(std::weak_ptr<SlideAnnotationRecord> annotRecord)
{
  auto record = annotRecord.lock();
  if (!record)
  {
    return;
  }

  // Function that provides the tranformation from annotation to World space
  auto annotToWorldTxProvider = [this, annotRecord]() -> std::optional<glm::mat4>
  {
    auto r = annotRecord.lock();
    if (!r)
    {
      return std::nullopt;
    }

    if (m_annotationToWorldTxQuerier)
    {
      // Get the affine transformation:
      return m_annotationToWorldTxQuerier(r->uid())->first;
    }

    return std::nullopt;
  };

  // Function that provides the slide thickness of the annotation
  auto annotThicknessProvider = [this, annotRecord]() -> std::optional<float>
  {
    auto r = annotRecord.lock();
    if (!r)
    {
      return std::nullopt;
    }

    if (m_annotationThicknessQuerier)
    {
      return m_annotationThicknessQuerier(r->uid());
    }

    return std::nullopt;
  };

  // Function that provides the scaling information for landmark drawables in landmark group
  //    auto annotScalingProvider = [this, &annotRecord] () -> std::optional<DrawableScaling>
  //    {
  //        if ( m_annotationScalingQuerier )
  //        {
  //            return m_annotationScalingQuerier( annotRecord.uid() );
  //        }
  //        return std::nullopt;
  //    };

  //    const auto itr = m_annotations.find( record->uid() );
  //    if ( std::end( m_annotations ) != itr )
  //    {
  // Already exists in the Assembly, so first remove it:
  //        removeAnnotation( record->uid() );
  //    }

  Annotations A;

  A.m_world_O_annot_root2d
    = std::make_shared<DynamicTransformation>("annotTx2d", annotToWorldTxProvider);

  A.m_world_O_annot_root3d
    = std::make_shared<DynamicTransformation>("annotTx3d", annotToWorldTxProvider);

  A.m_annot2d = std::make_shared<AnnotationSlice>(
    "annot2d", m_shaderActivator, m_uniformsProvider, annotToWorldTxProvider, annotRecord
  );

  A.m_annot3d = std::make_shared<AnnotationExtrusion>(
    "annot3d",
    m_shaderActivator,
    m_uniformsProvider,
    annotToWorldTxProvider,
    annotThicknessProvider,
    annotRecord
  );

  A.m_world_O_annot_root2d->addChild(A.m_annot2d);
  A.m_world_O_annot_root3d->addChild(A.m_annot3d);

  // Add point landmarks drawables to main roots
  m_rootFor2dViews->addChild(A.m_world_O_annot_root2d);
  m_rootFor3dViews->addChild(A.m_world_O_annot_root3d);

  // Save the drawables
  m_annotations.emplace(record->uid(), std::move(A));

  updateRenderingProperties();
}

/*
void AnnotationAssembly::removeAnnotation( const UID& annotUid )
{
    const auto it = m_annotations.find( annotUid );
    if ( std::end( m_annotations ) == it )
    {
        std::cerr << "Error: Cannot remove annotation with UID " << annotUid
                  << ". It was not found in AnnotationAssembly." << std::endl;
        return;
    }

    detatchAnnotations( it->second );

    // Erase the drawables for the group
    m_annotations.erase( it );

    updateRenderingProperties();
}


void AnnotationAssembly::clearAnnotations()
{
    for ( auto& annot : m_annotations )
    {
        detatchAnnotations( lmGroup.second );
    }

    m_annotations.clear();

    updateRenderingProperties();
}
*/

/*
void AnnotationAssembly::setAnnotationPoint( const UID& annotUid, const PointRecord<3>& pointRecord )
{
    const auto it = m_landmarks.find( annotUid );
    if ( std::end( m_landmarks ) == it )
    {
        std::cerr << "Error: Cannot set point in annotation with UID " << annotUid
                  << ". It was not found in AnnotationAssembly." << std::endl;
        return;
    }

    // Set 2D and 3D landmark drawable positions
    const auto it2 = it->second.m_landmarks.find( pointRecord.uid() );

    if ( std::end( it->second.m_landmarks ) != it2 )
    {
        if ( auto& lm2d = it2->second.first )
        {
            lm2d->setPosition( pointRecord.getPosition() ); // 2D landmark
        }

        if ( auto& lm3d = it2->second.second )
        {
            lm3d->setPosition( pointRecord.getPosition() ); // 3D landmark
        }
    }
    else
    {
        std::cerr << "Error: Unable to find point " << pointRecord.uid()
                  << " within annotation " << annotUid << " to set." << std::endl;
    }
}
*/

void AnnotationAssembly::setMasterOpacityMultiplier(float multiplier)
{
  m_properties.m_masterOpacityMultiplier = multiplier;
  updateRenderingProperties();
}

void AnnotationAssembly::setVisibleIn2dViews(bool visible)
{
  m_properties.m_visibleIn2dViews = visible;
  updateRenderingProperties();
}

void AnnotationAssembly::setVisibleIn3dViews(bool visible)
{
  m_properties.m_visibleIn3dViews = visible;
  updateRenderingProperties();
}

void AnnotationAssembly::setPickable(bool pickable)
{
  m_properties.m_pickable = pickable;
  updateRenderingProperties();
}

const AnnotationAssemblyRenderingProperties& AnnotationAssembly::getRenderingProperties() const
{
  return m_properties;
}

/*
void AnnotationAssembly::detatchAnnotations( const Landmarks& annot )
{
    // Remove 2D versions of annotations from root
    if ( annot.m_world_O_landmark2d_root && m_rootFor2dViews )
    {
        m_rootFor2dViews->removeChild( *( annot.m_world_O_landmark2d_root ) );
    }

    // Remove 3D versions of annotations from root
    if ( annot.m_world_O_landmark3d_root && m_rootFor3dViews )
    {
        m_rootFor3dViews->removeChild( *( annot.m_world_O_landmark3d_root ) );
    }
}
*/

void AnnotationAssembly::updateRenderingProperties()
{
  const auto& P = m_properties;

  for (auto& annot : m_annotations)
  {
    if (auto& a = annot.second.m_annot2d)
    {
      a->setEnabled(P.m_visibleIn2dViews);
      a->setMasterOpacityMultiplier(P.m_masterOpacityMultiplier);
      a->setPickable(P.m_pickable);
    }

    if (auto& a = annot.second.m_annot3d)
    {
      a->setEnabled(P.m_visibleIn3dViews);
      a->setMasterOpacityMultiplier(P.m_masterOpacityMultiplier);
      a->setPickable(P.m_pickable);
    }
  }
}
