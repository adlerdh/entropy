#pragma once

namespace ui::export_jobs
{
class Service;

/// Render the non-modal export progress, cancellation, and completion window.
void renderStatusWindow(Service& service);
} // namespace ui::export_jobs
