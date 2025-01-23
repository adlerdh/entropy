#ifndef VIEW_HIT_H
#define VIEW_HIT_H

#include "windowing/View.h"

#include <glm/glm.hpp>
#include <uuid.h>

class AppData;

/**
 * @brief When a view is hit by a mouse/pointer click, this structure is used to
 * return data about the view that was hit, including its ID, a reference to the view,
 * and the hit position in Clip space of the view.
 */
struct ViewHit
{
  View* view = nullptr; //!< A non-owning pointer to the view that was hit
  uuids::uuid viewUid;  //!< UID of the view

  glm::vec2 windowClipPos{0.0f};
  glm::vec2 viewClipPos{0.0f};
  glm::vec4 worldPos{0.0f};
  glm::vec4 worldPos_offsetApplied{0.0f};
  glm::vec3 worldFrontAxis{0.0f, 0.0f, 1.0f};
};

/// @todo Interface instead of appData
std::optional<ViewHit> getViewHit(
  AppData& appData,
  const glm::vec2& windowPos,
  const std::optional<uuids::uuid>& viewUidForOverride = std::nullopt
);

#endif // VIEW_HIT_H
