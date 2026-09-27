#include "ui/Clipboard.h"

#include <imgui/imgui.h>

namespace ui
{

bool setClipboardPayload(const ClipboardPayload& payload)
{
  ImGui::SetClipboardText(payload.plainText.c_str());
  return true;
}

} // namespace ui
