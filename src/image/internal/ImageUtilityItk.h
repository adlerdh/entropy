#pragma once

#include "common/Types.h"
#include "image/ImageIoInfo.h"

#include <itkCommonEnums.h>
#include <itkImageBase.h>
#include <itkImageIOBase.h>

#include <string>
#include <utility>

PixelType fromItkPixelType(const itk::IOPixelEnum& pixelType);

ComponentType fromItkComponentType(const itk::IOComponentEnum& componentType);

itk::IOComponentEnum toItkComponentType(const ComponentType& componentType);

std::pair<itk::CommonEnums::IOComponent, std::string> sniffComponentType(const char* fileName);

typename itk::ImageIOBase::Pointer createStandardImageIo(const char* fileName);

bool setImageIoInfoFromItk(ImageIoInfo& info, const itk::ImageIOBase::Pointer imageIo);

bool setSizeInfoFromItkImageBase(
  SizeInfo& info,
  const typename itk::ImageBase<3>::Pointer imageBase,
  std::size_t componentSizeInBytes);

bool setSpaceInfoFromItkImageBase(SpaceInfo& info, const typename itk::ImageBase<3>::Pointer imageBase);
