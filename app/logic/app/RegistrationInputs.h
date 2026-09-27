#pragma once
#include "registration/Types.h"
class AppData;
namespace registration_inputs
{
/// Freeze the chosen component/frame, voxel revision and physical geometry into
/// an exclusively owned job workspace. Same boundary used by queued UI jobs.
bool materialize(AppData& data, registration::JobSpec& job);
} // namespace registration_inputs
