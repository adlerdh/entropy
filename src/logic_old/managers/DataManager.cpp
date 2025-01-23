#include "logic/managers/DataManager.h"

#include "common/HZeeException.hpp"

#include <glm/glm.hpp>

#include <boost/range.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/signals2.hpp>

#include <algorithm>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <unordered_map>

namespace
{

/**
 * @brief Transform a map of shared records to a range of weak records
 */
template<class Record>
DataManager::weak_record_range_t<Record> transformSharedToWeakRecords(
  const std::unordered_map<UID, std::shared_ptr<Record> >& hashMap
)
{
  return boost::adaptors::transform(
    hashMap | boost::adaptors::map_values,
    [](std::shared_ptr<Record> rec) -> std::weak_ptr<Record> { return rec; }
  );
}

/**
 * @brief Search if a given value exists in a map or not.
 * Adds all the keys with given value in the vector
 */
template<typename K, typename V>
bool findByValue(std::vector<K>& vec, std::map<K, V> mapOfElemen, V value)
{
  bool bResult = false;
  auto it = mapOfElemen.begin();

  // Iterate through the map
  while (it != std::end(mapOfElemen))
  {
    // Check if value of this entry matches with given value
    if (it->second == value)
    {
      bResult = true;           // Yes found
      vec.push_back(it->first); // Push the key in given map
    }

    it++; // Go to next entry in map
  }

  return bResult;
}

/**
 * @brief Compare two lists for equality, returning true iff they are identical.
 */
template<class T>
bool compareListContents(const std::list<T>& l1, const std::list<T>& l2)
{
  std::list<T> l1Sorted = l1;
  std::list<T> l2Sorted = l2;

  l1Sorted.sort();
  l2Sorted.sort();

  return (l1Sorted == l2Sorted);
}

} // namespace

struct DataManager::Impl
{
  Impl()
    : m_project()
    ,

    m_imageRecords()
    , m_parcelRecords()
    , m_isoMeshRecords()
    , m_labelMeshRecords()
    , m_slideRecords()
    , m_imageColorMapRecords()
    , m_labelsRecords()
    , m_refImageLandmarkGroupRecords()
    , m_slideLandmarkGroupRecords()
    , m_slideAnnotationRecords()
    ,

    m_orderedImageUids()
    , m_orderedParcelUids()
    , m_orderedSlideUids()
    , m_orderedImageColorMapUids()
    , m_orderedRefImageLandmarkGroupUids()
    , m_orderedSlideLandmarkGroupUids()
    , m_orderedSlideAnnotationUids()
    ,

    m_activeImageUids(std::nullopt)
    , m_activeParcelUids(std::nullopt)
    , m_activeSlideUid(std::nullopt)
    ,

    m_defaultImageColorMapUid(std::nullopt)
    ,

    m_imageUid_to_defaultParcelUid()
    ,

    m_labelMeshUid_to_parcelUid()
    , m_parcelUid_to_labelMeshUids()
    ,

    m_isoMeshUid_to_imageUid()
    , m_imageUid_to_isoMeshUids()
    ,

    m_imageUid_to_imageColorMapUid()
    , m_parcelUid_to_labelsUid()
    ,

    m_refImageLandmarkGroupUid_to_imageUid()
    , m_slideLandmarkGroupUid_to_slideUid()
    , m_imageUid_to_landmarkGroupUids()
    , m_slideUid_to_landmarkGroupUids()
    ,

    m_slideAnnotationUid_to_slideUid()
    , m_slideUid_to_annotationUids()
  {
  }

  serialize::HZeeProject m_project;

  std::unordered_map<UID, std::shared_ptr<ImageRecord> > m_imageRecords;
  std::unordered_map<UID, std::shared_ptr<ParcellationRecord> > m_parcelRecords;

  std::unordered_map<UID, std::shared_ptr<MeshRecord> > m_isoMeshRecords;
  std::unordered_map<UID, std::shared_ptr<MeshRecord> > m_labelMeshRecords;

  std::unordered_map<UID, std::shared_ptr<SlideRecord> > m_slideRecords;

  std::unordered_map<UID, std::shared_ptr<ImageColorMapRecord> > m_imageColorMapRecords;
  std::unordered_map<UID, std::shared_ptr<LabelTableRecord> > m_labelsRecords;

  std::unordered_map<UID, std::shared_ptr<LandmarkGroupRecord> > m_refImageLandmarkGroupRecords;
  std::unordered_map<UID, std::shared_ptr<LandmarkGroupRecord> > m_slideLandmarkGroupRecords;

  std::unordered_map<UID, std::shared_ptr<SlideAnnotationRecord> > m_slideAnnotationRecords;

  /// Images ordered by sequence in list
  std::list<UID> m_orderedImageUids;

  /// Parcellations ordered by sequence in list
  std::list<UID> m_orderedParcelUids;

  /// Slides ordered by sequence in list
  std::list<UID> m_orderedSlideUids;

  /// Image color maps ordered by sequence in list
  std::vector<UID> m_orderedImageColorMapUids;

  /// Reference image landmark groups ordered in list
  std::list<UID> m_orderedRefImageLandmarkGroupUids;

  /// For each slide, the landmark groups are ordered in a list
  std::unordered_map<UID, std::list<UID> > m_orderedSlideLandmarkGroupUids;

  /// For each slide, the annotations are ordered in a list
  std::unordered_map<UID, std::list<UID> > m_orderedSlideAnnotationUids;

  /// The image that determines the reference space.
  /// It is also the one being actively manipulated in the UI.
  std::optional<UID> m_activeImageUids;

  /// The visible parcellation. It is also the one being actively manipulated in the UI.
  std::optional<UID> m_activeParcelUids;

  /// The slide being actively manipulated in the UI.
  std::optional<UID> m_activeSlideUid;

  /// The default image color map
  std::optional<UID> m_defaultImageColorMapUid;

  /// Map from image UID to its default parcellation UID
  std::unordered_map<UID, UID> m_imageUid_to_defaultParcelUid;

  /// Map from label mesh UID to its corresponding parcellation UID
  std::unordered_map<UID, UID> m_labelMeshUid_to_parcelUid;

  /// Map from parcellation UID to its label mesh UIDs.
  /// (i.e. inverse of m_labelMeshUID_to_parcellationUID)
  /// Label mesh UIDs are held in map with key = label index; value = meshUid.
  std::unordered_map<UID, std::map<uint32_t, UID> > m_parcelUid_to_labelMeshUids;

  /// Map from isosurface mesh UID to its corresponding image UID
  std::unordered_map<UID, UID> m_isoMeshUid_to_imageUid;

  /// Map from image UID to its isosurface mesh UIDs.
  /// (i.e. inverse of m_isoMeshUID_to_imageUid)
  /// @todo Iso-mesh UIDs are held in map with (key = iso value; value = mesh UID).
  std::unordered_map<UID, std::set<UID> > m_imageUid_to_isoMeshUids;

  /// Map from image UID to image color map UID
  std::unordered_map<UID, UID> m_imageUid_to_imageColorMapUid;

  /// Map from parcellation UID to labels table UID
  std::unordered_map<UID, UID> m_parcelUid_to_labelsUid;

  /// Map from parcellation UID to colormap buffer texture
  std::unordered_map<UID, std::shared_ptr<GLBufferTexture> > m_labelUid_to_colormapTextureBuffer;

  /// Map from reference image landmark group UID to corresponding image UID
  std::unordered_map<UID, UID> m_refImageLandmarkGroupUid_to_imageUid;

  /// Map from reference slide landmark group UID to corresponding slide UID
  std::unordered_map<UID, UID> m_slideLandmarkGroupUid_to_slideUid;

  /// Map from image UID to its landmark group UIDs.
  /// (i.e. inverse of m_refImageLandmarkGroupUid_to_imageUid)
  std::unordered_map<UID, std::set<UID> > m_imageUid_to_landmarkGroupUids;

  /// Map from slide UID to its landmark group UIDs.
  /// (i.e. inverse of m_slideLandmarkGroupUid_to_slideUid)
  std::unordered_map<UID, std::set<UID> > m_slideUid_to_landmarkGroupUids;

  /// Map from reference slide annotation UID to corresponding slide UID
  std::unordered_map<UID, UID> m_slideAnnotationUid_to_slideUid;

  /// Map from slide UID to its annotation UIDs.
  /// (i.e. inverse of m_slideAnnotationUid_to_slideUid)
  std::unordered_map<UID, std::set<UID> > m_slideUid_to_annotationUids;

  /// Signal that image data has changed
  boost::signals2::signal<void(const UID& imageUid)> m_signalImageDataChanged;

  /// Signal that image window/level has changed
  boost::signals2::signal<void(const UID& imageUid)> m_signalImageWindowLevelChanged;

  /// Signal that parcellation data has changed
  boost::signals2::signal<void(const UID& parcelUid)> m_signalParcellationDataChanged;

  /// Signal that a single slide's data has changed
  boost::signals2::signal<void(const UID& slideUid)> m_signalSlideDataChanged;

  /// Signal that slide stack composition has changed
  boost::signals2::signal<void(void)> m_signalSlideStackChanged;

  /// Signal that the active slide selection has changed
  boost::signals2::signal<void(const UID& slideUid)> m_signalActiveSlideChanged;

  /// Signal that image color map data has changed
  boost::signals2::signal<void(const UID& colorMapUid)> m_signalImageColorMapDataChanged;

  /// Signal that label table data has changed
  boost::signals2::signal<void(const UID& labelTableUid)> m_signalLabelTableDataChanged;

  /// Signal that image iso-surface mesh data has changed
  boost::signals2::signal<void(const UID& meshUid)> m_signalIsoMeshDataChanged;

  /// Signal that label mesh data has changed
  boost::signals2::signal<void(const UID& meshUid)> m_signalLabelMeshDataChanged;

  /// Signal that the reference image landmark group has changed
  boost::signals2::signal<void(const UID& lmGroupUid)> m_signalRefImageLandmarkGroupChanged;

  /// Signal that the slide landmark group has changed
  boost::signals2::signal<void(const UID& lmGroupUid)> m_signalSlideLandmarkGroupChanged;

  /// Signal that the slide annotation has changed
  boost::signals2::signal<void(const UID& annotUid)> m_signalSlideAnnotationChanged;
};

DataManager::DataManager()
  : m_impl(std::make_unique<Impl>())
{
}

DataManager::~DataManager() = default;

void DataManager::setProject(serialize::HZeeProject project)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_project = std::move(project);
}

const serialize::HZeeProject& DataManager::project() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_project;
}

serialize::HZeeProject& DataManager::project()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_project;
}

void DataManager::updateProject(const std::optional<std::string>& newFileName)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (newFileName)
  {
    // Update file name
    m_impl->m_project.m_fileName = *newFileName;
  }

  // Update active image index
  if (const auto& uid = activeImageUid())
  {
    if (const auto index = orderedImageIndex(*uid))
    {
      m_impl->m_project.m_activeRefImage = static_cast<uint32_t>(*index);
    }
    else
    {
      m_impl->m_project.m_activeRefImage = 0; // Shouldn't happen
    }
  }
  else
  {
    m_impl->m_project.m_activeRefImage = 0; // Shouldn't happen
  }

  // Update active parcellation index
  if (const auto& uid = activeParcellationUid())
  {
    m_impl->m_project.m_activeParcellation = orderedParcellationIndex(*uid);
  }
  else
  {
    m_impl->m_project.m_activeParcellation = std::nullopt;
  }

  // Update reference images:
  m_impl->m_project.m_refImages.clear();

  for (const auto& uid : orderedImageUids())
  {
    const auto image = imageRecord(uid).lock();

    if (image && image->cpuData())
    {
      serialize::Image serImage;

      serImage.m_fileName = image->cpuData()->header().m_fileName;

      serImage.m_world_T_subject = CoordinateFrame(
        image->cpuData()->transformations().getWorldSubjectOrigin(),
        image->cpuData()->transformations().getSubjectToWorldRotation()
      );

      const auto& S = image->cpuData()->settings();
      serImage.m_displaySettings.m_displayName = S.displayName();
      serImage.m_displaySettings.m_opacity = S.opacity(0);
      serImage.m_displaySettings.m_window = S.window(0);
      serImage.m_displaySettings.m_level = S.level(0);
      serImage.m_displaySettings.m_thresholdLow = S.thresholdLow(0);
      serImage.m_displaySettings.m_thresholdHigh = S.thresholdHigh(0);
      serImage.m_displaySettings.m_interpolationMode = S.interpolationMode(0);

      m_impl->m_project.m_refImages.emplace_back(std::move(serImage));
    }
  }

  // Update parcellations:
  m_impl->m_project.m_parcellations.clear();

  for (const auto& uid : orderedParcellationUids())
  {
    const auto parcel = parcellationRecord(uid).lock();

    if (parcel && parcel->cpuData())
    {
      serialize::Image serParcel;

      if (parcel->cpuData()->header().m_fileName.empty())
      {
        // This parcellation does not exist on disk; it was generated by the application
        continue;
      }

      serParcel.m_fileName = parcel->cpuData()->header().m_fileName;

      serParcel.m_world_T_subject = CoordinateFrame(
        parcel->cpuData()->transformations().getWorldSubjectOrigin(),
        parcel->cpuData()->transformations().getSubjectToWorldRotation()
      );

      const auto& S = parcel->cpuData()->settings();
      serParcel.m_displaySettings.m_displayName = S.displayName();
      serParcel.m_displaySettings.m_opacity = S.opacity(0);

      m_impl->m_project.m_parcellations.emplace_back(std::move(serParcel));
    }
  }

  // Update slides:
  m_impl->m_project.m_slides.clear();

  for (const auto& uid : orderedSlideUids())
  {
    const auto slide = slideRecord(uid).lock();

    if (slide && slide->cpuData())
    {
      serialize::Slide serSlide;
      serSlide.m_fileName = slide->cpuData()->header().fileName();
      serSlide.m_properties = slide->cpuData()->properties();
      serSlide.m_slideStack_T_slide = slide->cpuData()->transformation();

      m_impl->m_project.m_slides.emplace_back(std::move(serSlide));
    }
  }
}

std::optional<UID> DataManager::insertImageRecord(std::shared_ptr<ImageRecord> record)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!record)
  {
    return std::nullopt;
  }

  // Add this image and make it the last one in the ordering
  const UID imageUid;
  record->setUid(imageUid);

  m_impl->m_imageRecords[imageUid] = record;
  m_impl->m_orderedImageUids.push_back(imageUid); // keep track of ordering

  m_impl->m_signalImageDataChanged(imageUid);
  return imageUid;
}

std::optional<UID> DataManager::insertParcellationRecord(std::shared_ptr<ParcellationRecord> record)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!record)
  {
    return std::nullopt;
  }

  const UID parcelUid;
  record->setUid(parcelUid);

  m_impl->m_parcelRecords[parcelUid] = record;
  m_impl->m_orderedParcelUids.push_back(parcelUid); // keep track of ordering

  m_impl->m_signalParcellationDataChanged(parcelUid);
  return parcelUid;
}

std::optional<UID> DataManager::insertSlideRecord(std::shared_ptr<SlideRecord> record)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!record)
  {
    return std::nullopt;
  }

  const UID slideUid;
  record->setUid(slideUid);

  m_impl->m_slideRecords[slideUid] = record;
  m_impl->m_orderedSlideUids.push_back(slideUid); // keep track of ordering

  m_impl->m_signalSlideStackChanged();
  m_impl->m_signalSlideDataChanged(slideUid);
  return slideUid;
}

std::optional<UID> DataManager::insertImageColorMapRecord(std::shared_ptr<ImageColorMapRecord> record
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!record)
  {
    return std::nullopt;
  }

  const UID mapUid;
  record->setUid(mapUid);

  m_impl->m_imageColorMapRecords[mapUid] = record;
  m_impl->m_orderedImageColorMapUids.push_back(mapUid); // keep track of ordering

  m_impl->m_signalImageColorMapDataChanged(mapUid);
  return mapUid;
}

std::optional<UID> DataManager::insertLabelTableRecord(std::shared_ptr<LabelTableRecord> record)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!record)
  {
    return std::nullopt;
  }

  const UID tableUid;
  record->setUid(tableUid);

  m_impl->m_labelsRecords[tableUid] = record;

  m_impl->m_signalLabelTableDataChanged(tableUid);
  return tableUid;
}

bool DataManager::associateColorMapWithImage(const UID& imageUid, const UID& mapUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto imageIt = m_impl->m_imageRecords.find(imageUid);
  auto mapIt = m_impl->m_imageColorMapRecords.find(mapUid);

  if (std::end(m_impl->m_imageRecords) == imageIt)
  {
    std::cerr << "Image record " << imageUid << " does not exist" << std::endl;
    return false;
  }

  if (std::end(m_impl->m_imageColorMapRecords) == mapIt)
  {
    std::cerr << "Image color map record " << mapUid << " does not exist" << std::endl;
    return false;
  }

  m_impl->m_imageUid_to_imageColorMapUid[imageUid] = mapUid;

  m_impl->m_signalImageDataChanged(imageUid);
  return true;
}

bool DataManager::associateLabelTableWithParcellation(const UID& parcelUid, const UID& labelsUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto parcelIt = m_impl->m_parcelRecords.find(parcelUid);
  auto tableIt = m_impl->m_labelsRecords.find(labelsUid);

  if (std::end(m_impl->m_parcelRecords) == parcelIt)
  {
    std::cerr << "Parcellation record " << parcelUid << " does not exist" << std::endl;
    return false;
  }

  if (std::end(m_impl->m_labelsRecords) == tableIt)
  {
    std::cerr << "Label table record " << labelsUid << " does not exist" << std::endl;
    return false;
  }

  m_impl->m_parcelUid_to_labelsUid[parcelUid] = labelsUid;

  m_impl->m_signalParcellationDataChanged(parcelUid);
  m_impl->m_signalLabelTableDataChanged(labelsUid);

  return true;
}

bool DataManager::associateDefaultParcellationWithImage(const UID& imageUid, const UID& parcelUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto imageIt = m_impl->m_imageRecords.find(imageUid);
  if (std::end(m_impl->m_imageRecords) == imageIt)
  {
    std::cerr << "Image record " << imageUid << " does not exist" << std::endl;
    return false;
  }

  auto labelIt = m_impl->m_parcelRecords.find(parcelUid);
  if (std::end(m_impl->m_parcelRecords) == labelIt)
  {
    std::cerr << "Parcellation record " << parcelUid << " does not exist" << std::endl;
    return false;
  }

  m_impl->m_imageUid_to_defaultParcelUid[imageUid] = parcelUid;

  m_impl->m_signalImageDataChanged(imageUid);
  return true;
}

std::optional<UID> DataManager::insertIsoMeshRecord(
  const UID& imageUid, std::shared_ptr<MeshRecord> meshRecord
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!meshRecord)
  {
    return std::nullopt;
  }

  auto imageIt = m_impl->m_imageRecords.find(imageUid);
  if (std::end(m_impl->m_imageRecords) == imageIt)
  {
    std::cerr << "Image record " << imageUid << " does not exist" << std::endl;
    return std::nullopt;
  }

  const UID meshUid;
  meshRecord->setUid(meshUid);

  m_impl->m_isoMeshRecords[meshUid] = meshRecord;
  m_impl->m_isoMeshUid_to_imageUid[meshUid] = imageUid;
  m_impl->m_imageUid_to_isoMeshUids[imageUid].insert(meshUid);

  m_impl->m_signalImageDataChanged(imageUid);
  m_impl->m_signalIsoMeshDataChanged(meshUid);

  return meshUid;
}

std::optional<UID> DataManager::insertLabelMeshRecord(
  const UID& parcelUid, std::shared_ptr<MeshRecord> meshRecord
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!meshRecord)
  {
    return std::nullopt;
  }

  auto parcelIt = m_impl->m_parcelRecords.find(parcelUid);
  if (std::end(m_impl->m_parcelRecords) == parcelIt)
  {
    std::cerr << "Parcellation " << parcelUid << " does not exist" << std::endl;
    return std::nullopt;
  }

  if (!meshRecord || !meshRecord->cpuData())
  {
    std::cerr << "Label mesh record is null" << std::endl;
    return std::nullopt;
  }

  const UID meshUid;
  meshRecord->setUid(meshUid);

  m_impl->m_labelMeshRecords[meshUid] = meshRecord;
  m_impl->m_labelMeshUid_to_parcelUid[meshUid] = parcelUid;

  // Store mesh UID in map with key equal to its label index:
  const uint32_t labelIndex = meshRecord->cpuData()->meshInfo().labelIndex();
  m_impl->m_parcelUid_to_labelMeshUids[parcelUid][labelIndex] = meshUid;

  m_impl->m_signalParcellationDataChanged(parcelUid);
  m_impl->m_signalLabelMeshDataChanged(meshUid);

  return meshUid;
}

std::optional<UID> DataManager::insertRefImageLandmarkGroupRecord(
  const UID& imageUid, std::shared_ptr<LandmarkGroupRecord> lmGroupRecord
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!lmGroupRecord)
  {
    return std::nullopt;
  }

  auto imageIt = m_impl->m_imageRecords.find(imageUid);
  if (std::end(m_impl->m_imageRecords) == imageIt)
  {
    std::cerr << "Image record " << imageUid << " does not exist" << std::endl;
    return std::nullopt;
  }

  const UID lmGroupUid;
  lmGroupRecord->setUid(lmGroupUid);

  m_impl->m_refImageLandmarkGroupRecords[lmGroupUid] = lmGroupRecord;
  m_impl->m_refImageLandmarkGroupUid_to_imageUid[lmGroupUid] = imageUid;
  m_impl->m_imageUid_to_landmarkGroupUids[imageUid].insert(lmGroupUid);

  m_impl->m_orderedRefImageLandmarkGroupUids.push_back(lmGroupUid);

  m_impl->m_signalImageDataChanged(imageUid);
  m_impl->m_signalRefImageLandmarkGroupChanged(lmGroupUid);

  return lmGroupUid;
}

std::optional<UID> DataManager::insertSlideLandmarkGroupRecord(
  const UID& slideUid, std::shared_ptr<LandmarkGroupRecord> lmGroupRecord
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!lmGroupRecord)
  {
    return std::nullopt;
  }

  auto slideIt = m_impl->m_slideRecords.find(slideUid);
  if (std::end(m_impl->m_slideRecords) == slideIt)
  {
    std::cerr << "Slide record " << slideUid << " does not exist" << std::endl;
    return std::nullopt;
  }

  const UID lmGroupUid;
  lmGroupRecord->setUid(lmGroupUid);

  m_impl->m_slideLandmarkGroupRecords[lmGroupUid] = lmGroupRecord;
  m_impl->m_slideLandmarkGroupUid_to_slideUid[lmGroupUid] = slideUid;
  m_impl->m_slideUid_to_landmarkGroupUids[slideUid].insert(lmGroupUid);

  auto it = m_impl->m_orderedSlideLandmarkGroupUids.find(slideUid);
  if (std::end(m_impl->m_orderedSlideLandmarkGroupUids) == it)
  {
    m_impl->m_orderedSlideLandmarkGroupUids[slideUid] = std::list<UID>{lmGroupUid};
  }
  else
  {
    m_impl->m_orderedSlideLandmarkGroupUids[slideUid].push_back(lmGroupUid);
  }

  m_impl->m_signalSlideDataChanged(slideUid);
  m_impl->m_signalSlideLandmarkGroupChanged(lmGroupUid);

  return lmGroupUid;
}

std::optional<UID> DataManager::insertSlideAnnotationRecord(
  const UID& slideUid, std::shared_ptr<SlideAnnotationRecord> annotRecord
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!annotRecord)
  {
    return std::nullopt;
  }

  auto slideIt = m_impl->m_slideRecords.find(slideUid);
  if (std::end(m_impl->m_slideRecords) == slideIt)
  {
    std::cerr << "Slide record " << slideUid << " does not exist" << std::endl;
    return std::nullopt;
  }

  const UID annotUid;
  annotRecord->setUid(annotUid);

  m_impl->m_slideAnnotationRecords[annotUid] = annotRecord;
  m_impl->m_slideAnnotationUid_to_slideUid[annotUid] = slideUid;
  m_impl->m_slideUid_to_annotationUids[slideUid].insert(annotUid);

  auto it = m_impl->m_orderedSlideAnnotationUids.find(slideUid);
  if (std::end(m_impl->m_orderedSlideAnnotationUids) == it)
  {
    m_impl->m_orderedSlideAnnotationUids[slideUid] = std::list<UID>{annotUid};
  }
  else
  {
    m_impl->m_orderedSlideAnnotationUids[slideUid].push_back(annotUid);
  }

  m_impl->m_signalSlideDataChanged(slideUid);
  m_impl->m_signalSlideAnnotationChanged(annotUid);

  return annotUid;
}

bool DataManager::unloadImage(const UID& imageUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_imageRecords) == m_impl->m_imageRecords.find(imageUid))
  {
    // Image not found
    return false;
  }

  // If unloading the active image, then set the active image to be the
  // first one, if at least one exists
  if (auto activeUID = activeImageUid())
  {
    if (imageUid == *activeUID)
    {
      if (!m_impl->m_imageRecords.empty())
      {
        auto it = std::begin(m_impl->m_imageRecords);
        setActiveImageUid(it->first);
      }
      else
      {
        setActiveImageUid(std::nullopt);
      }
    }
  }

  if (m_impl->m_imageRecords.erase(imageUid) > 0)
  {
    m_impl->m_orderedImageUids.remove(imageUid);
    m_impl->m_imageUid_to_defaultParcelUid.erase(imageUid);

    m_impl->m_signalImageDataChanged(imageUid);
    return true;
  }

  return false;
}

bool DataManager::unloadParcellation(const UID& parcelUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_parcelRecords) == m_impl->m_parcelRecords.find(parcelUid))
  {
    // Parcellation not found
    return false;
  }

  // If unloading the active label, then clear the active label
  if (auto activeUID = activeParcellationUid())
  {
    if (parcelUid == *activeUID)
    {
      setActiveParcellationUid(std::nullopt);
    }
  }

  if (m_impl->m_parcelRecords.erase(parcelUid) > 0)
  {
    m_impl->m_orderedParcelUids.remove(parcelUid);
    m_impl->m_parcelUid_to_labelsUid.erase(parcelUid);
    m_impl->m_signalParcellationDataChanged(parcelUid);
    return true;
  }

  return false;
}

bool DataManager::unloadSlide(const UID& slideUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_slideRecords) == m_impl->m_slideRecords.find(slideUid))
  {
    // Slide not found
    return false;
  }

  // If unloading the active slide, then set the active slide to the previous one
  // according to the ordering.

  const auto activeUid = activeSlideUid();
  const auto activeIndex = activeSlideIndex();

  if (activeUid && activeIndex && slideUid == *activeUid)
  {
    const size_t newActiveIndex = (*activeIndex > 0) ? (*activeIndex - 1) : 0;
    setActiveSlideIndex(newActiveIndex);
  }

  if (m_impl->m_slideRecords.erase(slideUid) > 0)
  {
    m_impl->m_orderedSlideUids.remove(slideUid);

    // Clear the active slide if there are no slides left
    if (m_impl->m_slideRecords.empty())
    {
      m_impl->m_activeSlideUid = std::nullopt;
    }

    m_impl->m_signalSlideStackChanged();
    return true;
  }

  return false;
}

bool DataManager::unloadLabelMesh(const UID& meshUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_labelMeshRecords) == m_impl->m_labelMeshRecords.find(meshUid))
  {
    // Mesh not found
    return false;
  }

  if (m_impl->m_labelMeshRecords.erase(meshUid) > 0)
  {
    auto it = m_impl->m_labelMeshUid_to_parcelUid.find(meshUid);
    if (std::end(m_impl->m_labelMeshUid_to_parcelUid) != it)
    {
      // Parcellation for the mesh
      const UID parcelUid = it->second;

      auto it2 = m_impl->m_parcelUid_to_labelMeshUids.find(parcelUid);
      if (std::end(m_impl->m_parcelUid_to_labelMeshUids) != it2)
      {
        // Find label indices with value equal to meshUid. (There should be only one!)
        std::vector<uint32_t> indices;
        if (findByValue(indices, it2->second, meshUid))
        {
          for (const uint32_t index : indices)
          {
            it2->second.erase(index);
          }
        }
      }
    }

    m_impl->m_labelMeshUid_to_parcelUid.erase(meshUid);
    m_impl->m_signalLabelMeshDataChanged(meshUid);
    return true;
  }

  return false;
}

bool DataManager::unloadIsoMesh(const UID& meshUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_isoMeshRecords) == m_impl->m_isoMeshRecords.find(meshUid))
  {
    // Mesh not found
    return false;
  }

  if (m_impl->m_isoMeshRecords.erase(meshUid) > 0)
  {
    auto it = m_impl->m_isoMeshUid_to_imageUid.find(meshUid);
    if (std::end(m_impl->m_isoMeshUid_to_imageUid) != it)
    {
      // Image for the mesh
      const UID imageUid = it->second;

      auto it2 = m_impl->m_imageUid_to_isoMeshUids.find(imageUid);
      if (std::end(m_impl->m_imageUid_to_isoMeshUids) != it2)
      {
        it2->second.erase(meshUid);
      }
    }

    m_impl->m_isoMeshUid_to_imageUid.erase(meshUid);
    m_impl->m_signalIsoMeshDataChanged(meshUid);
    return true;
  }

  return false;
}

bool DataManager::unloadLabelTable(const UID& labelsUID)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_labelsRecords) == m_impl->m_labelsRecords.find(labelsUID))
  {
    return false;
  }

  if (m_impl->m_labelsRecords.erase(labelsUID) > 0)
  {
    // Remove all instances of labelsUID from m_parcelUid_to_labelsUID
    for (auto it = std::begin(m_impl->m_parcelUid_to_labelsUid);
         it != std::end(m_impl->m_parcelUid_to_labelsUid);)
    {
      if (labelsUID == it->second)
      {
        m_impl->m_parcelUid_to_labelsUid.erase(it++);
      }
      else
      {
        ++it;
      }
    }

    m_impl->m_signalLabelTableDataChanged(labelsUID);
    return true;
  }

  return false;
}

bool DataManager::unloadRefImageLandmarkGroup(const UID& lmGroupUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_refImageLandmarkGroupRecords) == m_impl->m_refImageLandmarkGroupRecords.find(lmGroupUid))
  {
    // UID not found
    return false;
  }

  if (m_impl->m_refImageLandmarkGroupRecords.erase(lmGroupUid) > 0)
  {
    m_impl->m_orderedRefImageLandmarkGroupUids.remove(lmGroupUid);

    auto it = m_impl->m_refImageLandmarkGroupUid_to_imageUid.find(lmGroupUid);
    if (std::end(m_impl->m_refImageLandmarkGroupUid_to_imageUid) != it)
    {
      // Image for the landmark group
      const UID imageUid = it->second;

      auto it2 = m_impl->m_imageUid_to_landmarkGroupUids.find(imageUid);
      if (std::end(m_impl->m_imageUid_to_landmarkGroupUids) != it2)
      {
        it2->second.erase(lmGroupUid);
      }
    }

    m_impl->m_refImageLandmarkGroupUid_to_imageUid.erase(lmGroupUid);
    m_impl->m_signalRefImageLandmarkGroupChanged(lmGroupUid);
    return true;
  }

  return false;
}

bool DataManager::unloadSlideLandmarkGroup(const UID& lmGroupUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_slideLandmarkGroupRecords) == m_impl->m_slideLandmarkGroupRecords.find(lmGroupUid))
  {
    // UID not found
    return false;
  }

  if (m_impl->m_slideLandmarkGroupRecords.erase(lmGroupUid) > 0)
  {
    auto it = m_impl->m_slideLandmarkGroupUid_to_slideUid.find(lmGroupUid);
    if (std::end(m_impl->m_slideLandmarkGroupUid_to_slideUid) != it)
    {
      // Slide for the landmark group
      const UID slideUid = it->second;

      m_impl->m_orderedSlideLandmarkGroupUids[slideUid].remove(lmGroupUid);

      auto it2 = m_impl->m_slideUid_to_landmarkGroupUids.find(slideUid);
      if (std::end(m_impl->m_slideUid_to_landmarkGroupUids) != it2)
      {
        it2->second.erase(lmGroupUid);
      }
    }

    m_impl->m_slideLandmarkGroupUid_to_slideUid.erase(lmGroupUid);
    m_impl->m_signalSlideLandmarkGroupChanged(lmGroupUid);
    return true;
  }

  return false;
}

bool DataManager::unloadSlideAnnotation(const UID& annotUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (std::end(m_impl->m_slideAnnotationRecords) == m_impl->m_slideAnnotationRecords.find(annotUid))
  {
    // UID not found
    return false;
  }

  if (m_impl->m_slideAnnotationRecords.erase(annotUid) > 0)
  {
    auto it = m_impl->m_slideAnnotationUid_to_slideUid.find(annotUid);
    if (std::end(m_impl->m_slideAnnotationUid_to_slideUid) != it)
    {
      // Slide for the annotation
      const UID slideUid = it->second;

      m_impl->m_orderedSlideAnnotationUids[slideUid].remove(annotUid);

      auto it2 = m_impl->m_slideUid_to_annotationUids.find(slideUid);
      if (std::end(m_impl->m_slideUid_to_annotationUids) != it2)
      {
        it2->second.erase(slideUid);
      }
    }

    m_impl->m_slideAnnotationUid_to_slideUid.erase(annotUid);
    m_impl->m_signalSlideAnnotationChanged(annotUid);
    return true;
  }

  return false;
}

std::optional<UID> DataManager::activeImageUid() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_activeImageUids;
}

std::optional<UID> DataManager::activeParcellationUid() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_activeParcelUids;
}

std::optional<UID> DataManager::activeSlideUid() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_activeSlideUid;
}

std::optional<size_t> DataManager::activeSlideIndex() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!m_impl->m_activeSlideUid)
  {
    return std::nullopt;
  }

  return slideIndex(*m_impl->m_activeSlideUid);
}

std::optional<size_t> DataManager::slideIndex(const UID& slideUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = std::find(
    std::begin(m_impl->m_orderedSlideUids), std::end(m_impl->m_orderedSlideUids), slideUid
  );

  if (std::end(m_impl->m_orderedSlideUids) != it)
  {
    auto d = std::distance(std::begin(m_impl->m_orderedSlideUids), it);
    if (d < 0)
    {
      return std::nullopt;
    }

    return static_cast<size_t>(d);
  }

  return std::nullopt;
}

bool DataManager::setActiveImageUid(const std::optional<UID>& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!uid)
  {
    m_impl->m_activeImageUids = std::nullopt;
    /// m_impl->m_signalImageDataChanged( *uid );
    return true;
  }

  // Set active image UID if it is valid
  auto it = m_impl->m_imageRecords.find(*uid);
  if (std::end(m_impl->m_imageRecords) != it)
  {
    m_impl->m_activeImageUids = *uid;

    m_impl->m_signalImageDataChanged(*uid);
    return true;
  }

  return false;
}

bool DataManager::setActiveParcellationUid(const std::optional<UID>& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (!uid)
  {
    // Passed in std::nullopt, which means that the active label is cleared
    m_impl->m_activeParcelUids = std::nullopt;
    /// m_impl->m_signalParcellationDataChanged( *uid );
    return true;
  }

  // Set active label UID if it is for a valid label record
  auto it = m_impl->m_parcelRecords.find(*uid);
  if (std::end(m_impl->m_parcelRecords) != it)
  {
    m_impl->m_activeParcelUids = *uid;
    m_impl->m_signalParcellationDataChanged(*uid);
    return true;
  }

  return false;
}

bool DataManager::setActiveSlideUid(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  // Set active slide UID, if it is valid
  auto it = m_impl->m_slideRecords.find(uid);
  if (std::end(m_impl->m_slideRecords) != it)
  {
    if (m_impl->m_activeSlideUid != uid)
    {
      m_impl->m_activeSlideUid = uid;
      m_impl->m_signalActiveSlideChanged(uid);
      m_impl->m_signalSlideDataChanged(uid);
      return true;
    }
  }

  return false;
}

bool DataManager::setActiveSlideIndex(size_t slideIndex)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (m_impl->m_orderedSlideUids.size() <= slideIndex)
  {
    return false; // Invalid index
  }

  auto it = std::next(std::begin(m_impl->m_orderedSlideUids), static_cast<long>(slideIndex));

  if (m_impl->m_activeSlideUid != *it)
  {
    m_impl->m_activeSlideUid = *it;
    m_impl->m_signalActiveSlideChanged(*it);
    m_impl->m_signalSlideDataChanged(*it);
    return true;
  }

  return false;
}

bool DataManager::setDefaultImageColorMapUid(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  // Set default color map UID if it is valid
  auto it = m_impl->m_imageColorMapRecords.find(uid);
  if (std::end(m_impl->m_imageColorMapRecords) != it)
  {
    m_impl->m_defaultImageColorMapUid = uid;
    m_impl->m_signalImageColorMapDataChanged(uid);
    return true;
  }

  return false;
}

bool DataManager::setSlideOrder(const std::list<UID>& orderedSlideUids)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (m_impl->m_orderedSlideUids.size() != orderedSlideUids.size() || !compareListContents(m_impl->m_orderedSlideUids, orderedSlideUids))
  {
    return false;
  }

  if (m_impl->m_orderedSlideUids != orderedSlideUids)
  {
    m_impl->m_orderedSlideUids = orderedSlideUids;
    m_impl->m_signalSlideStackChanged();
    return true;
  }

  return false;
}

std::optional<UID> DataManager::defaultParcellationUid_of_image(const UID& imageUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_imageUid_to_defaultParcelUid.find(imageUid);
  if (std::end(m_impl->m_imageUid_to_defaultParcelUid) != it)
  {
    return it->second;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::parcellationUid_of_labelMesh(const UID& labelMeshUID) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_labelMeshUid_to_parcelUid.find(labelMeshUID);
  if (std::end(m_impl->m_labelMeshUid_to_parcelUid) != it)
  {
    return it->second;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::imageUid_of_isoMesh(const UID& isoMeshUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_isoMeshUid_to_imageUid.find(isoMeshUid);
  if (std::end(m_impl->m_isoMeshUid_to_imageUid) != it)
  {
    return it->second;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::imageColorMapUid_of_image(const UID& imageUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_imageUid_to_imageColorMapUid.find(imageUid);
  if (std::end(m_impl->m_imageUid_to_imageColorMapUid) != it)
  {
    return it->second;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::labelTableUid_of_parcellation(const UID& parcelUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_parcelUid_to_labelsUid.find(parcelUid);
  if (std::end(m_impl->m_parcelUid_to_labelsUid) != it)
  {
    return it->second;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::defaultImageColorMapUid() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_defaultImageColorMapUid;
}

std::optional<UID> DataManager::imageUid_of_landmarkGroup(const UID& lmGroupUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_refImageLandmarkGroupUid_to_imageUid.find(lmGroupUid);
  if (std::end(m_impl->m_refImageLandmarkGroupUid_to_imageUid) != it)
  {
    return it->second;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::slideUid_of_landmarkGroup(const UID& lmGroupUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_slideLandmarkGroupUid_to_slideUid.find(lmGroupUid);
  if (std::end(m_impl->m_slideLandmarkGroupUid_to_slideUid) != it)
  {
    return it->second;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::slideUid_of_annotation(const UID& annotUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_slideAnnotationUid_to_slideUid.find(annotUid);
  if (std::end(m_impl->m_slideAnnotationUid_to_slideUid) != it)
  {
    return it->second;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::orderedImageUid(long index)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (0 <= index && static_cast<size_t>(index) < m_impl->m_orderedImageUids.size())
  {
    auto it = std::begin(m_impl->m_orderedImageUids);
    std::advance(it, index);
    return *it;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::orderedParcellationUid(long index)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (0 <= index && static_cast<size_t>(index) < m_impl->m_orderedParcelUids.size())
  {
    auto it = std::begin(m_impl->m_orderedParcelUids);
    std::advance(it, index);
    return *it;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::orderedSlideUid(long index)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (0 <= index && static_cast<size_t>(index) < m_impl->m_orderedSlideUids.size())
  {
    auto it = std::begin(m_impl->m_orderedSlideUids);
    std::advance(it, index);
    return *it;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::orderedImageColorMapUid(long index)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (0 <= index && static_cast<size_t>(index) < m_impl->m_orderedImageColorMapUids.size())
  {
    return m_impl->m_orderedImageColorMapUids.at(static_cast<size_t>(index));
  }

  return std::nullopt;
}

/// @todo Ordering is kept for ALL images. Change this to order PER image.
std::optional<UID> DataManager::orderedRefImageLandmarkGroupUid(const UID& /*imageUid*/, long index)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (0 <= index && static_cast<size_t>(index) < m_impl->m_orderedRefImageLandmarkGroupUids.size())
  {
    auto it = std::begin(m_impl->m_orderedRefImageLandmarkGroupUids);
    std::advance(it, index);
    return *it;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::orderedSlideLandmarkGroupUid(const UID& slideUid, long index)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_orderedSlideLandmarkGroupUids.find(slideUid);
  if (std::end(m_impl->m_orderedSlideLandmarkGroupUids) == it)
  {
    return std::nullopt;
  }

  auto& orderedLmList = it->second;

  if (0 <= index && static_cast<size_t>(index) < orderedLmList.size())
  {
    auto it = std::begin(orderedLmList);
    std::advance(it, index);
    return *it;
  }

  return std::nullopt;
}

std::optional<UID> DataManager::orderedSlideAnnotationUid(const UID& slideUid, long index)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_orderedSlideAnnotationUids.find(slideUid);
  if (std::end(m_impl->m_orderedSlideAnnotationUids) == it)
  {
    return std::nullopt;
  }

  auto& orderedAnnotList = it->second;

  if (0 <= index && static_cast<size_t>(index) < orderedAnnotList.size())
  {
    auto it = std::begin(orderedAnnotList);
    std::advance(it, index);
    return *it;
  }

  return std::nullopt;
}

std::optional<long> DataManager::orderedImageIndex(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it
    = std::find(std::begin(m_impl->m_orderedImageUids), std::end(m_impl->m_orderedImageUids), uid);

  if (std::end(m_impl->m_orderedImageUids) != it)
  {
    return std::distance(std::begin(m_impl->m_orderedImageUids), it);
  }

  return std::nullopt;
}

std::optional<long> DataManager::orderedParcellationIndex(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it
    = std::find(std::begin(m_impl->m_orderedParcelUids), std::end(m_impl->m_orderedParcelUids), uid);

  if (std::end(m_impl->m_orderedParcelUids) != it)
  {
    return std::distance(std::begin(m_impl->m_orderedParcelUids), it);
  }

  return std::nullopt;
}

std::optional<long> DataManager::orderedSlideIndex(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it
    = std::find(std::begin(m_impl->m_orderedSlideUids), std::end(m_impl->m_orderedSlideUids), uid);

  if (std::end(m_impl->m_orderedSlideUids) != it)
  {
    return std::distance(std::begin(m_impl->m_orderedSlideUids), it);
  }

  return std::nullopt;
}

std::optional<long> DataManager::orderedImageColorMapIndex(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = std::find(
    std::begin(m_impl->m_orderedImageColorMapUids), std::end(m_impl->m_orderedImageColorMapUids), uid
  );

  if (std::end(m_impl->m_orderedImageColorMapUids) != it)
  {
    return std::distance(std::begin(m_impl->m_orderedImageColorMapUids), it);
  }

  return std::nullopt;
}

/// @todo Use the imageUid. Lms are not currently ordered per image.
std::optional<long> DataManager::orderedRefImageLandmarkGroupIndex(
  const UID& /*imageUid*/, const UID& lmGroupUid
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = std::find(
    std::begin(m_impl->m_orderedRefImageLandmarkGroupUids),
    std::end(m_impl->m_orderedRefImageLandmarkGroupUids),
    lmGroupUid
  );

  if (std::end(m_impl->m_orderedRefImageLandmarkGroupUids) != it)
  {
    return std::distance(std::begin(m_impl->m_orderedRefImageLandmarkGroupUids), it);
  }

  return std::nullopt;
}

/// @todo Make common functionality...
std::optional<long> DataManager::orderedSlideLandmarkGroupIndex(
  const UID& slideUid, const UID& lmGroupUid
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it2 = m_impl->m_orderedSlideLandmarkGroupUids.find(slideUid);
  if (std::end(m_impl->m_orderedSlideLandmarkGroupUids) == it2)
  {
    return std::nullopt;
  }

  auto& orderedLmList = it2->second;

  auto it = std::find(std::begin(orderedLmList), std::end(orderedLmList), lmGroupUid);

  if (std::end(orderedLmList) != it)
  {
    return std::distance(std::begin(orderedLmList), it);
  }

  return std::nullopt;
}

/// @todo Make common functionality...
std::optional<long> DataManager::orderedSlideAnnotationIndex(const UID& slideUid, const UID& annotUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it2 = m_impl->m_orderedSlideAnnotationUids.find(slideUid);
  if (std::end(m_impl->m_orderedSlideAnnotationUids) == it2)
  {
    return std::nullopt;
  }

  auto& orderedAnnotList = it2->second;

  auto it = std::find(std::begin(orderedAnnotList), std::end(orderedAnnotList), annotUid);

  if (std::end(orderedAnnotList) != it)
  {
    return std::distance(std::begin(orderedAnnotList), it);
  }

  return std::nullopt;
}

uid_range_t DataManager::orderedImageUids() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_orderedImageUids;
}

uid_range_t DataManager::orderedParcellationUids() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_orderedParcelUids;
}

uid_range_t DataManager::orderedSlideUids() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_orderedSlideUids;
}

uid_range_t DataManager::orderedRefImageLandmarkGroupUids() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_orderedRefImageLandmarkGroupUids;
}

uid_range_t DataManager::orderedSlideLandmarkGroupUids(const UID& slideUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_orderedSlideLandmarkGroupUids[slideUid];
}

uid_range_t DataManager::orderedSlideAnnotationUids(const UID& slideUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_orderedSlideAnnotationUids[slideUid];
}

uid_range_t DataManager::isoMeshUids() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_isoMeshRecords | boost::adaptors::map_keys;
}

uid_range_t DataManager::labelMeshUids() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_labelMeshRecords | boost::adaptors::map_keys;
}

uid_range_t DataManager::orderedImageColorMapUids() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_orderedImageColorMapUids;
}

uid_range_t DataManager::labelTableUids() const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return m_impl->m_labelsRecords | boost::adaptors::map_keys;
}

uid_range_t DataManager::isoMeshUids_of_image(const UID& imageUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_imageUid_to_isoMeshUids.find(imageUid);
  if (std::end(m_impl->m_imageUid_to_isoMeshUids) != it)
  {
    return it->second;
  }

  return {};
}

uid_range_t DataManager::landmarkGroupUids_of_image(const UID& imageUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_imageUid_to_landmarkGroupUids.find(imageUid);
  if (std::end(m_impl->m_imageUid_to_landmarkGroupUids) != it)
  {
    return it->second;
  }

  return {};
}

uid_range_t DataManager::landmarkGroupUids_of_slide(const UID& slideUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_slideUid_to_landmarkGroupUids.find(slideUid);
  if (std::end(m_impl->m_slideUid_to_landmarkGroupUids) != it)
  {
    return it->second;
  }

  return {};
}

uid_range_t DataManager::annotationUids_of_slide(const UID& slideUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_slideUid_to_annotationUids.find(slideUid);
  if (std::end(m_impl->m_slideUid_to_annotationUids) != it)
  {
    return it->second;
  }

  return {};
}

std::map<uint32_t, UID> DataManager::labelMeshUids_of_parcellation(const UID& parcelUid) const
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_parcelUid_to_labelMeshUids.find(parcelUid);
  if (std::end(m_impl->m_parcelUid_to_labelMeshUids) != it)
  {
    return it->second;
  }

  return {};
}

std::weak_ptr<ImageRecord> DataManager::activeImageRecord()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (auto uid = activeImageUid())
  {
    return imageRecord(*uid);
  }
  return {};
}

std::weak_ptr<ParcellationRecord> DataManager::activeParcellationRecord()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (auto uid = activeParcellationUid())
  {
    return parcellationRecord(*uid);
  }
  return {};
}

std::weak_ptr<SlideRecord> DataManager::activeSlideRecord()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  if (auto uid = activeSlideUid())
  {
    return slideRecord(*uid);
  }
  return {};
}

std::weak_ptr<ImageRecord> DataManager::imageRecord(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_imageRecords.find(uid);
  if (std::end(m_impl->m_imageRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<ParcellationRecord> DataManager::parcellationRecord(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_parcelRecords.find(uid);
  if (std::end(m_impl->m_parcelRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<MeshRecord> DataManager::isoMeshRecord(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_isoMeshRecords.find(uid);
  if (std::end(m_impl->m_isoMeshRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<MeshRecord> DataManager::labelMeshRecord(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_labelMeshRecords.find(uid);
  if (std::end(m_impl->m_labelMeshRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<SlideRecord> DataManager::slideRecord(const UID& uid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_slideRecords.find(uid);
  if (std::end(m_impl->m_slideRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<ImageColorMapRecord> DataManager::imageColorMapRecord(const UID& mapUID)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_imageColorMapRecords.find(mapUID);
  if (std::end(m_impl->m_imageColorMapRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<LabelTableRecord> DataManager::labelTableRecord(const UID& tableUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_labelsRecords.find(tableUid);
  if (std::end(m_impl->m_labelsRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<LandmarkGroupRecord> DataManager::refImageLandmarkGroupRecord(const UID& lmGroupUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_refImageLandmarkGroupRecords.find(lmGroupUid);
  if (std::end(m_impl->m_refImageLandmarkGroupRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<LandmarkGroupRecord> DataManager::slideLandmarkGroupRecord(const UID& lmGroupUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_slideLandmarkGroupRecords.find(lmGroupUid);
  if (std::end(m_impl->m_slideLandmarkGroupRecords) != it)
  {
    return it->second;
  }
  return {};
}

std::weak_ptr<SlideAnnotationRecord> DataManager::slideAnnotationRecord(const UID& annotUid)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }

  auto it = m_impl->m_slideAnnotationRecords.find(annotUid);
  if (std::end(m_impl->m_slideAnnotationRecords) != it)
  {
    return it->second;
  }
  return {};
}

DataManager::weak_record_range_t<ImageRecord> DataManager::imageRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_imageRecords);
}

DataManager::weak_record_range_t<ParcellationRecord> DataManager::parcellationRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_parcelRecords);
}

DataManager::weak_record_range_t<MeshRecord> DataManager::isoMeshRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_isoMeshRecords);
}

DataManager::weak_record_range_t<MeshRecord> DataManager::labelMeshRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_labelMeshRecords);
}

DataManager::weak_record_range_t<SlideRecord> DataManager::slideRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_slideRecords);
}

DataManager::weak_record_range_t<ImageColorMapRecord> DataManager::imageColorMapRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_imageColorMapRecords);
}

DataManager::weak_record_range_t<LabelTableRecord> DataManager::labelTableRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_labelsRecords);
}

DataManager::weak_record_range_t<LandmarkGroupRecord> DataManager::refImageLandmarkGroupRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_refImageLandmarkGroupRecords);
}

DataManager::weak_record_range_t<LandmarkGroupRecord> DataManager::slideLandmarkGroupRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_slideLandmarkGroupRecords);
}

DataManager::weak_record_range_t<SlideAnnotationRecord> DataManager::slideAnnotationRecords()
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  return transformSharedToWeakRecords(m_impl->m_slideAnnotationRecords);
}

void DataManager::connectToImageDataChangedSignal(std::function<void(const UID& imageUid)> slot)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalImageDataChanged.connect(slot);
}

void DataManager::connectToParcellationDataChangedSignal(
  std::function<void(const UID& parcelUid)> slot
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalParcellationDataChanged.connect(slot);
}

void DataManager::connectToLabelTableDataChangedSignal(
  std::function<void(const UID& labelTabelUid)> slot
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalLabelTableDataChanged.connect(slot);
}

void DataManager::connectToSlideDataChangedSignal(std::function<void(const UID& slideUid)> slot)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalSlideDataChanged.connect(slot);
}

void DataManager::connectToSlideStackChangedSignal(std::function<void(void)> slot)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalSlideStackChanged.connect(slot);
}

void DataManager::connectToActiveSlideChangedSignal(
  std::function<void(const UID& activeSlideUid)> slot
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalActiveSlideChanged.connect(slot);
}

void DataManager::connectToRefImageLandmarkGroupChangedSignal(
  std::function<void(const UID& lmGroupUid)> slot
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalRefImageLandmarkGroupChanged.connect(slot);
}

void DataManager::connectToSlideLandmarkGroupChangedSignal(
  std::function<void(const UID& lmGroupUid)> slot
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalSlideLandmarkGroupChanged.connect(slot);
}

void DataManager::connectToSlideAnnotationChangedSignal(std::function<void(const UID& annotUid)> slot
)
{
  if (!m_impl)
  {
    throw_debug("Null impl")
  }
  m_impl->m_signalSlideAnnotationChanged.connect(slot);
}
