#include "ui/Clipboard.h"

#import <AppKit/NSPasteboard.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>

namespace {

NSString* nsString(const std::string& text) {
  return [NSString stringWithUTF8String:text.c_str()];
}

}  // namespace

namespace entropy::ui {

bool setClipboardPayload(const entropy::ClipboardPayload& payload) {
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard clearContents];
  [pasteboard declareTypes:@[ NSPasteboardTypeRTF, NSPasteboardTypeHTML, NSPasteboardTypeString ] owner:nil];

  bool wroteAny = false;
  if (payload.rtf && !payload.rtf->empty()) {
    NSData* rtfData = [NSData dataWithBytes:payload.rtf->data() length:payload.rtf->size()];
    wroteAny = [pasteboard setData:rtfData forType:NSPasteboardTypeRTF] || wroteAny;
  }
  if (payload.html && !payload.html->empty()) {
    wroteAny = [pasteboard setString:nsString(*payload.html) forType:NSPasteboardTypeHTML] || wroteAny;
  }
  if (!payload.plainText.empty()) {
    wroteAny = [pasteboard setString:nsString(payload.plainText) forType:NSPasteboardTypeString] || wroteAny;
  }

  return wroteAny;
}

}  // namespace entropy::ui
