#include "ui/Clipboard.h"

#include "common/ClipboardFormats.h"

#include <windows.h>

#include <cstring>
#include <string>
#include <string_view>

namespace
{

std::wstring utf8ToWide(const std::string& text)
{
  if (text.empty()) {
    return {};
  }

  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) {
    return {};
  }

  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
  return wide;
}

HGLOBAL copyBytesToGlobalMemory(std::string_view bytes)
{
  HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes.size() + 1u);
  if (!handle) {
    return nullptr;
  }

  void* data = GlobalLock(handle);
  if (!data) {
    GlobalFree(handle);
    return nullptr;
  }

  std::memcpy(data, bytes.data(), bytes.size());
  static_cast<char*>(data)[bytes.size()] = '\0';
  GlobalUnlock(handle);
  return handle;
}

HGLOBAL copyWideToGlobalMemory(const std::wstring& text)
{
  HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1u) * sizeof(wchar_t));
  if (!handle) {
    return nullptr;
  }

  void* data = GlobalLock(handle);
  if (!data) {
    GlobalFree(handle);
    return nullptr;
  }

  std::memcpy(data, text.c_str(), (text.size() + 1u) * sizeof(wchar_t));
  GlobalUnlock(handle);
  return handle;
}

} // namespace

namespace entropy::ui
{

bool setClipboardPayload(const entropy::ClipboardPayload& payload)
{
  if (!OpenClipboard(nullptr)) {
    return false;
  }

  EmptyClipboard();
  bool wroteAny = false;

  const std::wstring plainText = utf8ToWide(payload.plainText);
  if (HGLOBAL textHandle = copyWideToGlobalMemory(plainText)) {
    if (SetClipboardData(CF_UNICODETEXT, textHandle)) {
      wroteAny = true;
    }
    else {
      GlobalFree(textHandle);
    }
  }

  if (payload.html && !payload.html->empty()) {
    const UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
    const std::string htmlPayload = entropy::clipboard::windowsHtmlClipboardPayload(*payload.html);
    if (HGLOBAL htmlHandle = copyBytesToGlobalMemory(htmlPayload)) {
      if (SetClipboardData(htmlFormat, htmlHandle)) {
        wroteAny = true;
      }
      else {
        GlobalFree(htmlHandle);
      }
    }
  }

  if (payload.rtf && !payload.rtf->empty()) {
    const UINT rtfFormat = RegisterClipboardFormatW(L"Rich Text Format");
    if (HGLOBAL rtfHandle = copyBytesToGlobalMemory(*payload.rtf)) {
      if (SetClipboardData(rtfFormat, rtfHandle)) {
        wroteAny = true;
      }
      else {
        GlobalFree(rtfHandle);
      }
    }
  }

  CloseClipboard();
  return wroteAny;
}

} // namespace entropy::ui
