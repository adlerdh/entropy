#include "slideio/SlideReading.h"
#include "slideio/SlideAssociatedImages.h"
#include "slideio/SlideCpuRecord.h"

#include "rendering/utility/gl/GLTexture.h"

#include "common/Exception.hpp"

//extern "C"
//{
//#include <openslide/openslide.h>
//}

//#include <opencv2/opencv.hpp>

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <algorithm>
#include <cstdlib> // strtoul
#include <iostream>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

/**
 * @note Notes on proper handling of premultiplied ARGB pixel:
 * One of the less-obvious requirements of the OpenSlide C API is the need
 * for applications to properly handle premultiplied ARGB pixel data
 * returned by openslide_read_region() and openslide_read_associated_image().
 *
 * If this data is incorrectly treated as un-premultiplied, background areas in
 * Leica and MIRAX slides may be rendered as black pixels, and, more subtly,
 * dark lines can appear on the borders between slide regions and background.
 *
 * If un-premultiplied output is needed, the application must multiply each color
 * channel by 255 and divide by alpha.
 *
 * @see https://lists.andrew.cmu.edu/pipermail/openslide-users/2015-February/001025.html
 * @see https://github.com/openslide/openslide/wiki/PremultipliedARGB
 */

namespace
{

//static const glm::i64vec2 sk_maxSlideDimsToLoad( 4096, 4096 );
//static const glm::i64vec2 sk_maxSlideDimsForGPU( 2048, 2048 );

/**
 * @brief Check for an error in the OpenSlide reader. Print its text if present.
 * @param reader Non-owning pointer to reader
 * @return True iff there is an error
 */
//bool checkOpenSlideError( openslide_t* reader )
//{
//    if ( const char* error = openslide_get_error( reader ) )
//    {
//        openslide_close( reader );
//        std::cerr << "OpenSlide error: " << error << std::endl;
//        return true;
//    }
//    return false;
//}

//bool checkValidDims( const glm::i64vec2& dims )
//{
//    if ( dims.x <= 0 || dims.y <= 0 )
//    {
//        return false;
//    }
//    return true;
//}

//glm::vec3 readSlideBackgroundColor( openslide_t* reader )
//{
//    const char* backgroundString = openslide_get_property_value(
//                reader, OPENSLIDE_PROPERTY_NAME_BACKGROUND_COLOR );

//    if ( backgroundString )
//    {
//        // Represented as RGB hex triplet
//        const uint32_t backgroundHex = static_cast<uint32_t>(
//                    std::strtoul( backgroundString, nullptr, 16 ) );

//        const uint8_t r = ( backgroundHex >> 16 ) & 0xFF;
//        const uint8_t g = ( backgroundHex >> 8 ) & 0xFF;
//        const uint8_t b = ( backgroundHex ) & 0xFF;

//        return ( 1.0f / 255.0f * glm::vec3( r, g, b ) );
//    }

//    static const glm::vec3 sk_white( 1.0f, 1.0f, 1.0f );

//    return sk_white;
//}

//slideio::AssociatedImage readAssociatedImage( openslide_t* reader, const char* name )
//{
//    static const slideio::AssociatedImage sk_nullRet{ nullptr, {-1, -1} };

//    if ( ! reader )
//    {
//        std::cerr << "Null OpenSlide reader" << std::endl;
//        return sk_nullRet;
//    }

//    glm::i64vec2 dims;

//    // OpenSlide returns -1 dimensions if an error occurred
//    openslide_get_associated_image_dimensions( reader, name, &(dims.x), &(dims.y) );

//    if ( ! checkValidDims( dims ) )
//    {
//        return sk_nullRet;
//    }

//    auto data = std::make_shared< std::vector<uint32_t> >( static_cast<size_t>( dims.x * dims.y ) );
//    openslide_read_associated_image( reader, name, data.get()->data() );
//    return std::make_pair( data, dims );
//}

//void downsample( uint32_t* srcData, const glm::i32vec2& srcSize,
//                 uint32_t* dstData, const glm::i32vec2& dstSize )
//{
//    if ( ! srcData )
//    {
//        throw_debug( "Null source image data" );
//    }

//    // no data is copied: external data is not automatically deallocated
//    cv::Mat srcImage( cv::Size( srcSize.x, srcSize.y ), CV_8UC4, srcData, cv::Mat::AUTO_STEP );
//    cv::Mat dstImage;

//    cv::resize( srcImage, dstImage, cv::Size( dstSize.x, dstSize.y ), 0, 0, cv::INTER_AREA );

//    //    cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
//    //    cv::imshow( "Display window", slideDst );

//    if ( ! dstImage.isContinuous() )
//    {
//        dstImage = dstImage.clone();
//    }

//    // Save downsampled data from OpenCV matrix (uint8_t*) to raw uint32_t* buffer
//    std::copy( dstImage.data, dstImage.data + 4 * dstSize.x * dstSize.y,
//               reinterpret_cast< uint8_t* >( dstData ) );
//}

//slideio::SlideAssociatedImages readSlideAssociatedImages( openslide_t* reader )
//{
//    static const char* sk_thumbName = "thumbnail";
//    static const char* sk_macroName = "macro";
//    static const char* sk_labelName = "label";

//    slideio::AssociatedImage thumbImage = readAssociatedImage( reader, sk_thumbName );
//    slideio::AssociatedImage macroImage = readAssociatedImage( reader, sk_macroName );
//    slideio::AssociatedImage labelImage = readAssociatedImage( reader, sk_labelName );

//    slideio::SlideAssociatedImages images;

//    if ( thumbImage.first )
//    {
//        images.setThumbImage( thumbImage.first, thumbImage.second );
//    }

//    if ( macroImage.first )
//    {
//        images.setMacroImage( macroImage.first, macroImage.second );
//    }

//    if ( labelImage.first )
//    {
//        images.setLabelImage( labelImage.first, labelImage.second );
//    }

//    return images;
//}

} // namespace

namespace slideio
{

/// @todo Break this function into pieces
std::unique_ptr<SlideCpuRecord>
readSlide(const std::string& /*fileName*/, const glm::vec2& /*pixelSize*/, float /*thickness*/)
{
#if 0
    openslide_t* reader = openslide_open( fileName.c_str() );

    if ( ! reader )
    {
        std::cerr << "File " << fileName
                  << " is not recognized or has unsupported format." << std::endl;

        return nullptr;
    }

    if ( checkOpenSlideError( reader ) )
    {
        return nullptr;
    }

    const char* vendor = openslide_detect_vendor( fileName.c_str() );

    if ( ! vendor )
    {
        std::cerr << "Vendor for file format not recognized." << std::endl;
        return nullptr;
    }

    const int32_t k_numFileLevels = openslide_get_level_count( reader );

    if ( k_numFileLevels < 0 )
    {
        std::cerr << "An error occurred while reading image levels in slide." << std::endl;
        return nullptr;
    }


    /// @todo Log
    std::cout << "Loading slide " << fileName << std::endl;
    std::cout << "\tLevel count = " << k_numFileLevels << std::endl;


    SlideHeader header;
    header.setFileName( fileName );
    header.setVendorId( vendor );
    header.setPixelSize( pixelSize );
    header.setThickness( thickness );

    header.setAssociatedImages( readSlideAssociatedImages( reader ) );
    header.setBackgroundColor( readSlideBackgroundColor( reader ) );


    // Use stem of first image filename as its "display name"
    auto baseName = boost::filesystem::path( fileName ).stem();
    while ( baseName != baseName.stem() )
    {
        baseName = baseName.stem();
    }

    SlideProperties props;
    props.setDisplayName( baseName.string() );
    props.setVisible( true );
    props.setOpacity( 1.0f );
    props.setAnnotVisible( true );
    props.setAnnotOpacity( 1.0f );
    props.setIntensityThresholds( { 0, 255 } );

    /// @todo Change the color of each slide (rainbow)
    props.setBorderColor( glm::vec3{ 0.0f, 0.5f, 1.0f } );


    std::unique_ptr<SlideCpuRecord> cpuRecord =
            std::make_unique<SlideCpuRecord>( std::move( header ), std::move( props ) );


    for ( int i = 0; i < k_numFileLevels; ++i )
    {
        SlideLevel level;
        level.m_level = i;

        openslide_get_level_dimensions( reader, i, &(level.m_dims.x), &(level.m_dims.y) );

        // Not used: double downsampleFactor = openslide_get_level_downsample( reader, i );

        if ( checkOpenSlideError( reader ) )
        {
            return nullptr;
        }

        if ( ! checkValidDims( level.m_dims ) )
        {
            std::cerr << "Dimensions of slide " << i << " are out of valid range." << std::endl;
            return nullptr;
        }

        if ( 0 == i )
        {
            // First level is defined to have 1.0 downsample factors in x and y
            level.m_downsampleFactors = glm::dvec2{ 1.0, 1.0 };
        }
        else if ( cpuRecord->numFileLevels() > 0 )
        {
            const glm::i64vec2 baseDims = cpuRecord->fileLevel( 0 ).m_dims;
            level.m_downsampleFactors.x = baseDims.x / static_cast<double>( level.m_dims.x );
            level.m_downsampleFactors.y = baseDims.y / static_cast<double>( level.m_dims.y );
        }
        else
        {
            // error
            return nullptr;
        }

        std::cout << "\tdims[" << i << "] = "
                  << glm::to_string( level.m_dims ) << std::endl;

        std::cout << "\tdownsampleFactor[" << i << "] = "
                  << glm::to_string( level.m_downsampleFactors ) << std::endl;

        ///////////////////// Only load data for last level:
        ///////////////////////if ( ( k_numFileLevels - 1 ) == i )
        // temporarily, load only the first level:
        if ( 0 == i )
        {
            if ( glm::any( glm::greaterThan( level.m_dims, sk_maxSlideDimsToLoad ) ) )
            {
                std::cerr << "Slide level " << i << " dimensions "
                          << glm::to_string( level.m_dims )
                          << " exceed maximum size "
                          << glm::to_string( sk_maxSlideDimsToLoad ) << std::endl;

                return nullptr;
            }

            level.m_data = std::make_unique< uint32_t[] >(
                        static_cast<size_t>( level.m_dims.x * level.m_dims.y ) );

            // Copy pre-multiplied ARGB data from a whole slide image
            openslide_read_region( reader, level.m_data.get(), 0, 0,
                                   i, level.m_dims.x, level.m_dims.y );

            if ( ! level.m_data )
            {
                std::cerr << "Unable to read data for slide level " << i << std::endl;
                return nullptr;
            }
        }
        else
        {
            level.m_data = nullptr;
        }

        cpuRecord->addFileLevel( std::move( level ) );

        ///////////////////////// so that we only load the first level
        break;
    }


    //////////////////////const size_t lastFileLevelIndex = cpuRecord->numFileLevels() - 1;
    //////////////////////// Changed so that we only check the first level
    const size_t lastFileLevelIndex = 0;
    const SlideLevel& k_lastFileLevel = cpuRecord->fileLevel( lastFileLevelIndex );

    if ( glm::any( glm::greaterThan( k_lastFileLevel.m_dims, sk_maxSlideDimsForGPU ) ) )
    {
        const int64_t k_downsampleFactor = std::max(
                    k_lastFileLevel.m_dims.x / sk_maxSlideDimsForGPU.x,
                    k_lastFileLevel.m_dims.y / sk_maxSlideDimsForGPU.y );

        SlideLevel newLevel;
        newLevel.m_level = k_numFileLevels;
        newLevel.m_dims = k_lastFileLevel.m_dims / k_downsampleFactor;

        const glm::dvec2 k_baseDims = glm::dvec2( cpuRecord->fileLevel( 0 ).m_dims );
        newLevel.m_downsampleFactors.x = k_baseDims.x / newLevel.m_dims.x;
        newLevel.m_downsampleFactors.y = k_baseDims.y / newLevel.m_dims.y;

        newLevel.m_data = std::make_unique< uint32_t[] >(
                    static_cast<size_t>( newLevel.m_dims.x * newLevel.m_dims.y ) );

        static const glm::i64vec2 sk_maxDimsForOpenCV( std::numeric_limits<int32_t>::max() );

        if ( glm::any( glm::greaterThan( k_lastFileLevel.m_dims, sk_maxDimsForOpenCV ) ) )
        {
            std::cerr << "Slide too large to downsample" << std::endl;
            return nullptr;
        }

        downsample( k_lastFileLevel.m_data.get(),
                    glm::i32vec2( k_lastFileLevel.m_dims ),
                    newLevel.m_data.get(),
                    glm::i32vec2( newLevel.m_dims ) );

        std::cout << "\tdims[" << k_numFileLevels << "] = "
                  << glm::to_string( newLevel.m_dims ) << std::endl;

        std::cout << "\tdownsampleFactor[" << k_numFileLevels << "] = "
                  << glm::to_string( newLevel.m_downsampleFactors ) << std::endl;

        cpuRecord->addCreatedLevel( std::move( newLevel ) );
    }


    // Create thumbnail image if none was created
    auto thumbImage = cpuRecord->header().associatedImages().thumbImage();
    auto thumbImageData = thumbImage.first.lock();

    if ( ! thumbImageData )
    {
        const glm::i64vec2 dims( 64, 64 );

        auto data = std::make_shared< std::vector<uint32_t> >( static_cast<size_t>( dims.x * dims.y ) );

        downsample( k_lastFileLevel.m_data.get(),
                    glm::i32vec2( k_lastFileLevel.m_dims ),
                    data->data(), dims );

        cpuRecord->header().associatedImages().setThumbImage( data, dims );
    }


    openslide_close( reader );

#endif

  std::unique_ptr<SlideCpuRecord> cpuRecord = nullptr;
  return cpuRecord;
}

} // namespace slideio
