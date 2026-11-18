#include "platform/platformintegration.h"

#include "backend/systemdbackend.h"

namespace Zapret::Platform {

ZapretBackend *createBackend()
{
    return new SystemdBackend;
}

} // namespace Zapret::Platform
