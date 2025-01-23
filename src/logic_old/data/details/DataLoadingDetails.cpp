#include "logic/data/details/DataLoadingDetails.h"

#include "imageio/ImageLoader.h"
#include "logic/managers/DataManager.h"
#include "mesh/MeshLoading.h"
#include "rendering/utility/CreateGLObjects.h"
#include "slideio/SlideReading.h"

#include <boost/filesystem.hpp>
#include <boost/tokenizer.hpp>

#include <QFile>
#include <QString>
#include <QTextStream>

#include <fstream>
#include <iostream>
#include <sstream>

namespace
{

// Only load the first (0th) component of images
static constexpr uint32_t sk_compIndex = 0;

} // namespace

namespace data
{

namespace details
{

// A maximum of 2^16 labels are supported. This is way too much for most reasonable purposes.
static constexpr size_t sk_maxNumLabels = (1 << 16);

// By default, 512 labels are used.
static constexpr size_t sk_defaultNumLabels = 512;

std::unique_ptr<imageio::ImageCpuRecord> generateImageCpuRecord(
  const std::string& filename,
  const std::optional<std::string>& dicomSeriesUid,
  const imageio::ComponentNormalizationPolicy& normPolicy
)
{
  // Cast image components to an OpenGL-compatible format. This means that
  // 64-bit signed/unsigned integers (int64_t/uint64_t) and 64-bit double-precision
  // floats are cast to 32-bit floats.
  imageio::ImageLoader imageLoader(imageio::ComponentTypeCastPolicy::ToOpenGLCompatible);

  auto cpuRecord = imageLoader.load(filename, dicomSeriesUid, normPolicy);

  if (!cpuRecord)
  {
    std::ostringstream ss;
    ss << "Unable to load image from file '" << filename << "'" << std::ends;
    std::cerr << ss.str() << std::endl;
    return nullptr;
  }

  const auto dim = cpuRecord->header().m_numDimensions;

  if (dim > 3)
  {
    std::ostringstream ss;
    ss << "Unable to load image of dimension " << dim << " (greater than 3)." << std::ends;
    std::cerr << ss.str() << std::endl;
    return nullptr;
  }

  return cpuRecord;
}

std::unique_ptr<ImageColorMap> loadImageColorMapWithQt(const std::string& path)
{
  using char_separator = boost::char_separator<char>;
  using tokenizer = boost::tokenizer<char_separator>;

  static const char_separator comma(",");

  QFile file(QString(path.c_str()));

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    return nullptr;
  }

  // Read names and description from first three lines of file
  std::string briefName;
  std::string technicalName;
  std::string description;

  QTextStream stream(&file);

  QString line = stream.readLine();
  if (!line.isNull())
  {
    briefName = line.toStdString();
  }
  else
  {
    file.close();
    return nullptr;
  }

  line = stream.readLine();
  if (!line.isNull())
  {
    technicalName = line.toStdString();
  }
  else
  {
    file.close();
    return nullptr;
  }

  line = stream.readLine();
  if (!line.isNull())
  {
    description = line.toStdString();
  }
  else
  {
    file.close();
    return nullptr;
  }

  // Read a color from each line of the file
  std::vector<glm::vec3> colors;

  while (stream.readLineInto(&line))
  {
    std::string s = line.toStdString();
    tokenizer tok(s, comma);

    std::vector<std::string> c;
    c.assign(tok.begin(), tok.end());

    if (c.size() == 3)
    {
      const float r = std::stof(c[0], nullptr);
      const float g = std::stof(c[1], nullptr);
      const float b = std::stof(c[2], nullptr);

      colors.push_back(glm::vec3{r, g, b});
    }
    else
    {
      std::cout << "Failed parse of " << line.toStdString() << std::endl;
      file.close();
      return nullptr;
    }
  }

  file.close();

  if (!colors.empty())
  {
    std::cout << "Loaded image color map containing " << colors.size() << " colors from file "
              << path << std::endl;

    return std::make_unique<ImageColorMap>(
      std::move(briefName), std::move(technicalName), std::move(description), std::move(colors)
    );
  }

  return nullptr;
}

#if 0
std::unique_ptr<ImageColorMap> loadImageColorMapWithStdLib( const std::string& path )
{
    std::ifstream file( path );

    if ( ! file.is_open() )
    {
        return nullptr;
    }

    // Read names and description from first three lines of file
    std::string briefName;
    std::string technicalName;
    std::string description;

    std::string line;

    if ( getline( file, line ) ) {
        briefName = line;
    }
    else {
        return nullptr;
    }

    if ( getline( file, line ) ) {
        technicalName = line;
    }
    else {
        return nullptr;
    }

    if ( getline( file, line ) ) {
        description = line;
    }
    else {
        return nullptr;
    }


    // Read a color from each line of the file
    std::vector< glm::vec3 > colors;

    while ( getline( file, line ) )
    {
        boost::tokenizer< boost::escaped_list_separator<char> > tok( line );

        std::vector< std::string > c;
        c.assign( tok.begin(), tok.end() );

        if ( c.size() == 3 )
        {
            const float r = std::stof( c[0], nullptr );
            const float g = std::stof( c[1], nullptr );
            const float b = std::stof( c[2], nullptr );

            colors.push_back( glm::vec3{ r, g, b } );
        }
    }

    if ( ! colors.empty() )
    {
        std::cout << "Loaded image color map from file " << path << std::endl;

        return std::make_unique<ImageColorMap>(
                    std::move( briefName ),
                    std::move( technicalName ),
                    std::move( description ),
                    std::move( colors ) );
    }

    return nullptr;
}
#endif

std::vector<std::unique_ptr<ImageColorMap> > loadImageColorMapsFromDirectory(const std::string& path)
{
  std::vector<std::unique_ptr<ImageColorMap> > colorMaps;

  const boost::filesystem::directory_iterator begin(path);
  const boost::filesystem::directory_iterator end;

  for (auto i = begin; i != end; ++i)
  {
    if (!boost::filesystem::is_regular_file(i->status()))
    {
      continue;
    }

    if (auto colorMap = loadImageColorMapWithQt(i->path().string()))
    {
      colorMaps.emplace_back(std::move(colorMap));
    }
  }

  return colorMaps;
}

std::shared_ptr<ImageColorMapRecord> createDefaultGreyscaleImageColorMapRecord()
{
  static const std::string briefName = "Linear grey";
  static const std::string technicalName = "linear_grey_0-100_c0_n256";
  static const std::string description = "Default linear greyscale";

  // Number of pixels in preview image of the color map
  static constexpr int sk_previewSize = 64;

  // Linearly interpolate between pure black and white
  static const std::vector<glm::vec3> colors = {glm::vec3{0.0, 0.0, 0.0}, glm::vec3{1.0, 1.0, 1.0}};

  auto mapCpuRecord = std::make_unique<ImageColorMap>(
    std::move(briefName), std::move(technicalName), std::move(description), std::move(colors)
  );

  if (!mapCpuRecord)
  {
    return nullptr;
  }

  std::vector<glm::vec4> previewColors(sk_previewSize);
  for (uint i = 0; i < sk_previewSize; ++i)
  {
    previewColors[i] = glm::vec4{glm::vec3{static_cast<float>(i) / sk_previewSize}, 1.0f};
  }

  mapCpuRecord->setPreviewMap(previewColors);

  auto mapGpuRecord = gpuhelper::createImageColorMapTexture(mapCpuRecord.get());
  if (!mapGpuRecord)
  {
    return nullptr;
  }

  return std::make_shared<ImageColorMapRecord>(std::move(mapCpuRecord), std::move(mapGpuRecord));
}

std::shared_ptr<LabelTableRecord> createLabelTableRecord(const size_t numLabels)
{
  const size_t size = std::min(numLabels, sk_maxNumLabels);

  auto labelsCpuRecord = std::make_unique<ParcellationLabelTable>(size);
  if (!labelsCpuRecord)
  {
    return nullptr;
  }

  auto labelsGpuRecord = gpuhelper::createLabelColorTableTextureBuffer(labelsCpuRecord.get());
  if (!labelsGpuRecord)
  {
    return nullptr;
  }

  return std::make_shared<LabelTableRecord>(std::move(labelsCpuRecord), std::move(labelsGpuRecord));
}

std::unique_ptr<MeshCpuRecord> generateIsoSurfaceMeshCpuRecord(
  DataManager& dataManager, const UID& imageUid, const double isoValue
)
{
  auto imageRecord = dataManager.imageRecord(imageUid).lock();

  if (!imageRecord || !imageRecord->cpuData() || !imageRecord->cpuData()->imageBaseData())
  {
    std::ostringstream ss;
    ss << "Null data in image record " << imageUid << std::ends;
    std::cerr << ss.str() << std::endl;
    return nullptr;
  }

  const auto imageData = imageRecord->cpuData()->imageBaseData()->asVTKImageData(sk_compIndex);

  if (!imageData)
  {
    std::ostringstream ss;
    ss << "Image record " << imageUid << " has null vtkImageData" << std::ends;
    std::cerr << ss.str() << std::endl;
    return nullptr;
  }

  return meshgen::generateIsoSurface(imageData.Get(), imageRecord->cpuData()->header(), isoValue);
}

std::unique_ptr<MeshCpuRecord> generateLabelMeshCpuRecord(
  DataManager& dataManager, const UID& parcelUid, const uint32_t labelIndex
)
{
  auto parcelRecord = dataManager.parcellationRecord(parcelUid).lock();

  if (!parcelRecord || !parcelRecord->cpuData() || !parcelRecord->cpuData()->imageBaseData())
  {
    std::ostringstream ss;
    ss << "Null data in parcellation " << parcelUid << std::ends;
    std::cerr << ss.str() << std::endl;
    return nullptr;
  }

  const auto parcelVtkData = parcelRecord->cpuData()->imageBaseData()->asVTKImageData(sk_compIndex);

  if (!parcelVtkData)
  {
    std::ostringstream ss;
    ss << "Parcellation " << parcelUid << " has null vtkImageData" << std::ends;
    std::cerr << ss.str() << std::endl;
    return nullptr;
  }

  return meshgen::generateLabelMesh(
    parcelVtkData.Get(), parcelRecord->cpuData()->header(), labelIndex
  );
}

std::optional<UID> generateLabelMeshRecord(
  DataManager& dataManager, const UID& parcelUid, const uint32_t labelIndex
)
{
  auto meshCpuRecord = generateLabelMeshCpuRecord(dataManager, parcelUid, labelIndex);

  if (!meshCpuRecord || !meshCpuRecord->polyData())
  {
    std::ostringstream ss;
    ss << "Unable to generate mesh CPU record at label index " << labelIndex << " for parcellation "
       << parcelUid << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  auto meshGpuRecord = gpuhelper::createMeshGpuRecordFromVtkPolyData(
    meshCpuRecord->polyData(),
    meshCpuRecord->meshInfo().primitiveType(),
    BufferUsagePattern::StreamDraw
  );

  if (!meshGpuRecord)
  {
    std::ostringstream ss;
    ss << "Unable to generate mesh GPU record at label index " << labelIndex << " for parcellation "
       << parcelUid << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  return dataManager.insertLabelMeshRecord(
    parcelUid, std::make_shared<MeshRecord>(std::move(meshCpuRecord), std::move(meshGpuRecord))
  );
}

/// Generate a blank/default parcellation image that matches the dimensions of a given image.
std::unique_ptr<imageio::ParcellationCpuRecord> generateDefaultParcellationCpuRecord(
  DataManager& dataManager, const UID& imageUid
)
{
  imageio::ImageLoader imageLoader(imageio::ComponentTypeCastPolicy::ToOpenGLCompatible);

  auto imageRecord = dataManager.imageRecord(imageUid).lock();

  if (!imageRecord)
  {
    std::ostringstream ss;
    ss << "Cannot generate default labels, since image " << imageUid << " is null" << std::ends;
    std::cerr << ss.str() << std::endl;
    return nullptr;
  }

  // Generate the clear parcellation image that matches the spatial
  // information in the header of the given source image
  return imageLoader.generateClearParcellationRecord(imageRecord->cpuData());
}

std::optional<UID> createBlankParcellation(DataManager& dataManager, const UID& imageUid)
{
  auto parcelCpuRecord = generateDefaultParcellationCpuRecord(dataManager, imageUid);

  if (!parcelCpuRecord)
  {
    std::ostringstream ss;
    ss << "Unable to generate blank parcellation for image " << imageUid << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  static constexpr bool sk_useNormalizedIntegers = false;

  auto parcelGpuRecord = gpuhelper::createImageGpuRecord(
    parcelCpuRecord.get(),
    sk_compIndex,
    tex::MinificationFilter::Nearest,
    tex::MagnificationFilter::Nearest,
    sk_useNormalizedIntegers
  );

  if (!parcelGpuRecord)
  {
    std::ostringstream ss;
    ss << "Unable to generate GPU record for blank parcellation of image " << imageUid << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  const auto defaultParcelUid = dataManager.insertParcellationRecord(
    std::make_shared<ParcellationRecord>(std::move(parcelCpuRecord), std::move(parcelGpuRecord))
  );

  if (!defaultParcelUid)
  {
    std::cerr << "Error loading blank parcellation for image " << imageUid << std::endl;
    return std::nullopt;
  }

  // Create mapping between the image and its blank (default) parcellation
  dataManager.associateDefaultParcellationWithImage(imageUid, *defaultParcelUid);

  // Create a label table for the blank parcellation
  if (auto labelTableRecord = createLabelTableRecord(sk_defaultNumLabels))
  {
    if (auto labelTableUid = dataManager.insertLabelTableRecord(labelTableRecord))
    {
      if (!dataManager.associateLabelTableWithParcellation(*defaultParcelUid, *labelTableUid))
      {
        std::ostringstream ss;
        ss << "Error associating blank label table " << *labelTableUid << " with parcellation "
           << *labelTableUid << std::ends;
        std::cerr << ss.str() << std::ends;
        return std::nullopt;
      }
    }
    else
    {
      std::ostringstream ss;
      ss << "Error inserting default label table record into DataManager" << std::ends;
      std::cerr << ss.str() << std::ends;
      return std::nullopt;
    }
  }
  else
  {
    std::ostringstream ss;
    ss << "Error creating label table for parcellation " << *defaultParcelUid << std::ends;
    std::cerr << ss.str() << std::ends;
    return std::nullopt;
  }

  std::cout << "Generated blank parcellation " << *defaultParcelUid << "for image " << imageUid
            << std::endl;

  return *defaultParcelUid;
}

std::unique_ptr<slideio::SlideCpuRecord> generateSlideCpuRecord(const std::string& filename)
{
  static const glm::vec2 sk_pixelSize(11.38f / 2011.0f, 11.38f / 2011.0f);
  static const float sk_thickness = 12.0f / 68.0f;

  return slideio::readSlide(filename, sk_pixelSize, sk_thickness);
}

} // namespace details

} // namespace data
