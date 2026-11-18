#pragma once

#include "backend/zapretbackend.h"

namespace Zapret::Platform {

// Creates the backend implementation selected for the current platform.
// The caller takes ownership of the returned object.
ZapretBackend *createBackend();\n\n// Creates the desktop tray adapter selected for the current platform.\n// The caller takes ownership of the returned object.\nclass TrayIntegration;\nTrayIntegration *createTrayIntegration();

} // namespace Zapret::Platform
