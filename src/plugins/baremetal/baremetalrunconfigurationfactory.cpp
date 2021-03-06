/****************************************************************************
**
** Copyright (C) 2016 Tim Sander <tim@krieglstein.org>
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

#include "baremetalrunconfigurationfactory.h"
#include "baremetalconstants.h"
#include "baremetalcustomrunconfiguration.h"
#include "baremetalrunconfiguration.h"

#include <projectexplorer/buildtargetinfo.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/project.h>
#include <projectexplorer/target.h>
#include <utils/qtcassert.h>

using namespace ProjectExplorer;

namespace BareMetal {
namespace Internal {

// BareMetalRunConfigurationFactory

BareMetalRunConfigurationFactory::BareMetalRunConfigurationFactory(QObject *parent) :
    IRunConfigurationFactory(parent)
{
    setObjectName("BareMetalRunConfigurationFactory");
    registerRunConfiguration<BareMetalRunConfiguration>(BareMetalRunConfiguration::IdPrefix);
    setSupportedTargetDeviceTypes({BareMetal::Constants::BareMetalOsType});
}

QList<QString> BareMetalRunConfigurationFactory::availableBuildTargets(Target *parent, CreationMode) const
{
    return Utils::transform(parent->applicationTargets().list, [](const BuildTargetInfo &bti) {
        return QString(bti.projectFilePath.toString() + '/' + bti.targetName);
    });
}

QString BareMetalRunConfigurationFactory::displayNameForBuildTarget(const QString &buildTarget) const
{
    return tr("%1 (on GDB server or hardware debugger)").arg(QFileInfo(buildTarget).fileName());
}


// BareMetalCustomRunConfigurationFactory

BareMetalCustomRunConfigurationFactory::BareMetalCustomRunConfigurationFactory(QObject *parent) :
    IRunConfigurationFactory(parent)
{
    setObjectName("BareMetalCustomRunConfigurationFactory");
    registerRunConfiguration<BareMetalCustomRunConfiguration>
            (BareMetalCustomRunConfiguration::runConfigId());
    setSupportedTargetDeviceTypes({BareMetal::Constants::BareMetalOsType});
}

QList<QString> BareMetalCustomRunConfigurationFactory::availableBuildTargets(Target *, CreationMode) const
{
    return {QString()};
}

QString BareMetalCustomRunConfigurationFactory::displayNameForBuildTarget(const QString &) const
{
    return BareMetalCustomRunConfiguration::runConfigDefaultDisplayName();
}

} // namespace Internal
} // namespace BareMetal
