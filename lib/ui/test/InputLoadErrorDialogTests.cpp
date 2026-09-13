#include "ui/dialogs/InputLoadErrorDialog.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("input load errors identify the failed file and cause", "[ui][dialogs]")
{
  const auto dialog = native_dialog::inputLoadErrorDialog(
    {.inputType = "segmentation",
     .path = "/data/labels.tar.gz",
     .cause = "[in function 'load'; file '/src/file.cpp' : line 20] Unsupported image format"});

  CHECK(dialog.title == "Unable to Load Segmentation");
  CHECK(dialog.message == "Entropy could not load \"labels.tar.gz\".");
  CHECK(dialog.informativeText.find("Path: /data/labels.tar.gz") != std::string::npos);
  CHECK(dialog.informativeText.find("Cause: Unsupported image format") != std::string::npos);
  CHECK(dialog.firstButton == "OK");
  CHECK(dialog.secondButton.empty());
  CHECK(dialog.severity == native_dialog::MessageDialogSeverity::Error);
}

TEST_CASE("input load errors provide a fallback cause", "[ui][dialogs]")
{
  const auto dialog = native_dialog::inputLoadErrorDialog({.inputType = "project", .path = std::nullopt, .cause = ""});
  CHECK(dialog.message == "Entropy could not load the selected project.");
  CHECK(dialog.informativeText.find("invalid, unsupported, unreadable") != std::string::npos);
}
