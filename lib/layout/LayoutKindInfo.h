#pragma once

#include "viewer/LayoutTypes.h"
#include "viewer/ViewTypes.h"

#include <string_view>

namespace layout
{

/**
 * @brief Whether a layout kind is one of the built-in lightbox layouts.
 * @param kind Layout kind to classify.
 * @return True for axial, coronal, and sagittal lightbox layout kinds.
 */
bool isLightboxLayoutKind(LayoutKind kind);

/**
 * @brief Whether a managed layout kind depends on the loaded image set.
 * @param kind Layout kind to classify.
 * @return True when layouts of this kind should be regenerated as images change.
 */
bool isImageDependentManagedLayoutKind(LayoutKind kind);

/**
 * @brief Whether a managed layout kind belongs before image-dependent layouts.
 * @param kind Layout kind to classify.
 * @return True for fixed built-in layouts that do not depend on image count.
 */
bool isFixedManagedLayoutKind(LayoutKind kind);

/**
 * @brief Whether a layout kind can be losslessly serialized as a row-major regular grid.
 * @param kind Layout kind to classify.
 * @return True when a grid recipe preserves the semantic view order of the layout.
 */
bool canUseRegularGridRecipe(LayoutKind kind);

/**
 * @brief Managed lightbox layout kind for a slice view type.
 * @param viewType View type used by the lightbox.
 * @return Axial, coronal, sagittal, or custom layout kind.
 */
LayoutKind lightboxLayoutKindForViewType(ViewType viewType);

/**
 * @brief User-facing display name for a layout kind.
 * @param kind Layout kind to name.
 * @param isLightbox Whether a custom layout should be described as a lightbox.
 * @return Stable display label for UI lists and menus.
 */
std::string_view layoutDisplayName(LayoutKind kind, bool isLightbox);

} // namespace layout
