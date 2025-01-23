#include "common/Types.h"

#include <IconFontCppHeaders/IconsForkAwesome.h>

#include <unordered_map>

bool isComponentFloatingPoint(const ComponentType& compType)
{
  switch (compType)
  {
  case ComponentType::Int8:
  case ComponentType::UInt8:
  case ComponentType::Int16:
  case ComponentType::UInt16:
  case ComponentType::Int32:
  case ComponentType::UInt32:
  case ComponentType::ULong:
  case ComponentType::Long:
  case ComponentType::ULongLong:
  case ComponentType::LongLong:
  {
    return false;
  }

  case ComponentType::Float32:
  case ComponentType::Float64:
  case ComponentType::LongDouble:
  {
    return true;
  }

  case ComponentType::Undefined:
  {
    return false;
  }
  }

  return false;
}

bool isComponentUnsignedInt(const ComponentType& compType)
{
  switch (compType)
  {
  case ComponentType::Int8:
  case ComponentType::Int16:
  case ComponentType::Int32:
  case ComponentType::Long:
  case ComponentType::LongLong:
  case ComponentType::Float32:
  case ComponentType::Float64:
  case ComponentType::LongDouble:
  {
    return false;
  }

  case ComponentType::UInt8:
  case ComponentType::UInt16:
  case ComponentType::UInt32:
  case ComponentType::ULong:
  case ComponentType::ULongLong:
  {
    return true;
  }

  case ComponentType::Undefined:
  {
    return false;
  }
  }

  return false;
}

bool isValidSegmentationComponentType(const ComponentType& compType)
{
  switch (compType)
  {
  case ComponentType::UInt8:
  case ComponentType::UInt16:
  case ComponentType::UInt32:
  {
    return true;
  }

  default:
  {
    return false;
  }
  }
}

std::string typeString(const InterpolationMode& mode)
{
  static const std::unordered_map<InterpolationMode, std::string> s_modeToString{
    {InterpolationMode::NearestNeighbor, "Nearest"},
    {InterpolationMode::Trilinear, "Linear"},
    {InterpolationMode::Tricubic, "Cubic"}
  };

  return s_modeToString.at(mode);
}

std::string componentTypeString(const ComponentType& compType)
{
  static const std::unordered_map<ComponentType, std::string> s_compTypeToStringMap{
    {ComponentType::Int8, "Signed 8-bit char (int8)"},
    {ComponentType::UInt8, "Unsigned 8-bit char (uint8)"},

    {ComponentType::Int16, "Signed 16-bit short int (int16)"},
    {ComponentType::UInt16, "Unsigned 16-bit short int (uint16)"},

    {ComponentType::Int32, "Signed 32-bit int (int32)"},
    {ComponentType::UInt32, "Unsigned 32-bit int (uint32)"},

    {ComponentType::Long, "Signed long int"},
    {ComponentType::ULong, "Unsigned long int"},

    {ComponentType::LongLong, "Signed long long int"},
    {ComponentType::ULongLong, "Unsigned long long int"},

    {ComponentType::Float32, "Single 32-bit float (float)"},
    {ComponentType::Float64, "Double 64-bit float (double)"},

    {ComponentType::LongDouble, "Long double"},

    {ComponentType::Undefined, "Undefined"}
  };

  return s_compTypeToStringMap.at(compType);
}

std::string typeString(const MouseMode& mouseMode)
{
  static const std::unordered_map<MouseMode, std::string> s_typeToStringMap{
    {MouseMode::Pointer, "Pointer (V)\nMove the crosshairs"},
    {MouseMode::WindowLevel,
     "Window/level and opacity (L)\nLeft button: window/level\nRight button: opacity"},
    {MouseMode::CameraTranslate,
     "Pan/dolly view (X)\nLeft button: pan in plane\nRight button: dolly in/out of plane (3D views "
     "only)"},
    {MouseMode::CameraRotate,
     "Rotate view\nLeft button: rotate in plane\nRight button: rotate out of plane\n(Use "
     "Shift/Ctrl to lock rotation about view X/Y)"},
    {MouseMode::CameraZoom,
     "Zoom view (Z)\nLeft button: zoom to crosshairs\nRight button: zoom to cursor"},
    {MouseMode::Segment,
     "Segment (B)\nLeft button: paint foreground label\nRight button: paint background label"},
    {MouseMode::Annotate, "Annotate"},
    {MouseMode::ImageTranslate,
     "Translate image (T)\nLeft button: translate in plane\nRight button: translate out of plane"},
    {MouseMode::ImageRotate,
     "Rotate image (R)\nLeft button: rotate in plane\nRight button: rotate out of plane"},
    {MouseMode::ImageScale, "Scale image (Y)"}
  };

  return s_typeToStringMap.at(mouseMode);
}

bool isIntegerType(const ComponentType& type)
{
  return (!isFloatingType(type));
}

bool isFloatingType(const ComponentType& type)
{
  if (ComponentType::Float32 == type || ComponentType::Float64 == type || ComponentType::LongDouble == type)
  {
    return true;
  }

  return false;
}

bool isSignedIntegerType(const ComponentType& type)
{
  if (ComponentType::Int8 == type || ComponentType::Int16 == type || ComponentType::Int32 == type || ComponentType::Long == type || ComponentType::LongLong == type)
  {
    return true;
  }

  return false;
}

bool isUnsignedIntegerType(const ComponentType& type)
{
  if (ComponentType::UInt8 == type || ComponentType::UInt16 == type || ComponentType::UInt32 == type || ComponentType::ULong == type || ComponentType::ULongLong == type)
  {
    return true;
  }

  return false;
}

const char* toolbarButtonIcon(const MouseMode& mouseMode)
{
  static const std::unordered_map<MouseMode, const char*> s_typeToIconMap{
    {MouseMode::Pointer, ICON_FK_MOUSE_POINTER},
    {MouseMode::Segment, ICON_FK_PAINT_BRUSH},
    {MouseMode::Annotate, ICON_FK_PENCIL},
    {MouseMode::WindowLevel, ICON_FK_ADJUST},
    {MouseMode::CameraTranslate, ICON_FK_HAND_PAPER_O},
    {MouseMode::CameraRotate, ICON_FK_FUTBOL_O},
    {MouseMode::CameraZoom, ICON_FK_SEARCH},
    {MouseMode::ImageTranslate, ICON_FK_ARROWS},
    {MouseMode::ImageRotate, ICON_FK_UNDO},
    {MouseMode::ImageScale, ICON_FK_EXPAND}
  };

  return s_typeToIconMap.at(mouseMode);
}
