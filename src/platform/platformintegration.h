#pragma once

#include "backend/zapretbackend.h"

class TrayIntegration;

namespace Zapret::Platform {

// Creates the backend implementation selected for the current platform.
// The caller takes ownership of the returned object.
ZapretBackend *createBackend();

// Creates the desktop tray adapter selected for the current platform.
// The caller takes ownership of the returned object.
TrayIntegration *createTrayIntegration();

} // namespace Zapret::Platform
