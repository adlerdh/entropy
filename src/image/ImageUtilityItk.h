#pragma once

#include "common/Types.h"

#include <itkCommonEnums.h>
#include <itkImageIOBase.h>

#include <string>
#include <utility>

PixelType fromItkPixelType(const itk::IOPixelEnum& pixelType);

ComponentType fromItkComponentType(const itk::IOComponentEnum& componentType);

itk::IOComponentEnum toItkComponentType(const ComponentType& componentType);

std::pair<itk::CommonEnums::IOComponent, std::string> sniffComponentType(const char* fileName);

typename itk::ImageIOBase::Pointer createStandardImageIo(const char* fileName);
