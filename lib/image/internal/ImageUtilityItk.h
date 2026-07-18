#pragma once

#include "common/Types.h"
#include "../ImageIoInfo.h"

#include <itkCommonEnums.h>
#include <itkImageBase.h>
#include <itkImageIOBase.h>

#include <string>
#include <utility>

/**
 * @brief Convert an ITK pixel type enum to Entropy's pixel type enum.
 */
PixelType fromItkPixelType(const itk::IOPixelEnum& pixelType);

/**
 * @brief Convert an ITK component type enum to Entropy's component type enum.
 */
ComponentType fromItkComponentType(const itk::IOComponentEnum& componentType);

/**
 * @brief Convert an Entropy component type enum to ITK's component type enum.
 */
itk::IOComponentEnum toItkComponentType(const ComponentType& componentType);

/**
 * @brief Read enough image metadata to determine the on-disk component type.
 * @param[in] fileName Image file to inspect.
 * @return ITK component type and display string.
 */
std::pair<itk::CommonEnums::IOComponent, std::string> sniffComponentType(const char* fileName);

/**
 * @brief Create and initialize an ITK ImageIO object for a file.
 * @param[in] fileName Image file to inspect.
 * @return Initialized ImageIO object, or null when no suitable reader exists.
 */
typename itk::ImageIOBase::Pointer createStandardImageIo(const char* fileName);

/**
 * @brief Populate Entropy image IO metadata from an initialized ITK ImageIO object.
 */
bool setImageIoInfoFromItk(ImageIoInfo& info, const itk::ImageIOBase::Pointer& imageIo);

/**
 * @brief Populate size metadata from a 3D ITK image base.
 * @param[out] info Size metadata to fill.
 * @param[in] imageBase ITK image object.
 * @param[in] componentSizeInBytes Size of one pixel component in bytes.
 * @return True when metadata was populated.
 */
bool setSizeInfoFromItkImageBase(
  SizeInfo& info,
  const typename itk::ImageBase<3>::Pointer& imageBase,
  std::size_t componentSizeInBytes);

/**
 * @brief Populate spatial metadata from a 3D ITK image base.
 */
bool setSpaceInfoFromItkImageBase(SpaceInfo& info, const typename itk::ImageBase<3>::Pointer& imageBase);
