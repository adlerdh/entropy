#ifndef LABEL_TABLE_RECORD_H
#define LABEL_TABLE_RECORD_H

#include "common/ParcellationLabelTable.h"
#include "logic_old/RenderableRecord.h"
#include "rendering/utility/gl/GLBufferTexture.h"

/**
 * Record for a table of parcellation labels. The GPU representation is a buffer texture
 * of the label colors ordered by label value.
 */
using LabelTableRecord = RenderableRecord<ParcellationLabelTable, GLBufferTexture>;

#endif // LABEL_TABLE_RECORD_H
