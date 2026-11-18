#include "platform/platformintegration.h"

#include "backend/systemdbackend.h"
#include "qml/traycontroller.h"
#include "platform/linuxautostartintegration.h"
#include "platform/linuxconfigurationoperations.h"
#include "platform/linuxhostlistoperations.h"

namespace Zapret::Platform {

ZapretBackend *createBackend()
{
    return new SystemdBackend;
}

ConfigurationOperations *createConfigurationOperations()
{
    return new LinuxConfigurationOperations;
}

HostListOperations *createHostListOperations()
{
    return new LinuxHostListOperations;
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
