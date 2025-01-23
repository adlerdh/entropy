#include "logic_old/annotation/AnnotationHelper.h"
#include "logic/managers/DataManager.h"
#include "logic_old/annotation/PlanarPolygon.h"
#include "logic_old/annotation/SlideAnnotationCpuRecord.h"

#include "externals/earcut/include/mapbox/earcut.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <iostream>
#include <list>
#include <utility>

namespace mapbox
{
namespace util
{

template<>
struct nth<0, PlanarPolygon::PointType>
{
  inline static auto get(const PlanarPolygon::PointType& point) { return point[0]; }
};

template<>
struct nth<1, PlanarPolygon::PointType>
{
  inline static auto get(const PlanarPolygon::PointType& point) { return point[1]; }
};

} // namespace util
} // namespace mapbox

void triangulatePolygon(PlanarPolygon& polygon)
{
  polygon.setTriangulation(mapbox::earcut<PlanarPolygon::IndexType>(polygon.getAllVertices()));
}

void setUniqueSlideAnnotationLayers(DataManager& dataManager)
{
  // Pair consisting of annotation UID and its layer
  using AnnotUidAndLayer = std::pair<UID, int>;

  // Loop over all slides
  for (auto slideUid : dataManager.orderedSlideUids())
  {
    std::cout << "Slide " << slideUid << std::endl;

    // Create list of all annotations for this slide, ordered by their layer
    std::list<AnnotUidAndLayer> orderedAnnotations;

    // Loop in order of annotations for the slide
    for (auto annotUid : dataManager.orderedSlideAnnotationUids(slideUid))
    {
      auto annot = dataManager.slideAnnotationRecord(annotUid);
      auto r = annot.lock();
      if (r && r->cpuData())
      {
        std::cout << "\tAnnot " << annotUid << ", orig layer = " << r->cpuData()->getLayer()
                  << std::endl;

        // Note: there is no guarantee that layers are unique
        orderedAnnotations.push_back(std::make_pair(r->uid(), r->cpuData()->getLayer()));
      }
    }

    // Sort annotation UIDs based on layer
    orderedAnnotations.sort([](const AnnotUidAndLayer& a, const AnnotUidAndLayer& b)
                            { return (a.second < b.second); });

    // Reassign unique layers, starting at 0:
    uint32_t layer = 0;
    const uint32_t maxLayer = orderedAnnotations.size() - 1;

    for (const auto& a : orderedAnnotations)
    {
      auto r = dataManager.slideAnnotationRecord(a.first).lock();
      if (r && r->cpuData())
      {
        std::cout << "\t\tAnnot " << a.first << ", new layer = " << layer << std::endl;

        r->cpuData()->setLayer(layer);
        r->cpuData()->setMaxLayer(maxLayer);
        ++layer;
      }
    }
  }
}

void changeSlideAnnotationLayering(
  DataManager& dataManager, const UID& slideAnnotUid, const LayerChangeType& layerChange
)
{
  // Pair consisting of annotation UID and its layer
  using AnnotUidAndLayer = std::pair<UID, int>;

  // First assign unique layers to all annotations
  setUniqueSlideAnnotationLayers(dataManager);

  auto slideUid = dataManager.slideUid_of_annotation(slideAnnotUid);
  if (!slideUid)
  {
    std::cerr << "Error: No slide associated with annotatation " << slideAnnotUid << std::endl;
    return;
  }

  // List of ordered annotations for the slide
  std::list<AnnotUidAndLayer> orderedAnnotations;

  for (auto uid : dataManager.annotationUids_of_slide(*slideUid))
  {
    auto annot = dataManager.slideAnnotationRecord(uid);
    auto r = annot.lock();
    if (r && r->cpuData())
    {
      orderedAnnotations.push_back(std::make_pair(r->uid(), r->cpuData()->getLayer()));
    }
  }

  // Sort annotation UIDs based on layer
  orderedAnnotations.sort([](const AnnotUidAndLayer& a, const AnnotUidAndLayer& b)
                          { return (a.second < b.second); });

  // Find the annotation to be changed:
  auto it = std::find_if(
    std::begin(orderedAnnotations),
    std::end(orderedAnnotations),
    [slideAnnotUid](const AnnotUidAndLayer& a) { return (a.first == slideAnnotUid); }
  );

  // Apply the layer change:
  switch (layerChange)
  {
  case LayerChangeType::Backwards:
  {
    orderedAnnotations.splice(std::prev(it), orderedAnnotations, it);
    break;
  }
  case LayerChangeType::Forwards:
  {
    orderedAnnotations.splice(std::next(it), orderedAnnotations, it);
    break;
  }
  case LayerChangeType::ToBack:
  {
    orderedAnnotations.splice(std::begin(orderedAnnotations), orderedAnnotations, it);
    break;
  }
  case LayerChangeType::ToFront:
  {
    orderedAnnotations.splice(std::end(orderedAnnotations), orderedAnnotations, it);
    break;
  }
  }

  // Reassign the layers and depths based on their new order. Start assigning layers at 0:
  uint32_t layer = 0;
  const uint32_t maxLayer = orderedAnnotations.size() - 1;

  for (const auto& a : orderedAnnotations)
  {
    auto r = dataManager.slideAnnotationRecord(a.first).lock();
    if (r && r->cpuData())
    {
      r->cpuData()->setLayer(layer);
      r->cpuData()->setMaxLayer(maxLayer);
      ++layer;
    }
  }
}
