#ifndef APP_DATA_H
#define APP_DATA_H

#include "common/ParcellationLabelTable.h"
#include "common/UuidRange.h"

#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/Isosurface.h"

#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/app/Settings.h"
#include "logic/app/State.h"
#include "logic/serialization/ProjectSerialization.h"

#include "rendering/RenderData.h"
#include "windowing/WindowData.h"

#include "ui/GuiData.h"

#include <glm/vec2.hpp>

#include <uuid.h>

#include <expected>
#include <functional> // for std::reference_wrapper
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using ComponentIndexType = uint32_t;

/**
 * @brief Holds all application data
 * @todo A simple database would be better suited for this purpose
 */
class AppData
{
  using uuid = uuids::uuid;

public:
  AppData();
  ~AppData();

  const AppSettings& settings() const;
  AppSettings& settings();

  const AppState& state() const;
  AppState& state();

  const GuiData& guiData() const;
  GuiData& guiData();

  const RenderData& renderData() const;
  RenderData& renderData();

  const WindowData& windowData() const;
  WindowData& windowData();

  /// @todo Put into AppState
  void setProject(serialize::EntropyProject project);
  const serialize::EntropyProject& project() const;
  serialize::EntropyProject& project();

  /**
     * @brief Add an image
     * @param[in] image The image.
     * @return The image's newly generated unique identifier
     */
  uuid addImage(Image image);

  /**
     * @brief Add a segmentation.
     * @param[in] seg Segmentation image. The image must have unsigned integer pixel component type.
     * @return If added, the segmentation image's newly generated unique identifier; else nullopt.
     */
  std::optional<uuid> addSeg(Image seg);

  /**
     * @brief Add an image deformation field
     * @param[in] def Deformation field image. The image must have at least three components (x, y, z) per pixel.
     * @return If added, deformation field image's newly generated unique identifier; else nullpt.
     */
  std::optional<uuid> addDef(Image def);

  /**
     * @brief Add a segmentation label color table
     * @param[in] numLabels Number of labels in the table
     * @param[in] maxNumLabels Maximum number of labels allowed in the table
     * @return Index of the new table
     */
  std::size_t addLabelColorTable(std::size_t numLabels, std::size_t maxNumLabels);

  /**
     * @brief Add a landmark group
     * @param[in] lmGroup Landmark group.
     * @return The landmark group's newly generated unique identifier
     */
  uuid addLandmarkGroup(LandmarkGroup lmGroup);

  /**
     * @brief Add an annotation and associate it with an image
     * @param[in] imageUid Image UID
     * @param[in] annotation New annotation
     * @return If the image exists, return the annotation's newly generated unique identifier;
     * otherwise return nullopt.
     */
  std::optional<uuid> addAnnotation(const uuid& imageUid, Annotation annotation);

  /**
     * @brief Add a distance map to an image component. The maps are used to accelerate
     * volume raycasting by enabling empty space skipping.
     *
     * @param[in] imageUid UID of image for which to set the distance map
     * @param[in] component Image component for which to set the distance map
     * @param[in] distanceMap Distance map (in physical units) to a boundary in the image
     * @param[in] boundaryIsoValue Value of the distance used for defining the distance map
     *
     * @return True iff the distance map was successfully added for the image component.
     */
  bool addDistanceMap(
    const uuid& imageUid,
    ComponentIndexType component,
    Image distanceMap,
    double boundaryIsoValue
  );

  bool addNoiseEstimate(
    const uuid& imageUid, ComponentIndexType component, Image noiseEstimate, uint32_t radius
  );

  /**
     * @brief Add an isosurface to an image component.
     *
     * @param[in] imageUid UID of image to which to add the isosurface
     * @param[in] component Image component to which to add the isosurface
     * @param[in] isosurface Isosurface
     *
     * @return Unique ID of the isosurface iff it was successfully added for the image component;
     * otherwise \c std::nullpt
     */
  std::optional<uuid> addIsosurface(
    const uuid& imageUid, ComponentIndexType component, Isosurface isosurface);

  //    bool removeImage( const uuid& imageUid );
  bool removeSeg(const uuid& segUid);
  bool removeDef(const uuid& defUid);
  bool removeAnnotation(const uuid& defUid);

  /**
     * @brief Remove an isosurface from an image component.
     *
     * @param[in] imageUid UID of image from which to remove the isosurface
     * @param[in] component Image component from which to remove the isosurface
     * @param[in] isosurfaceUid Isosurface UID
     *
     * @return True iff the isosurface was successfully removed.
     */
  bool removeIsosurface(
    const uuid& imageUid, ComponentIndexType component, const uuid& isosurfaceUid);

  const Image* image(const uuid& imageUid) const;
  Image* image(const uuid& imageUid);

  std::expected<std::reference_wrapper<const Image>, std::string> getImage(const uuid& imageUid) const;
  std::expected<std::reference_wrapper<Image>, std::string> getImage(const uuid& imageUid);

  const Image* seg(const uuid& segUid) const;
  Image* seg(const uuid& segUid);

  const Image* def(const uuid& defUid) const;
  Image* def(const uuid& defUid);

  /// Get the distance maps (keyed by isosurface value) associated with an image component
  const std::map<double, Image>& distanceMaps(
    const uuid& imageUid, ComponentIndexType component
  ) const;

  /// Get the noise estimate images (keyed by radius value) associated with an image component
  const std::map<uint32_t, Image>& noiseEstimates(
    const uuid& imageUid, ComponentIndexType component
  ) const;

  /**
     * @brief Get an isosurface of an image component.
     *
     * @param[in] imageUid UID of image
     * @param[in] component Image component
     * @param[in] isosurfaceUid Isosurface UID
     *
     * @return Pointer to the isosurface if it exists; otherwise nullptr
     */
  const Isosurface* isosurface(
    const uuid& imageUid, ComponentIndexType component, const uuid& isosurfaceUid) const;

  Isosurface* isosurface(
    const uuid& imageUid, ComponentIndexType component, const uuid& isosurfaceUid);

  /*
  bool updateIsosurfaceMeshCpuRecord(
    const uuid& imageUid,
    ComponentIndexType component,
    const uuid& isosurfaceUid,
    std::unique_ptr<MeshCpuRecord> cpuRecord
  );

  bool updateIsosurfaceMeshGpuRecord(
    const uuid& imageUid,
    ComponentIndexType component,
    const uuid& isosurfaceUid,
    std::unique_ptr<MeshGpuRecord> gpuRecord
  );
*/

  const ImageColorMap* imageColorMap(const uuid& mapUid) const;
  ImageColorMap* imageColorMap(const uuid& mapUid);

  const ParcellationLabelTable* labelTable(const uuid& tableUid) const;
  ParcellationLabelTable* labelTable(const uuid& tableUid);

  const LandmarkGroup* landmarkGroup(const uuid& lmGroupUid) const;
  LandmarkGroup* landmarkGroup(const uuid& lmGroupUid);

  /**
     * @brief Get an annotation by UID
     * @param annotUid Annotation UID
     * @return Raw pointer to the annotation if it exists;
     * nullptr if the annotation does not exist
     */
  const Annotation* annotation(const uuid& annotUid) const;
  Annotation* annotation(const uuid& annotUid);

  /// Set/get the reference image UID
  bool setRefImageUid(const uuid& refImageUid);
  std::optional<uuid> refImageUid() const;

  /// Set/get the UID of the active image: the one that is currently being transformed or
  /// whose settings are currently being changed by the user
  bool setActiveImageUid(const uuid& activeImageUid);
  std::optional<uuid> activeImageUid() const;

  /// Set rainbow colors for the image border and edges
  void setRainbowColorsForAllImages();

  /// Set rainbow colors for all of the landmark groups.
  /// Copy the image edge color
  void setRainbowColorsForAllLandmarkGroups();

  /// Move the image backward/forward in layers (decrease/increase its layer order)
  /// @note It is important that these take arguments by value, since imageUids are being swapped
  /// internally. Potential for nasty bugs if a reference is used.
  bool moveImageBackwards(const uuid imageUid);
  bool moveImageForwards(const uuid imageUid);

  /// Move the image to the backmost/frontmost layer
  bool moveImageToBack(const uuid imageUid);
  bool moveImageToFront(const uuid imageUid);

  /// Move the image annotation backward/forward in layers (decrease/increase its layer order)
  bool moveAnnotationBackwards(const uuid imageUid, const uuid annotUid);
  bool moveAnnotationForwards(const uuid imageUid, const uuid annotUid);

  /// Move the image annotation to the backmost/frontmost layer
  bool moveAnnotationToBack(const uuid imageUid, const uuid annotUid);
  bool moveAnnotationToFront(const uuid imageUid, const uuid annotUid);

  std::size_t numImages() const;
  std::size_t numSegs() const;
  std::size_t numDefs() const;
  std::size_t numImageColorMaps() const;
  std::size_t numLabelTables() const;
  std::size_t numLandmarkGroups() const;
  std::size_t numAnnotations() const;

  uuid_range_t imageUidsOrdered() const;
  uuid_range_t segUidsOrdered() const;
  uuid_range_t defUidsOrdered() const;
  uuid_range_t imageColorMapUidsOrdered() const;
  uuid_range_t labelTableUidsOrdered() const;
  uuid_range_t landmarkGroupUidsOrdered() const;

  uuid_range_t isosurfaceUids(const uuid& imageUid, ComponentIndexType component) const;

  /// Set/get the active segmentation for an image
  bool assignActiveSegUidToImage(const uuid& imageUid, const uuid& activeSegUid);
  std::optional<uuid> imageToActiveSegUid(const uuid& imageUid) const;

  /// Set/get the active deformation field for an image
  bool assignActiveDefUidToImage(const uuid& imageUid, const uuid& activeDefUid);
  std::optional<uuid> imageToActiveDefUid(const uuid& imageUid) const;

  /// Assign a segmentation to an image.
  /// Makes it the active segmentation if it is the first one.
  bool assignSegUidToImage(const uuid& imageUid, const uuid& segUid);

  /// Assign a deformation field to an image.
  /// Makes it the active deformation field if it is the first one.
  bool assignDefUidToImage(const uuid& imageUid, const uuid& defUid);

  /// Get all segmentations for an image
  std::vector<uuid> imageToSegUids(const uuid& imageUid) const;

  /// Get all deformation fields for an image
  std::vector<uuid> imageToDefUids(const uuid& imageUid) const;

  /// Assign a group of landmarks to an image
  bool assignLandmarkGroupUidToImage(const uuid& imageUid, uuid lmGroupUid);
  const std::vector<uuid>& imageToLandmarkGroupUids(const uuid& imageUid) const;

  /// Set/get the active landmark group for an image
  bool assignActiveLandmarkGroupUidToImage(
    const uuid& imageUid, const uuid& lmGroupUid
  );
  std::optional<uuid> imageToActiveLandmarkGroupUid(const uuid& imageUid) const;

  /// Set/get the active annotation for an image
  bool assignActiveAnnotationUidToImage(
    const uuid& imageUid, const std::optional<uuid>& annotUid
  );
  std::optional<uuid> imageToActiveAnnotationUid(const uuid& imageUid) const;

  /**
     * @brief Get a list of all annotations assigned to a given image. The annotation order
     * corresponds to the order in which the annotations were added to the image.
     * @param imageUid Image UID
     * @return List of (ordered) annotation UIDs for the image.
     * The list is empty if the image has no annotations or the image UID is invalid.
     */
  const std::list<uuid>& annotationsForImage(const uuid& imageUid) const;

  /// Set/get whether the image is
  void setImageBeingSegmented(const uuid& imageUid, bool set);
  bool isImageBeingSegmented(const uuid& imageUid) const;

  uuid_range_t imagesBeingSegmented() const;

  std::optional<uuid> imageUid(std::size_t index) const;
  std::optional<uuid> segUid(std::size_t index) const;
  std::optional<uuid> defUid(std::size_t index) const;
  std::optional<uuid> imageColorMapUid(std::size_t index) const;
  std::optional<uuid> labelTableUid(std::size_t index) const;
  std::optional<uuid> landmarkGroupUid(std::size_t index) const;

  std::optional<std::size_t> imageIndex(const uuid& imageUid) const;
  std::optional<std::size_t> segIndex(const uuid& segUid) const;
  std::optional<std::size_t> defIndex(const uuid& defUid) const;
  std::optional<std::size_t> imageColorMapIndex(const uuid& mapUid) const;
  std::optional<std::size_t> labelTableIndex(const uuid& tableUid) const;
  std::optional<std::size_t> landmarkGroupIndex(const uuid& lmGroupUid) const;
  std::optional<std::size_t> annotationIndex(
    const uuid& imageUid, const uuid& annotUid
  ) const;

  /// @todo Put into DataHelper
  Image* refImage();
  const Image* refImage() const;

  Image* activeImage();
  const Image* activeImage() const;

  Image* activeSeg();

  ParcellationLabelTable* activeLabelTable();

  std::string getAllImageDisplayNames() const;

  /// Save the World-space coordinates of the centers of all views
  void saveAllViewWorldCenterPositions();

  void restoreAllViewWorldCenterPositions();

private:
  /// @brief Data associated with the individual image components
  struct ComponentData
  {
    /// @note Distance maps are used to accelerate volume rendering by enabling the ray-casting
    /// algorithm to skip empty space in the image volume. Each distance map is to a boundary
    /// (defined by a single isosurface) in the image. Each component of the image has its own
    /// distance map. Each map is paired with its corresponding boundary isosurface value.

    /// Distance maps for the component, keyed by boundary isosurface value
    std::map<double, Image> m_distanceMaps;

    /// Voxel-wise noise estimates of the image, keyed by the radius of the neighborhood
    /// used for computing the estimate
    std::map<uint32_t, Image> m_noiseEstimates;

    /// Isosurfaces for the component
    using IsosurfacePtr = std::unique_ptr<Isosurface>;
    using IsoSurfaceMap = std::unordered_map<uuid, IsosurfacePtr>;
    // std::unique_ptr<Isosurface> aDummyStub; //<-- add this line

    std::vector<uuid> m_isosurfaceUidsSorted; //!< Sorted isosurface uids
    std::unordered_map<uuid, Isosurface> m_isosurfaces; //!< Isosurfaces
  };

  mutable std::mutex m_componentDataMutex;

  void loadLinearRampImageColorMaps();
  void loadDiscreteImageColorMaps();
  void loadImageColorMapsFromDisk();

  void loadImageColorMaps();

  /// @todo Put into EntropyApp
  AppSettings m_settings; //!< Application settings
  AppState m_state;       //!< Application state

  GuiData m_guiData;       //!< Data for the UI
  RenderData m_renderData; //!< Data for rendering
  WindowData m_windowData; //!< Data for windowing

  serialize::EntropyProject m_project; //!< Project that is used for serialization

  std::unordered_map<uuid, Image> m_images; //!< Images
  std::vector<uuid> m_imageUidsOrdered;     //!< Image UIDs in order

  std::unordered_map<uuid, Image> m_segs; //!< Segmentations, also stored as images
  std::vector<uuid> m_segUidsOrdered;     //!< Segmentation UIDs in order

  std::unordered_map<uuid, Image> m_defs; //!< Deformation fields, also stored as images
  std::vector<uuid> m_defUidsOrdered;     //!< Deformation field UIDs in order

  std::unordered_map<uuid, ImageColorMap> m_imageColorMaps; //!< Image color maps
  std::vector<uuid> m_imageColorMapUidsOrdered; //!< Image color map UIDs in order

  std::unordered_map<uuid, ParcellationLabelTable> m_labelTables; //!< Segmentation label tables
  std::vector<uuid> m_labelTablesUidsOrdered; //!< Segmentation label table UIDs in order

  std::unordered_map<uuid, LandmarkGroup> m_landmarkGroups; //!< Landmark groups
  std::vector<uuid> m_landmarkGroupUidsOrdered;             //!< Landmark group UIDs in order

  std::unordered_map<uuid, Annotation> m_annotations; //!< Annotations

  /// ID of the reference image. This is null iff there are no images.
  std::optional<uuid> m_refImageUid;

  /// ID of the image being actively transformed or whose settings are being changed.
  /// This is null iff there are no images.
  std::optional<uuid> m_activeImageUid;

  /// Map of image to its segmentations
  std::unordered_map<uuid, std::vector<uuid> > m_imageToSegs;

  /// Map of image to its active segmentation
  std::unordered_map<uuid, uuid> m_imageToActiveSeg;

  /// Map of image to its deformation fields
  std::unordered_map<uuid, std::vector<uuid> > m_imageToDefs;

  /// Map of image to its active deformation field
  std::unordered_map<uuid, uuid> m_imageToActiveDef;

  /// Map of image to its landmark groups
  std::unordered_map<uuid, std::vector<uuid> > m_imageToLandmarkGroups;

  /// Map of image to its active landmark group
  std::unordered_map<uuid, uuid> m_imageToActiveLandmarkGroup;

  /// Map of image to its annotations.
  /// The order of the annotations matches the order in the list.
  std::unordered_map<uuid, std::list<uuid> > m_imageToAnnotations;

  /// Map of image to its active/selected annotation
  std::unordered_map<uuid, uuid> m_imageToActiveAnnotation;

  /// Map of image to its per-component data
  std::unordered_map<uuid, std::vector<ComponentData> > m_imageToComponentData;

  /// Is an image being segmented (in addition to the active image)?
  /// @todo Move to AppState
  std::unordered_set<uuid> m_imagesBeingSegmented;

  /// For each layout, save the World-space position of the center of each view
  using MapViewUidToCenterPos = std::unordered_map<uuid, glm::vec3>;
  std::vector<MapViewUidToCenterPos> m_savedViewWorldCenterPositions;
};

#endif // APP_DATA_H
