#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "common/UIDRange.h"

#include "logic/records/ImageColorMapRecord.h"
#include "logic/records/ImageRecord.h"
#include "logic/records/LabelTableRecord.h"
#include "logic/records/LandmarkGroupRecord.h"
#include "logic/records/MeshRecord.h"
#include "logic/records/ParcellationRecord.h"
#include "logic/records/SlideAnnotationRecord.h"
#include "logic/records/SlideRecord.h"
#include "logic/serialization/ProjectSerialization.h"

#include <boost/range/any_range.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

/**
 * @brief This class owns the data for images, parcellations, label meshes,
 * iso-surface meshes, slides, image color maps, and parcellation label tables.
 * It only returns record UIDs and weak pointers to its data to clients.
 */
class DataManager
{
public:
  /// Range of weak pointers to Records
  template<class Record>
  using weak_record_range_t = boost::any_range<
    std::weak_ptr<Record>,
    boost::forward_traversal_tag,
    std::weak_ptr<Record>&,
    std::ptrdiff_t>;

  DataManager();

  DataManager(const DataManager&) = delete;
  DataManager& operator=(const DataManager&) = delete;

  DataManager(DataManager&&) = default;
  DataManager& operator=(DataManager&&) = default;

  ~DataManager();

  void setProject(serialize::HZeeProject);

  const serialize::HZeeProject& project() const;
  serialize::HZeeProject& project();

  /// Update the project with current images, slides, settings, and transformations
  /// @param[in] newFileName Optional new project file name
  void updateProject(const std::optional<std::string>& newFileName);

  /// Insert an image record and return its assigned UID.
  std::optional<UID> insertImageRecord(std::shared_ptr<ImageRecord>);

  /// Insert a parcellation record and return its assigned UID.
  std::optional<UID> insertParcellationRecord(std::shared_ptr<ParcellationRecord>);

  /// Insert a slide record and return its assigned UID.
  std::optional<UID> insertSlideRecord(std::shared_ptr<SlideRecord>);

  /// Insert an image color map record and return its assigned UID.
  std::optional<UID> insertImageColorMapRecord(std::shared_ptr<ImageColorMapRecord>);

  /// Insert a label table record and return its assigned UID.
  std::optional<UID> insertLabelTableRecord(std::shared_ptr<LabelTableRecord>);

  /// Insert an isosurface mesh record associated with the given image
  std::optional<UID> insertIsoMeshRecord(const UID& imageUid, std::shared_ptr<MeshRecord>);

  /// Insert a label mesh record associated with the given parcellation
  std::optional<UID> insertLabelMeshRecord(const UID& parcelUid, std::shared_ptr<MeshRecord>);

  /// Insert a reference image landmark group record associated with the given image
  std::optional<UID>
  insertRefImageLandmarkGroupRecord(const UID& imageUid, std::shared_ptr<LandmarkGroupRecord>);

  /// Insert a slide landmark group record associated with the given slide
  std::optional<UID>
  insertSlideLandmarkGroupRecord(const UID& slideUid, std::shared_ptr<LandmarkGroupRecord>);

  /// Insert a slide annotation record associated with the given slide
  std::optional<UID>
  insertSlideAnnotationRecord(const UID& slideUid, std::shared_ptr<SlideAnnotationRecord>);

  /// Associate an image color map with a given image.
  /// Return true iff the image and color map exist and association is successful.
  bool associateColorMapWithImage(const UID& imageUid, const UID& colorMapUid);

  /// Associate a label table with a given parcellation.
  /// Return true iff the parcellation and labels exist and association is successful.
  bool associateLabelTableWithParcellation(const UID& parcelUid, const UID& labelTableUid);

  /// Associate a default parcellation with an image.
  /// Return true iff the parcellation and image exist and association is successful.
  bool associateDefaultParcellationWithImage(const UID& imageUid, const UID& parcelUid);

  /// Unload records. Return true iff successful.
  bool unloadImage(const UID& imageUid);
  bool unloadParcellation(const UID& parcelUid);
  bool unloadSlide(const UID& slideUid);
  bool unloadLabelMesh(const UID& meshUid);
  bool unloadIsoMesh(const UID& meshUid);
  bool unloadLabelTable(const UID& labelsUid);
  bool unloadRefImageLandmarkGroup(const UID& lmGroupUid);
  bool unloadSlideLandmarkGroup(const UID& lmGroupUid);
  bool unloadSlideAnnotation(const UID& annotUid);

  /// Set active records. Return true iff successful.
  bool setActiveImageUid(const std::optional<UID>& imageUid);
  bool setActiveParcellationUid(const std::optional<UID>& parcelUid);

  bool setActiveSlideUid(const UID& slideUid);
  bool setActiveSlideIndex(size_t slideIndex);

  bool setDefaultImageColorMapUid(const UID& mapUid);

  /// Set the ordering of slides. Return true iff successful.
  bool setSlideOrder(const std::list<UID>& orderedSlideUids);

  /// Get the "active" image, which is currently being manipulated (i.e. transformed, window-leveled,
  /// highlighted) by user. This image also defines the coordinate system of the views.
  std::optional<UID> activeImageUid() const;

  /// Get parcellation currently being displayed
  std::optional<UID> activeParcellationUid() const;

  /// Get the active slide UID. This is the slide currently being manipulated by user.
  /// If there are no slides, then std::nullopt is returned.
  std::optional<UID> activeSlideUid() const;

  /// Get index of the active slide
  /// If there are no slides, then std::nullopt is returned.
  std::optional<size_t> activeSlideIndex() const;

  /// Get index of a slide within the Slide Stack.
  std::optional<size_t> slideIndex(const UID& slideUid) const;

  /// Get default parcellation UID associated with an image.
  /// This is the parcellation that is generated by default by the application
  /// and that is initially blank.
  std::optional<UID> defaultParcellationUid_of_image(const UID& imageUid) const;

  /// Get UID of parcellation used to generate a given label mesh
  std::optional<UID> parcellationUid_of_labelMesh(const UID& labelMeshUid) const;

  /// Get UID of image used to generate a given isosurface mesh
  std::optional<UID> imageUid_of_isoMesh(const UID& isoMeshUid) const;

  /// Get UID of image color map associated with an image
  std::optional<UID> imageColorMapUid_of_image(const UID& imageUid) const;

  /// Get UID of label table associated with a parcellation
  std::optional<UID> labelTableUid_of_parcellation(const UID& parcelUid) const;

  /// Get UID of the default greyscale image color map
  std::optional<UID> defaultImageColorMapUid() const;

  /// Get UID of reference image associated with a given group of landmarks
  std::optional<UID> imageUid_of_landmarkGroup(const UID& lmGroupUid) const;

  /// Get UID of slide associated with a given group of landmarks
  std::optional<UID> slideUid_of_landmarkGroup(const UID& lmGroupUid) const;

  /// Get UID of slide associated with a given annotation
  std::optional<UID> slideUid_of_annotation(const UID& annotUid) const;

  /// Get UID of image at given ordered index
  std::optional<UID> orderedImageUid(long index);

  /// Get UID of parcellation at given ordered index
  std::optional<UID> orderedParcellationUid(long index);

  /// Get UID of slide at given ordered index
  std::optional<UID> orderedSlideUid(long index);

  /// Get UID of image color map at given ordered index
  std::optional<UID> orderedImageColorMapUid(long index);

  /// Get UID of reference image landmark group at given ordered index
  std::optional<UID> orderedRefImageLandmarkGroupUid(const UID& imageUid, long index);

  /// Get UID of slide landmark group at given ordered index
  std::optional<UID> orderedSlideLandmarkGroupUid(const UID& slideUid, long index);

  /// Get UID of slide annotation at given ordered index
  std::optional<UID> orderedSlideAnnotationUid(const UID& slideUid, long index);

  /// Get index of image with given UID
  std::optional<long> orderedImageIndex(const UID& uid);

  /// Get index of parcellation with given UID
  std::optional<long> orderedParcellationIndex(const UID& uid);

  /// Get index of slide with given UID
  std::optional<long> orderedSlideIndex(const UID& uid);

  /// Get index of slide with given UID
  std::optional<long> orderedImageColorMapIndex(const UID& uid);

  /// Get index of reference image landmark group with given UID
  std::optional<long> orderedRefImageLandmarkGroupIndex(const UID& imageUid, const UID& lmGroupUid);

  /// Get index of slide landmark group with given UID
  std::optional<long> orderedSlideLandmarkGroupIndex(const UID& slideUid, const UID& lmGroupUid);

  /// Get index of slide annotation with given UID
  std::optional<long> orderedSlideAnnotationIndex(const UID& slideUid, const UID& annotUid);

  /// Return ordered image UIDs.
  uid_range_t orderedImageUids() const;

  /// Return ordered parcellation UIDs.
  uid_range_t orderedParcellationUids() const;

  /// Return ordered slide UIDs.
  uid_range_t orderedSlideUids() const;

  /// Return ordered reference image landmark group UIDs.
  uid_range_t orderedRefImageLandmarkGroupUids() const;

  /// Return ordered landmark group UIDs for a given slide.
  uid_range_t orderedSlideLandmarkGroupUids(const UID& slideUid) const;

  /// Return ordered annotation UIDs for a given slide.
  uid_range_t orderedSlideAnnotationUids(const UID& slideUid) const;

  /// Return un-ordered iso-surface mesh UIDs.
  uid_range_t isoMeshUids() const;

  /// Return un-ordered label mesh UIDs.
  uid_range_t labelMeshUids() const;

  /// Return ordered image color map UIDs.
  uid_range_t orderedImageColorMapUids() const;

  /// Return un-ordered label table record UIDs.
  uid_range_t labelTableUids() const;

  /// Return iso-surface mesh UIDs corresponding to an image.
  uid_range_t isoMeshUids_of_image(const UID& imageUid) const;

  /// Return landmark group UIDs corresponding to a reference image.
  uid_range_t landmarkGroupUids_of_image(const UID& imageUid) const;

  /// Return landmark group UIDs corresponding to a slide.
  uid_range_t landmarkGroupUids_of_slide(const UID& slideUid) const;

  /// Return annotation UIDs corresponding to a slide.
  uid_range_t annotationUids_of_slide(const UID& slideUid) const;

  /// Return map from all label indices to corresponding label mesh UID for a parcellation.
  std::map<uint32_t, UID> labelMeshUids_of_parcellation(const UID& parcelUid) const;

  std::weak_ptr<ImageRecord> imageRecord(const UID& imageUid);
  std::weak_ptr<ParcellationRecord> parcellationRecord(const UID& parcelUid);
  std::weak_ptr<MeshRecord> isoMeshRecord(const UID& meshUid);
  std::weak_ptr<MeshRecord> labelMeshRecord(const UID& meshUid);
  std::weak_ptr<SlideRecord> slideRecord(const UID& slideUid);

  std::weak_ptr<ImageColorMapRecord> imageColorMapRecord(const UID& mapUid);
  std::weak_ptr<LabelTableRecord> labelTableRecord(const UID& labelTableUid);

  std::weak_ptr<LandmarkGroupRecord> refImageLandmarkGroupRecord(const UID& lmGroupUid);
  std::weak_ptr<LandmarkGroupRecord> slideLandmarkGroupRecord(const UID& lmGroupUid);
  std::weak_ptr<SlideAnnotationRecord> slideAnnotationRecord(const UID& annotUid);

  /// Return un-ordered image records
  weak_record_range_t<ImageRecord> imageRecords();

  /// Return un-ordered parcellation records
  weak_record_range_t<ParcellationRecord> parcellationRecords();

  /// Return un-ordered iso-surface mesh records
  weak_record_range_t<MeshRecord> isoMeshRecords();

  /// Return un-ordered label mesh records
  weak_record_range_t<MeshRecord> labelMeshRecords();

  /// Return un-ordered slide records
  weak_record_range_t<SlideRecord> slideRecords();

  /// Return un-ordered image color map records
  weak_record_range_t<ImageColorMapRecord> imageColorMapRecords();

  /// Return un-ordered label table records
  weak_record_range_t<LabelTableRecord> labelTableRecords();

  /// Return un-ordered reference image landmark group records
  weak_record_range_t<LandmarkGroupRecord> refImageLandmarkGroupRecords();

  /// Return un-ordered slide landmark group records
  weak_record_range_t<LandmarkGroupRecord> slideLandmarkGroupRecords();

  /// Return un-ordered slide annotation records
  weak_record_range_t<SlideAnnotationRecord> slideAnnotationRecords();

  std::weak_ptr<ImageRecord> baseImageRecord();
  std::weak_ptr<ImageRecord> activeImageRecord();
  std::weak_ptr<ParcellationRecord> activeParcellationRecord();
  std::weak_ptr<SlideRecord> activeSlideRecord();

  /// @todo Separate logic for when image/parce/slide property changes vs
  /// the image/parcel selection or slide stack (order, includes of slides)

  /// Connect an external slot to the signal that image data has changed
  void connectToImageDataChangedSignal(std::function<void(const UID& imageUid)>);

  /// Connect an external slot to the signal that parcellation data has changed
  void connectToParcellationDataChangedSignal(std::function<void(const UID& parcelUid)>);

  /// Connect an external slot to the signal that label data has changed
  void connectToLabelTableDataChangedSignal(std::function<void(const UID& labelTableUid)>);

  /// Connect an external slot to the signal that a single slide's data has changed
  void connectToSlideDataChangedSignal(std::function<void(const UID& slideUid)>);

  /// Connect an external slot to the signal that slide stack composition has changed
  void connectToSlideStackChangedSignal(std::function<void(void)>);

  /// Connect an external slot to the signal that the active slide selection has changed
  void connectToActiveSlideChangedSignal(std::function<void(const UID& activeSlideUid)>);

  /// Connect an external slot to the signal that the reference image landmark group has changed
  void connectToRefImageLandmarkGroupChangedSignal(std::function<void(const UID& lmGroupUid)>);

  /// Connect an external slot to the signal that the slide landmark group has changed
  void connectToSlideLandmarkGroupChangedSignal(std::function<void(const UID& lmGroupUid)>);

  /// Connect an external slot to the signal that the slide annotation has changed
  void connectToSlideAnnotationChangedSignal(std::function<void(const UID& annotUid)>);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

#endif // DATA_MANAGER_H
