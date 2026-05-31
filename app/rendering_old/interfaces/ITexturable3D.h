#ifndef I_TEXTURABLE_3D_H
#define I_TEXTURABLE_3D_H

#include "logic_old/records/ImageColorMapRecord.h"
#include "logic_old/records/ImageRecord.h"
#include "logic_old/records/LabelTableRecord.h"
#include "logic_old/records/ParcellationRecord.h"

#include <memory>

/**
 * @brief Something that can be textured by a 3D image and parcellation
 */
class ITexturable3d
{
public:
  virtual ~ITexturable3d() = default;

  /// Set the 3D image texture record
  virtual void setImage3dRecord(std::weak_ptr<ImageRecord>) = 0;

  /// Set the 3D parcellation image texture record
  virtual void setParcellationRecord(std::weak_ptr<ParcellationRecord>) = 0;

  /// Set the image's color map record
  virtual void setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord>) = 0;

  /// Set the parcellation labels record
  virtual void setLabelTableRecord(std::weak_ptr<LabelTableRecord>) = 0;
};

#endif // I_TEXTURABLE_3D_H
