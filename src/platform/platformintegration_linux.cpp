#include "platform/platformintegration.h"

#include "backend/systemdbackend.h"
#include "qml/traycontroller.h"
#include "platform/linuxautostartintegration.h"

namespace Zapret::Platform {

ZapretBackend *createBackend()
{
    return new SystemdBackend;
}

TrayIntegration *createTrayIntegration()
{
    return new TrayController;
}

AutostartIntegration *createAutostartIntegration()
{
    return new LinuxAutostartIntegration;
}

} // namespace Zapret::Platform
