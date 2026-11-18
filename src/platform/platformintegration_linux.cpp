#include "platform/platformintegration.h"

#include "backend/systemdbackend.h"
#include "qml/traycontroller.h"

namespace Zapret::Platform {

ZapretBackend *createBackend()
{
    return new SystemdBackend;
}

TrayIntegration *createTrayIntegration()
{
    return new TrayController;
}

} // namespace Zapret::Platform
