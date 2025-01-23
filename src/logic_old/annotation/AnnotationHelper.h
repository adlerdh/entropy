#ifndef ANNOTATION_HELPER_H
#define ANNOTATION_HELPER_H

#include <uuids.h>

class DataManager;
class Polygon;

/**
 * @brief Triangulate a polygon using the Earcut algorithm. This algorithm can triangulate a simple,
 * planar polygon of any winding order that includes holes. It returns a robust, acceptable solution
 * for non-simple poygons. Earcut works on a 2D plane.
 *
 * @see https://github.com/mapbox/earcut.hpp
 *
 * @param[in,out] polygon Polygon to triangulate.
 */
void triangulatePolygon(Polygon& polygon);

/**
 * @brief Annotation layers for a given slide may not be unique. This function sets each annotation
 * to a unique layer.
 *
 * @param dataManager
 */
void setUniqueSlideAnnotationLayers(DataManager& dataManager);

/// Types of changes to layering
enum class LayerChangeType
{
  Backwards,
  Forwards,
  ToBack,
  ToFront
};

/**
 * @brief Apply a change to an annotation's layering
 * @param dataManager
 * @param slideAnnotUid UID of the annotation
 * @param layerChange Change to apply to the layer
 */
void changeSlideAnnotationLayering(
  DataManager& dataManager, const uuids::uuid& slideAnnotUid, const LayerChangeType& layerChange
);

#endif // ANNOTATION_HELPER_H
