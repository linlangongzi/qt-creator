/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "remotelinuxplugin.h"

#include "embeddedlinuxqtversionfactory.h"
#include "genericlinuxdeviceconfigurationfactory.h"
#include "remotelinuxqmltoolingsupport.h"
#include "remotelinuxcustomrunconfiguration.h"
#include "remotelinuxdebugsupport.h"
#include "remotelinuxdeployconfiguration.h"
#include "remotelinuxrunconfiguration.h"
#include "remotelinuxrunconfigurationfactory.h"

#include "genericdirectuploadstep.h"
#include "remotelinuxcheckforfreediskspacestep.h"
#include "remotelinuxdeployconfiguration.h"
#include "remotelinuxcustomcommanddeploymentstep.h"
#include "tarpackagecreationstep.h"
#include "uploadandinstalltarpackagestep.h"

#include <QtPlugin>

namespace RemoteLinux {
namespace Internal {

template <class Step>
class GenericLinuxDeployStepFactory : public ProjectExplorer::BuildStepFactory
{
public:
    GenericLinuxDeployStepFactory()
    {
        registerStep<Step>(Step::stepId());
        setDisplayName(Step::displayName());
        setSupportedConfiguration(RemoteLinuxDeployConfiguration::genericDeployConfigurationId());
        setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_DEPLOY);
    }
};

RemoteLinuxPlugin::RemoteLinuxPlugin()
{
    setObjectName(QLatin1String("RemoteLinuxPlugin"));
}

bool RemoteLinuxPlugin::initialize(const QStringList &arguments,
    QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    using namespace ProjectExplorer;
    using namespace ProjectExplorer::Constants;

    auto constraint = [](RunConfiguration *runConfig) {
        const Core::Id id = runConfig->id();
        return id == RemoteLinuxCustomRunConfiguration::runConfigId()
            || id.name().startsWith(RemoteLinuxRunConfiguration::IdPrefix);
    };

    RunControl::registerWorker<SimpleTargetRunner>(NORMAL_RUN_MODE, constraint);
    RunControl::registerWorker<LinuxDeviceDebugSupport>(DEBUG_RUN_MODE, constraint);
    RunControl::registerWorker<RemoteLinuxQmlProfilerSupport>(QML_PROFILER_RUN_MODE, constraint);
    RunControl::registerWorker<RemoteLinuxQmlPreviewSupport>(QML_PREVIEW_RUN_MODE, constraint);

    addAutoReleasedObject(new GenericLinuxDeviceConfigurationFactory);
    addAutoReleasedObject(new RemoteLinuxRunConfigurationFactory);
    addAutoReleasedObject(new RemoteLinuxCustomRunConfigurationFactory);
    addAutoReleasedObject(new RemoteLinuxDeployConfigurationFactory);
    addAutoReleasedObject(new GenericLinuxDeployStepFactory<TarPackageCreationStep>);
    addAutoReleasedObject(new GenericLinuxDeployStepFactory<UploadAndInstallTarPackageStep>);
    addAutoReleasedObject(new GenericLinuxDeployStepFactory<GenericDirectUploadStep>);
    addAutoReleasedObject(new GenericLinuxDeployStepFactory
                                <GenericRemoteLinuxCustomCommandDeploymentStep>);
    addAutoReleasedObject(new GenericLinuxDeployStepFactory<RemoteLinuxCheckForFreeDiskSpaceStep>);

    addAutoReleasedObject(new EmbeddedLinuxQtVersionFactory);

    return true;
}

RemoteLinuxPlugin::~RemoteLinuxPlugin()
{
}

void RemoteLinuxPlugin::extensionsInitialized()
{
}

} // namespace Internal
} // namespace RemoteLinux
