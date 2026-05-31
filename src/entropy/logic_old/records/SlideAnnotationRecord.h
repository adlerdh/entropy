#ifndef SLIDE_ANNOTATION_RECORD_H
#define SLIDE_ANNOTATION_RECORD_H

#include "logic_old/RenderableRecord.h"
#include "logic_old/annotation/SlideAnnotationCpuRecord.h"
#include "rendering_old/records/SlideAnnotationGpuRecord.h"

/// @note Slide annotations are closed, planar polygons defined in normalized [0, 1]^2
/// coordinates of a slide.
using SlideAnnotationRecord = RenderableRecord<SlideAnnotationCpuRecord, SlideAnnotationGpuRecord>;

#endif // SLIDE_ANNOTATION_RECORD_H
