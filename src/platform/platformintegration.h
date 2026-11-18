#pragma once

#include "backend/zapretbackend.h"

namespace Zapret::Platform {

// Creates the backend implementation selected for the current platform.
// The caller takes ownership of the returned object.
ZapretBackend *createBackend();

} // namespace Zapret::Platform
