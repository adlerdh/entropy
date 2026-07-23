#pragma once

#include "common/UuidRange.h"
#include "logic/app/ParcellationLabelTable.h"

#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/ImageDerivedData.h"
#include "image/Isosurface.h"

#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/app/Settings.h"
#include "logic/app/State.h"
#include "logic/serialization/ProjectSerialization.h"

#include "registration/Jobs.h"

#include "rendering/RenderData.h"
#include "windowing/WindowData.h"

#include "ui/GuiData.h"

#include <glm/vec2.hpp>

#include <uuid.h>

#include <expected>

#include <filesystem>
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
 * @brief Cache key for a scalar component projection derived from one source image frame.
 */
struct ComponentProjectionCacheKey
{
  ComponentProjectionMode mode{ComponentProjectionMode::Mean}; //!< Projection operation
  uint32_t timePoint{0};                                       //!< Source image time point

  /// @brief Order keys for use in std::map.
  bool operator<(const ComponentProjectionCacheKey& other) const
  {
    if (mode != other.mode) {
      return mode < other.mode;
    }
    return timePoint < other.timePoint;
  }
};

/**
 * @brief Holds all application data
 * @todo A simple database would be better suited for this purpose
 */
class AppData
{
  using uuid = uuids::uuid;

public:
  AppData();
  ~AppData() = default;

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

  /**
   * @brief Return immutable registration jobs.
   */
  const registration::JobStore& registrationJobs() const;

  /**
   * @brief Return mutable registration jobs.
   */
  registration::JobStore& registrationJobs();

  /// @todo Put into AppState
  void setProject(serialize::EntropyProject project);
  void setProjectFileName(std::optional<std::filesystem::path> fileName);
  void clearProjectData();
  const serialize::EntropyProject& project() const;
  serialize::EntropyProject& project();
  const std::optional<std::filesystem::path>& projectFileName() const;

  /**
   * @brief Add an image
   * @param[in] image The image.
   * @return The image's newly generated unique identifier
   */
  uuid addImage(Image image);
  bool replaceImage(const uuid& imageUid, Image image);

  /**
   * @brief Add a segmentation.
   * @param[in] seg Segmentation image. The image must have unsigned integer pixel component type.
   * @return If added, the segmentation image's newly generated unique identifier; else nullopt.
   */
  std::optional<uuid> addSeg(Image seg);

  /**
   * @brief Add an image warp field.
   * @param[in] def Warp-field image. The image must have at least three components (x, y, z)
   * per pixel.
   * @return If added, warp-field image's newly generated unique identifier; else nullpt.
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
  uuid addLandmarkGroup(const LandmarkGroup& lmGroup);

  /**
   * @brief Add an annotation and associate it with an image
   * @param[in] imageUid Image UID
   * @param[in] annotation New annotation
   * @return If the image exists, return the annotation's newly generated unique identifier;
   * otherwise return nullopt.
   */
  std::optional<uuid> addAnnotation(const uuid& imageUid, const Annotation& annotation);

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
  bool addDistanceMap(const uuid& imageUid, ComponentIndexType component, Image distanceMap, double boundaryIsoValue);

  /**
   * @brief Remove distance maps for one image component.
   *
   * @param[in] imageUid UID of image whose distance maps should be removed
   * @param[in] component Image component whose distance maps should be removed
   * @return True iff component data existed and was cleared.
   */
  bool removeDistanceMaps(const uuid& imageUid, ComponentIndexType component);

  bool addNoiseEstimate(const uuid& imageUid, ComponentIndexType component, Image noiseEstimate, uint32_t radius);

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
  std::optional<uuid> addIsosurface(const uuid& imageUid, ComponentIndexType component, Isosurface isosurface);

  bool removeImage(const uuid& imageUid);
  bool removeSeg(const uuid& segUid);
  bool removeDef(const uuid& defUid);
  bool removeAnnotation(const uuid& annotUid);
  bool removeLandmarkGroup(const uuid& lmGroupUid);

  /**
   * @brief Remove an isosurface from an image component.
   *
   * @param[in] imageUid UID of image from which to remove the isosurface
   * @param[in] component Image component from which to remove the isosurface
   * @param[in] isosurfaceUid Isosurface UID
   *
   * @return True iff the isosurface was successfully removed.
   */
  bool removeIsosurface(const uuid& imageUid, ComponentIndexType component, const uuid& isosurfaceUid);

  const Image* image(const uuid& imageUid) const;
  Image* image(const uuid& imageUid);

  /**
   * @brief Add or replace a cached scalar projection for a multi-component source image.
   * @param imageUid Source image UID.
   * @param mode Component projection represented by \p image.
   * @param timePoint Source image time point used to compute \p image.
   * @param image Derived scalar image.
   * @return UID of the cached projection, or std::nullopt when the source image is invalid.
   */
  std::optional<uuid>
  setComponentProjectionImage(const uuid& imageUid, ComponentProjectionMode mode, uint32_t timePoint, Image image);

  /**
   * @brief Get the UID of a cached scalar component projection.
   * @param imageUid Source image UID.
   * @param mode Component projection mode.
   * @param timePoint Source image time point.
   * @return Cached projection UID, or std::nullopt when the projection does not exist.
   */
  std::optional<uuid>
  componentProjectionImageUid(const uuid& imageUid, ComponentProjectionMode mode, uint32_t timePoint) const;

  /**
   * @brief Get the source image UID for a cached scalar component projection.
   * @param projectionUid Hidden projection image UID.
   * @return Source image UID, or std::nullopt when \p projectionUid is not a projection.
   */
  std::optional<uuid> componentProjectionSourceImageUid(const uuid& projectionUid) const;

  /**
   * @brief Get the renderable image UID for an image's current component mode.
   * @param imageUid Source image UID selected by the view/layout.
   * @return Source UID, or a cached projection UID when that projection should be rendered.
   */
  uuid effectiveImageUidForRendering(const uuid& imageUid) const;

  std::expected<std::reference_wrapper<const Image>, std::string> getImage(const uuid& imageUid) const;
  std::expected<std::reference_wrapper<Image>, std::string> getImage(const uuid& imageUid);

  const Image* seg(const uuid& segUid) const;
  Image* seg(const uuid& segUid);

  const Image* def(const uuid& defUid) const;
  Image* def(const uuid& defUid);

  /**
   * @brief Resolve a UID that can be used as a warp field.
   *
   * Dedicated warp fields are accepted, as are normal loaded images that
   * can be interpreted as 3-component vector fields.
   *
   * @param warpUid Candidate warp-field UID.
   * @return Warp-field image, or nullptr when the UID cannot be used as a warp.
   */
  const Image* warpField(const uuid& warpUid) const;
  Image* warpField(const uuid& warpUid);

  /// Get the distance maps (keyed by isosurface value) associated with an image component
  const std::map<double, Image>& distanceMaps(const uuid& imageUid, ComponentIndexType component) const;

  /// Get the noise estimate images (keyed by radius value) associated with an image component
  const std::map<uint32_t, Image>& noiseEstimates(const uuid& imageUid, ComponentIndexType component) const;

  /**
   * @brief Get an isosurface of an image component.
   *
   * @param[in] imageUid UID of image
   * @param[in] component Image component
   * @param[in] isosurfaceUid Isosurface UID
   *
   * @return Pointer to the isosurface if it exists; otherwise nullptr
   */
  const Isosurface* isosurface(const uuid& imageUid, ComponentIndexType component, const uuid& isosurfaceUid) const;

  Isosurface* isosurface(const uuid& imageUid, ComponentIndexType component, const uuid& isosurfaceUid);

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
  /**
   * @brief Return all loaded images that can be selected as warp fields.
   *
   * Dedicated warp fields are listed first, followed by ordinary loaded
   * 3-component vector-field images.
   */
  uuid_range_t warpFieldCandidateUidsOrdered() const;
  uuid_range_t imageColorMapUidsOrdered() const;
  uuid_range_t labelTableUidsOrdered() const;
  uuid_range_t landmarkGroupUidsOrdered() const;

  uuid_range_t isosurfaceUids(const uuid& imageUid, ComponentIndexType component) const;

  /// Set/get the active segmentation for an image
  bool assignActiveSegUidToImage(const uuid& imageUid, const uuid& activeSegUid);
  std::optional<uuid> imageToActiveSegUid(const uuid& imageUid) const;

  /**
   * @brief Assign the active inverse warp for an image.
   *
   * @param imageUid Moving image that will be sampled through the warp.
   * @param activeWarpUid Inverse warp UID.
   * @return True when both image and warp field exist and the field is usable.
   */
  bool assignActiveInverseWarpUidToImage(const uuid& imageUid, const uuid& activeWarpUid);
  bool assignActiveInverseWarpUidToImage(
    const uuid& imageUid,
    const uuid& activeWarpUid,
    const std::optional<uuid>& referenceImageUid);

  /** @brief Clear the active inverse warp for an image. */
  void clearActiveInverseWarpUidForImage(const uuid& imageUid);

  /**
   * @brief Return the active inverse warp for an image.
   *
   * @param imageUid Image UID.
   * @return Active inverse warp UID, or std::nullopt when none is assigned.
   */
  std::optional<uuid> imageToActiveInverseWarpUid(const uuid& imageUid) const;

  /**
   *  Return the reference-space image for the active inverse warp.
   *
   *  imageUid Moving image UID.
   *  Reference image UID, or the moving image UID when no explicit reference was assigned.
   */
  std::optional<uuid> imageToActiveInverseWarpReferenceImageUid(const uuid& imageUid) const;

  bool setActiveInverseWarpReferenceImageUid(const uuid& imageUid, const std::optional<uuid>& referenceImageUid);

  /**
   * @brief Assign the active forward warp for an image.
   *
   * @param imageUid Image whose forward warp is being assigned.
   * @param activeWarpUid Forward warp UID.
   * @return True when both image and warp field exist and the field is usable.
   */
  bool assignActiveForwardWarpUidToImage(const uuid& imageUid, const uuid& activeWarpUid);

  /** @brief Clear the active forward warp for an image. */
  void clearActiveForwardWarpUidForImage(const uuid& imageUid);

  /**
   * @brief Return the active forward warp for an image.
   *
   * @param imageUid Image UID.
   * @return Active forward warp UID, or std::nullopt when none is assigned.
   */
  std::optional<uuid> imageToActiveForwardWarpUid(const uuid& imageUid) const;

  /// Assign a segmentation to an image.
  /// Makes it the active segmentation if it is the first one.
  bool assignSegUidToImage(const uuid& imageUid, const uuid& segUid);

  /**
   * @brief Add an inverse warp to an image.
   *
   * Makes the field active when it is the first inverse warp assigned to the image.
   *
   * @param imageUid Image UID.
   * @param warpUid Inverse warp UID.
   * @return True when the field was assigned.
   */
  bool assignInverseWarpUidToImage(const uuid& imageUid, const uuid& warpUid);
  bool
  assignInverseWarpUidToImage(const uuid& imageUid, const uuid& warpUid, const std::optional<uuid>& referenceImageUid);

  /**
   * @brief Add a forward warp to an image.
   *
   * Assigns the field as the active forward warp without changing the active inverse warp.
   *
   * @param imageUid Image UID.
   * @param warpUid Forward warp UID.
   * @return True when the field was assigned.
   */
  bool assignForwardWarpUidToImage(const uuid& imageUid, const uuid& warpUid);

  /// Get all segmentations for an image
  std::vector<uuid> imageToSegUids(const uuid& imageUid) const;

  /// Get all warp fields for an image
  std::vector<uuid> imageToDefUids(const uuid& imageUid) const;

  /// Assign a group of landmarks to an image
  bool assignLandmarkGroupUidToImage(const uuid& imageUid, uuid lmGroupUid);
  const std::vector<uuid>& imageToLandmarkGroupUids(const uuid& imageUid) const;

  /// Set/get the active landmark group for an image
  bool assignActiveLandmarkGroupUidToImage(const uuid& imageUid, const uuid& lmGroupUid);
  std::optional<uuid> imageToActiveLandmarkGroupUid(const uuid& imageUid) const;

  /// Set/get the active annotation for an image
  bool assignActiveAnnotationUidToImage(const uuid& imageUid, const std::optional<uuid>& annotUid);
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
  std::optional<std::size_t> annotationIndex(const uuid& imageUid, const uuid& annotUid) const;

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
    /// @note Distance maps accelerate volume raycasting by skipping empty space relative to a foreground mask. A map is
    /// conservative only while the rendered surfaces remain inside the threshold range used to create that mask.

    /// Distance map for the component
    std::map<double, Image> m_distanceMaps;

    /// Voxel-wise noise estimates of the image, keyed by the radius of the neighborhood
    /// used for computing the estimate
    std::map<uint32_t, Image> m_noiseEstimates;

    /// Isosurfaces for the component
    using IsosurfacePtr = std::unique_ptr<Isosurface>;
    using IsoSurfaceMap = std::unordered_map<uuid, IsosurfacePtr>;
    // std::unique_ptr<Isosurface> aDummyStub; //<-- add this line

    std::vector<uuid> m_isosurfaceUidsSorted;           //!< Sorted isosurface uids
    std::unordered_map<uuid, Isosurface> m_isosurfaces; //!< Isosurfaces
  };

  mutable std::mutex m_componentDataMutex;

  void loadLinearRampImageColorMaps();
  void loadDiscreteImageColorMaps();
  void loadImageColorMapsFromDisk();

  void loadImageColorMaps();

  /**
   * @brief Remove a warp field UID from all image assignments.
   * @param warpUid Warp field UID to remove.
   */
  void removeWarpReferences(const uuid& warpUid);

  /// @todo Put into EntropyApp
  /// Application settings
  AppSettings m_settings;
  AppState m_state; //!< Application state

  GuiData m_guiData;                         //!< Data for the UI
  RenderData m_renderData;                   //!< Data for rendering
  WindowData m_windowData;                   //!< Data for windowing
  registration::JobStore m_registrationJobs; //!< In-memory registration job records

  serialize::EntropyProject m_project;                    //!< Project that is used for serialization
  std::optional<std::filesystem::path> m_projectFileName; //!< File name of the currently loaded/saved project

  std::unordered_map<uuid, Image> m_images; //!< Images
  std::vector<uuid> m_imageUidsOrdered;     //!< Image UIDs in order

  std::unordered_map<uuid, Image> m_componentProjectionImages; //!< Hidden scalar component projections
  /// @todo This cache is time-point-aware but not memory-bounded. Replace it with an LRU cache when
  /// large time series make it possible to accumulate many derived frames.
  std::unordered_map<uuid, std::map<ComponentProjectionCacheKey, uuid> > m_imageToComponentProjectionImages;
  std::unordered_map<uuid, uuid> m_componentProjectionToSourceImage; //!< Hidden projection UID to source image UID

  std::unordered_map<uuid, Image> m_segs; //!< Segmentations, also stored as images
  std::vector<uuid> m_segUidsOrdered;     //!< Segmentation UIDs in order

  std::unordered_map<uuid, Image> m_defs; //!< Warp fields, also stored as images
  std::vector<uuid> m_defUidsOrdered;     //!< Warp-field UIDs in order

  std::unordered_map<uuid, ImageColorMap> m_imageColorMaps; //!< Image color maps
  std::vector<uuid> m_imageColorMapUidsOrdered;             //!< Image color map UIDs in order

  std::unordered_map<uuid, ParcellationLabelTable> m_labelTables; //!< Segmentation label tables
  std::vector<uuid> m_labelTablesUidsOrdered;                     //!< Segmentation label table UIDs in order

  std::unordered_map<uuid, LandmarkGroup> m_landmarkGroups; //!< Landmark groups
  std::vector<uuid> m_landmarkGroupUidsOrdered;             //!< Landmark group UIDs in order

  std::unordered_map<uuid, Annotation> m_annotations; //!< Annotations

  std::optional<uuid> m_refImageUid; //!< Reference image UID, or null iff there are no images

  /// ID of the image being actively transformed or whose settings are being changed.
  /// This is null iff there are no images
  std::optional<uuid> m_activeImageUid;

  /// Map of image to its segmentations
  std::unordered_map<uuid, std::vector<uuid> > m_imageToSegs;

  /// Map of image to its active segmentation
  std::unordered_map<uuid, uuid> m_imageToActiveSeg;

  /// Map of image to its warp fields
  std::unordered_map<uuid, std::vector<uuid> > m_imageToDefs;

  /// Map of image to the active inverse warp used for image sampling
  std::unordered_map<uuid, uuid> m_imageToActiveInverseWarp;

  // Map of image to the reference-space image for its active inverse warp
  std::unordered_map<uuid, uuid> m_imageToActiveInverseWarpReferenceImage;

  /// Map of image to the active forward warp used for landmarks and annotations
  std::unordered_map<uuid, uuid> m_imageToActiveForwardWarp;

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
