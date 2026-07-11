#include "ui/Clipboard.h"

#include <imgui/imgui.h>

namespace entropy::ui
{

bool setClipboardPayload(const entropy::ClipboardPayload& payload)
{
  ImGui::SetClipboardText(payload.plainText.c_str());
  return true;
}

} // namespace entropy::ui
