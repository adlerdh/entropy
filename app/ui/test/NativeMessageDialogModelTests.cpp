#include "ui/dialogs/NativeMessageDialogModel.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace dialog_model = native_dialog::model;

TEST_CASE("native message dialog model preserves button order", "[ui][dialogs]")
{
  const native_dialog::MessageDialog dialog{"Title", "Message", "Details", "Save", "Discard", "Cancel"};

  CHECK(dialog_model::buttonLabels(dialog) == std::vector<std::string>{"Save", "Discard", "Cancel"});
}

TEST_CASE("native message dialog model omits absent third button", "[ui][dialogs]")
{
  const native_dialog::MessageDialog dialog{"Title", "Message", "Details", "Remove", "Cancel", ""};

  CHECK(dialog_model::buttonLabels(dialog) == std::vector<std::string>{"Remove", "Cancel"});
}

TEST_CASE("native message dialog model combines detail text with platform spacing", "[ui][dialogs]")
{
  CHECK(dialog_model::combinedInformativeText("Message", "Details") == "Message\n\nDetails");
  CHECK(dialog_model::combinedInformativeText("", "Details") == "Details");
  CHECK(dialog_model::combinedInformativeText("Message", "") == "Message");
}

TEST_CASE("native message dialog model fallback text uses message and details", "[ui][dialogs]")
{
  const native_dialog::MessageDialog
    dialog{"Title", "The current project has unsaved changes.", "Save before quitting?", "Save", "Discard", "Cancel"};

  CHECK(
    dialog_model::fallbackMessageText(dialog) == "The current project has unsaved changes.\n\nSave before quitting?");
}
