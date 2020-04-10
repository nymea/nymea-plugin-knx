/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
*
* GNU Lesser General Public License Usage
* This project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "plugininfo.h"
#include "integrationpluginknx.h"

#include <QObject>
#include <QDateTime>
#include <QKnxAddress>
#include <QKnx2ByteFloat>
#include <QNetworkInterface>
#include <QKnx8BitUnsignedValue>
#include <QKnxGroupAddressInfos>

IntegrationPluginKnx::IntegrationPluginKnx()
{

}

void IntegrationPluginKnx::init()
{
    m_discovery = new KnxServerDiscovery(this);

    connect(this, &IntegrationPluginKnx::configValueChanged, this, &IntegrationPluginKnx::onPluginConfigurationChanged);
}

void IntegrationPluginKnx::startMonitoringAutoThings()
{
    // Start seaching for devices which can be discovered and added automatically
}

void IntegrationPluginKnx::discoverThings(ThingDiscoveryInfo *info)
{
    if (info->thingClassId() == knxNetIpServerThingClassId) {
        if (!m_discovery->startDisovery()) {
            info->finish(Thing::ThingErrorThingInUse);
        }

        connect(m_discovery, &KnxServerDiscovery::discoveryFinished, info, [this, info](){
            qCDebug(dcKnx()) << "Discovery finished.";
            foreach (const QKnxNetIpServerInfo &serverInfo, m_discovery->discoveredServers()) {
                qCDebug(dcKnx()) << "Found server:" << QString("%1:%2").arg(serverInfo.controlEndpointAddress().toString()).arg(serverInfo.controlEndpointPort());
                KnxServerDiscovery::printServerInfo(serverInfo);
                ThingDescriptor descriptor(knxNetIpServerThingClassId, "KNX NetIp Server", QString("%1:%2").arg(serverInfo.controlEndpointAddress().toString()).arg(serverInfo.controlEndpointPort()));
                ParamList params;
                params.append(Param(knxNetIpServerThingAddressParamTypeId, serverInfo.controlEndpointAddress().toString()));
                params.append(Param(knxNetIpServerThingPortParamTypeId, serverInfo.controlEndpointPort()));
                descriptor.setParams(params);
                foreach (Thing *existingThing, myThings()) {
                    if (existingThing->paramValue(knxNetIpServerThingAddressParamTypeId).toString() == serverInfo.controlEndpointAddress().toString()) {
                        descriptor.setThingId(existingThing->id());
                        break;
                    }
                }
                info->addThingDescriptor(descriptor);
            }

            info->finish(Thing::ThingErrorNoError);
        });

    }
}

void IntegrationPluginKnx::setupThing(ThingSetupInfo *info)
{
    qCDebug(dcKnx()) << "Setup device" << info->thing()->name() << info->thing()->params();

    if (!m_pluginTimer) {
        m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(300);
        connect(m_pluginTimer, &PluginTimer::timeout, this, &IntegrationPluginKnx::onPluginTimerTimeout);
    }

    if (info->thing()->thingClassId() == knxNetIpServerThingClassId) {
        QHostAddress remoteAddress = QHostAddress(info->thing()->paramValue(knxNetIpServerThingAddressParamTypeId).toString());
        KnxTunnel *tunnel = new KnxTunnel(remoteAddress, this);
        connect(tunnel, &KnxTunnel::connectedChanged, this, &IntegrationPluginKnx::onTunnelConnectedChanged);
        connect(tunnel, &KnxTunnel::frameReceived, this, &IntegrationPluginKnx::onTunnelFrameReceived);
        m_tunnels.insert(tunnel, info->thing());
    }

    if (info->thing()->thingClassId() == knxTriggerThingClassId) {
        QHostAddress tunnelAddress =  QHostAddress(info->thing()->paramValue(knxTriggerThingAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for address" << tunnelAddress.toString();
            info->finish(Thing::ThingErrorHardwareFailure);
            return;
        }

        info->thing()->setParentId(m_tunnels.value(tunnel)->id());
    }

    if (info->thing()->thingClassId() == knxShutterThingClassId) {
        QHostAddress tunnelAddress =  QHostAddress(info->thing()->paramValue(knxShutterThingAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for address" << tunnelAddress.toString();
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        info->thing()->setParentId(m_tunnels.value(tunnel)->id());
    }

    if (info->thing()->thingClassId() == knxLightThingClassId) {
        QHostAddress tunnelAddress =  QHostAddress(info->thing()->paramValue(knxLightThingAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for address" << tunnelAddress.toString();
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        info->thing()->setParentId(m_tunnels.value(tunnel)->id());
    }

    if (info->thing()->thingClassId() == knxDimmableLightThingClassId) {
        QHostAddress tunnelAddress =  QHostAddress(info->thing()->paramValue(knxDimmableLightThingAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for address" << tunnelAddress.toString();
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        info->thing()->setParentId(m_tunnels.value(tunnel)->id());
    }

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginKnx::postSetupThing(Thing *thing)
{
    qCDebug(dcKnx()) << "Post setup device" << thing->name() << thing->params();
    if (thing->thingClassId() == knxNetIpServerThingClassId) {
        KnxTunnel *tunnel = m_tunnels.key(thing);
        tunnel->connectTunnel();
        if (configValue(knxPluginGenericDevicesEnabledParamTypeId).toBool()) {
            createGenericDevices(thing);
        } else {
            destroyGenericDevices(thing);
        }
    }

    if (thing->thingClassId() == knxGenericSwitchThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) return;
        thing->setStateValue(knxGenericSwitchConnectedStateTypeId, tunnel->connected());

        // Read initial state
        if (tunnel->connected()) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericSwitchThingKnxAddressParamTypeId).toString());
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    if (thing->thingClassId() == knxGenericUpDownThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (tunnel) thing->setStateValue(knxGenericUpDownConnectedStateTypeId, tunnel->connected());
    }

    if (thing->thingClassId() == knxGenericScalingThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (tunnel) thing->setStateValue(knxGenericScalingConnectedStateTypeId, tunnel->connected());
    }

    if (thing->thingClassId() == knxGenericTemperatureSensorThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) return;
        thing->setStateValue(knxGenericTemperatureSensorConnectedStateTypeId, tunnel->connected());

        // Read initial state
        if (tunnel->connected()) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericTemperatureSensorThingKnxAddressParamTypeId).toString());
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    if (thing->thingClassId() == knxGenericLightSensorThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) return;
        thing->setStateValue(knxGenericLightSensorConnectedStateTypeId, tunnel->connected());

        // Read initial state
        if (tunnel->connected()) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericLightSensorThingKnxAddressParamTypeId).toString());
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    if (thing->thingClassId() == knxGenericWindSpeedSensorThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) return;
        thing->setStateValue(knxGenericWindSpeedSensorConnectedStateTypeId, tunnel->connected());

        // Read initial state
        if (tunnel->connected()) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericWindSpeedSensorThingKnxAddressParamTypeId).toString());
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    if (thing->thingClassId() == knxTriggerThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (tunnel) thing->setStateValue(knxTriggerConnectedStateTypeId, tunnel->connected());
    }

    if (thing->thingClassId() == knxShutterThingClassId) {
        QHostAddress tunnelAddress =  QHostAddress(thing->paramValue(knxShutterThingAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) return;
        thing->setStateValue(knxShutterConnectedStateTypeId, tunnel->connected());
    }

    if (thing->thingClassId() == knxLightThingClassId) {
        QHostAddress tunnelAddress =  QHostAddress(thing->paramValue(knxLightThingAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) return;
        thing->setStateValue(knxLightConnectedStateTypeId, tunnel->connected());

        // Read initial state
        if (tunnel->connected()) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxLightThingKnxAddressParamTypeId).toString());
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    if (thing->thingClassId() == knxDimmableLightThingClassId) {
        QHostAddress tunnelAddress =  QHostAddress(thing->paramValue(knxDimmableLightThingAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (tunnel) thing->setStateValue(knxDimmableLightConnectedStateTypeId, tunnel->connected());

        // Read initial state
        if (tunnel->connected()) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxDimmableLightThingKnxSwitchAddressParamTypeId).toString());
            tunnel->readKnxGroupValue(knxAddress);
            // TODO: read brightness
        }
    }
}

void IntegrationPluginKnx::thingRemoved(Thing *thing)
{
    qCDebug(dcKnx()) << "Remove device" << thing->name() << thing->params();
    if (thing->thingClassId() == knxNetIpServerThingClassId) {
        KnxTunnel *tunnel = m_tunnels.key(thing);
        m_tunnels.remove(tunnel);
        tunnel->disconnectTunnel();
        tunnel->deleteLater();
    }

    if (myThings().isEmpty() && m_pluginTimer) {
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}

void IntegrationPluginKnx::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    Action action = info->action();
    qCDebug(dcKnx()) << "Executing action for device" << thing->name() << action.actionTypeId().toString() << action.params();

    // Knx NetIp server
    if (thing->thingClassId() == knxNetIpServerThingClassId) {
        if (action.actionTypeId() == knxNetIpServerAutoCreateDevicesActionTypeId) {
            autoCreateKnownDevices(thing);
            info->finish(Thing::ThingErrorNoError);
            return;
        }
    }

    // Generic switch
    if (thing->thingClassId() == knxGenericSwitchThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericSwitchThingKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxGenericSwitchPowerActionTypeId) {
            tunnel->sendKnxDpdSwitchFrame(knxAddress, action.param(knxGenericSwitchPowerActionPowerParamTypeId).value().toBool());
        }

        if (action.actionTypeId() == knxGenericSwitchReadActionTypeId) {
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    // Generic UpDown
    if (thing->thingClassId() == knxGenericUpDownThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }
        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericUpDownThingKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxGenericUpDownOpenActionTypeId) {
            tunnel->sendKnxDpdUpDownFrame(knxAddress, true);
        }

        if (action.actionTypeId() == knxGenericUpDownCloseActionTypeId) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericUpDownThingKnxAddressParamTypeId).toString());
            tunnel->sendKnxDpdUpDownFrame(knxAddress, false);
        }
    }

    // Generic Scaling
    if (thing->thingClassId() == knxGenericScalingThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericScalingThingKnxAddressParamTypeId).toString());
        int scaling = action.param(knxGenericScalingScaleActionScaleParamTypeId).value().toInt();
        tunnel->sendKnxDpdScalingFrame(knxAddress, scaling);
    }

    // Generic Temperature
    if (thing->thingClassId() == knxGenericTemperatureSensorThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericTemperatureSensorThingKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxGenericTemperatureSensorReadActionTypeId) {
            qCDebug(dcKnx()) << "Send temperature read request" << knxAddress.toString();
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    // Generic Light sensor
    if (thing->thingClassId() == knxGenericLightSensorThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericLightSensorThingKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxGenericLightSensorReadActionTypeId) {
            qCDebug(dcKnx()) << "Send temperature read request" << knxAddress.toString();
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    // Generic wind speed sensor
    if (thing->thingClassId() == knxGenericWindSpeedSensorThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericWindSpeedSensorThingKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxGenericWindSpeedSensorReadActionTypeId) {
            qCDebug(dcKnx()) << "Send wind speed read request" << knxAddress.toString();
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    // Shutter
    if (thing->thingClassId() == knxShutterThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        QKnxAddress knxAddressStep = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxShutterThingKnxAddressStepParamTypeId).toString());
        QKnxAddress knxAddressUpDown = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxShutterThingKnxAddressUpDownParamTypeId).toString());

        if (action.actionTypeId() == knxShutterOpenActionTypeId) {
            // Note: first send step, then delayed the up/down command
            tunnel->sendKnxDpdStepFrame(knxAddressStep, false);
            tunnel->sendKnxDpdUpDownFrame(knxAddressUpDown, true);
            info->finish(Thing::ThingErrorNoError);
            return;
        }

        if (action.actionTypeId() == knxShutterCloseActionTypeId) {
            // Note: first send step, then delayed the up/down command
            tunnel->sendKnxDpdStepFrame(knxAddressStep, true);
            tunnel->sendKnxDpdUpDownFrame(knxAddressUpDown, false);
            info->finish(Thing::ThingErrorNoError);
            return;
        }

        if (action.actionTypeId() == knxShutterStopActionTypeId) {
            tunnel->sendKnxDpdStepFrame(knxAddressStep, true);
            info->finish(Thing::ThingErrorNoError);
            return;
        }
    }

    // Light
    if (thing->thingClassId() == knxLightThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxLightThingKnxAddressParamTypeId).toString());
        if (action.actionTypeId() == knxLightPowerActionTypeId) {
            tunnel->sendKnxDpdSwitchFrame(knxAddress, action.param(knxLightPowerActionPowerParamTypeId).value().toBool());
        }

        if (action.actionTypeId() == knxLightReadActionTypeId) {
            tunnel->readKnxGroupValue(knxAddress);
        }
    }

    //  Dimmable Light
    if (thing->thingClassId() == knxDimmableLightThingClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(thing);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        QKnxAddress knxSwitchAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxDimmableLightThingKnxSwitchAddressParamTypeId).toString());
        QKnxAddress knxScalingAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxDimmableLightThingKnxScalingAddressParamTypeId).toString());

        if (action.actionTypeId() == knxDimmableLightPowerActionTypeId) {
            tunnel->sendKnxDpdSwitchFrame(knxSwitchAddress, action.param(knxDimmableLightPowerActionPowerParamTypeId).value().toBool());
        }

        if (action.actionTypeId() == knxDimmableLightBrightnessActionTypeId) {
            int percentage = action.param(knxDimmableLightBrightnessActionBrightnessParamTypeId).value().toInt();
            int scaled = qRound(percentage * 255.0 / 100.0);
            qCDebug(dcKnx()) << "Percentage" << percentage << "-->" << scaled;
            thing->setStateValue(knxDimmableLightBrightnessStateTypeId, percentage);
            tunnel->sendKnxDpdScalingFrame(knxScalingAddress, scaled);
        }

        if (action.actionTypeId() == knxDimmableLightReadActionTypeId) {
            tunnel->readKnxGroupValue(knxSwitchAddress);
            tunnel->readKnxGroupValue(knxScalingAddress);
        }
    }

    info->finish(Thing::ThingErrorNoError);
}

KnxTunnel *IntegrationPluginKnx::getTunnelForDevice(Thing *thing)
{
    Thing *parent = nullptr;
    foreach (Thing *t, myThings()) {
        if (t->id() == thing->parentId()) {
            parent = t;
        }
    }

    if (!parent) {
        qCWarning(dcKnx()) << "Could not find parent device for" << thing->name() << thing->id().toString();
        return nullptr;
    }

    return m_tunnels.key(parent);
}

void IntegrationPluginKnx::autoCreateKnownDevices(Thing *parentThing)
{
    // Load project file and compair name/DataPoint values
    if (parentThing->thingClassId() != knxNetIpServerThingClassId) {
        qCWarning(dcKnx()) << "Cannot create generic devices for" << parentThing->name() << ". This is not a NetIpServer device";
        return;
    }

    // Load project file
    QString projectFilePath = parentThing->paramValue(knxNetIpServerThingKnxProjectFileParamTypeId).toString();
    QKnxGroupAddressInfos groupInformation(projectFilePath);
    qCDebug(dcKnx()) << "Opening project file" << groupInformation.projectFile();
    if (!groupInformation.parse()) {
        qCWarning(dcKnx()) << "Could not parse project file" << groupInformation.projectFile() << groupInformation.errorString();
        return;
    }

    QHash<QString, QList<QKnxDatapointType::Type>> nameDatapointHash;

    qCDebug(dcKnx()) << "Found" << groupInformation.projectIds().count() << "project ids";
    foreach (const QString &projectId, groupInformation.projectIds()) {
        qCDebug(dcKnx()) << "Project id" << projectId << groupInformation.projectName(projectId);
        foreach (const QString &installation, groupInformation.installations(projectId)) {
            QVector<QKnxGroupAddressInfo> groupAddresses = groupInformation.addressInfos(projectId, installation);
            qCDebug(dcKnx()) << "Installation contains" << groupAddresses.count() << "group addresses.";
            foreach (const QKnxGroupAddressInfo &addressInfo, groupAddresses) {
                if (nameDatapointHash.keys().contains(addressInfo.name())) {
                    QList<QKnxDatapointType::Type> dataPointList = nameDatapointHash.value(addressInfo.name());
                    if (!dataPointList.contains(addressInfo.datapointType())) {
                        dataPointList.append(addressInfo.datapointType());
                        nameDatapointHash[addressInfo.name()] = dataPointList;
                    }
                } else {
                    nameDatapointHash.insert(addressInfo.name(), QList<QKnxDatapointType::Type>() << addressInfo.datapointType());
                }
            }
        }
    }

    qCDebug(dcKnx()) << "Show name datapoint list";
    foreach (const QString &name, nameDatapointHash.keys()) {
        qCDebug(dcKnx()) << "-->" << name;
        QList<QKnxDatapointType::Type> dataPointList = nameDatapointHash.value(name);

        foreach (const QKnxDatapointType::Type &type, dataPointList) {
            qCDebug(dcKnx()) << "    -" << type;
        }

        // TODO: find known data point combinations

    }
}

void IntegrationPluginKnx::createGenericDevices(Thing *parentThing)
{
    // Make sure the feature is enabled
    if (!configValue(knxPluginGenericDevicesEnabledParamTypeId).toBool()) {
        qCDebug(dcKnx()) << "Do not scan the project file for autocreation of devices";
        return;
    }

    if (parentThing->thingClassId() != knxNetIpServerThingClassId) {
        qCWarning(dcKnx()) << "Cannot create generic devices for" << parentThing->name() << ". This is not a NetIpServer device";
        return;
    }

    // Load project file
    QString projectFilePath = parentThing->paramValue(knxNetIpServerThingKnxProjectFileParamTypeId).toString();
    QKnxGroupAddressInfos groupInformation(projectFilePath);
    qCDebug(dcKnx()) << "Opening project file" << groupInformation.projectFile();
    if (!groupInformation.parse()) {
        qCWarning(dcKnx()) << "Could not parse project file" << groupInformation.projectFile() << groupInformation.errorString();
        return;
    }

    // Create descriptor lists for generic devices
    QList<ThingDescriptor> knxGenericSwitchThingDescriptors;
    QList<ThingDescriptor> knxGenericUpDownThingDescriptors;
    QList<ThingDescriptor> knxGenericScalingThingDescriptors;
    QList<ThingDescriptor> knxGenericTemperatureSensorThingDescriptors;
    QList<ThingDescriptor> knxGenericLightSensorThingDescriptors;
    QList<ThingDescriptor> knxGenericWindSpeedSensorThingDescriptors;

    qCDebug(dcKnx()) << "Found" << groupInformation.projectIds().count() << "project ids";
    foreach (const QString &projectId, groupInformation.projectIds()) {
        qCDebug(dcKnx()) << "Project id" << projectId << groupInformation.projectName(projectId);
        foreach (const QString &installation, groupInformation.installations(projectId)) {
            QVector<QKnxGroupAddressInfo> groupAddresses = groupInformation.addressInfos(projectId, installation);
            qCDebug(dcKnx()) << "Installation contains" << groupAddresses.count() << "group addresses.";

            foreach (const QKnxGroupAddressInfo &addressInfo, groupAddresses) {
                qCDebug(dcKnx()) << "-" << addressInfo.name();
                qCDebug(dcKnx()) << "    Description:" << addressInfo.description();
                qCDebug(dcKnx()) << "    Address:" << addressInfo.address().toString();
                qCDebug(dcKnx()) << "    DataPoint Type:" << addressInfo.datapointType();
                switch (addressInfo.datapointType()) {
                case QKnxDatapointType::Type::DptSwitch: {
                    // Check if we already added a switch device
                    ParamList params;
                    params.append(Param(knxGenericSwitchThingKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericSwitchThingKnxAddressParamTypeId, addressInfo.address().toString()));
                    Thing *thing = myThings().findByParams(params);

                    // If there is already a device with this params, continue
                    if (thing) continue;

                    qCDebug(dcKnx()) << "Found new switch" << params;

                    ThingDescriptor descriptor(knxGenericSwitchThingClassId, addressInfo.name() + " (" + tr("Generic Switch") + ")", addressInfo.address().toString(), parentThing->id());
                    descriptor.setParams(params);
                    knxGenericSwitchThingDescriptors.append(descriptor);
                    break;
                }
                case QKnxDatapointType::Type::DptUpDown: {
                    // Check if we already added a shutter device
                    ParamList params;
                    params.append(Param(knxGenericUpDownThingKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericUpDownThingKnxAddressParamTypeId, addressInfo.address().toString()));
                    Thing *thing = myThings().findByParams(params);

                    // If there is already a device with this params, continue
                    if (thing) continue;

                    qCDebug(dcKnx()) << "Found new shutter" << params;

                    ThingDescriptor descriptor(knxGenericUpDownThingClassId, addressInfo.name() + " (" + tr("Generic UpDown") + ")", addressInfo.address().toString(), parentThing->id());
                    descriptor.setParams(params);
                    knxGenericUpDownThingDescriptors.append(descriptor);
                    break;
                }
                case QKnxDatapointType::Type::DptScaling: {
                    // Check if we already added a dimmer device
                    ParamList params;
                    params.append(Param(knxGenericScalingThingKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericScalingThingKnxAddressParamTypeId, addressInfo.address().toString()));
                    Thing *thing = myThings().findByParams(params);

                    // If there is already a device with this params, continue
                    if (thing) continue;

                    qCDebug(dcKnx()) << "Found new dimmer" << params;
                    ThingDescriptor descriptor(knxGenericScalingThingClassId, addressInfo.name() + " (" + tr("Generic Scaling") + ")", addressInfo.address().toString(), parentThing->id());
                    descriptor.setParams(params);
                    knxGenericScalingThingDescriptors.append(descriptor);
                    break;
                }
                case QKnxDatapointType::Type::DptTemperatureCelsius: {
                    // Check if we already added a generic temperature sensor device
                    ParamList params;
                    params.append(Param(knxGenericTemperatureSensorThingKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericTemperatureSensorThingKnxAddressParamTypeId, addressInfo.address().toString()));
                    Thing *thing = myThings().findByParams(params);

                    // If there is already a device with this params, continue
                    if (thing) continue;

                    qCDebug(dcKnx()) << "Found new temperature sensor" << params;
                    ThingDescriptor descriptor(knxGenericTemperatureSensorThingClassId, addressInfo.name() + " (" + tr("Generic temperature sensor") + ")", addressInfo.address().toString(), parentThing->id());
                    descriptor.setParams(params);
                    knxGenericTemperatureSensorThingDescriptors.append(descriptor);
                    break;
                }
                case QKnxDatapointType::Type::DptValueLux: {
                    // Check if we already added a generic light sensor device
                    ParamList params;
                    params.append(Param(knxGenericLightSensorThingKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericLightSensorThingKnxAddressParamTypeId, addressInfo.address().toString()));
                    Thing *thing = myThings().findByParams(params);

                    // If there is already a device with this params, continue
                    if (thing) continue;

                    qCDebug(dcKnx()) << "Found new light sensor" << params;
                    ThingDescriptor descriptor(knxGenericLightSensorThingClassId, addressInfo.name() + " (" + tr("Generic light sensor") + ")", addressInfo.address().toString(), parentThing->id());
                    descriptor.setParams(params);
                    knxGenericLightSensorThingDescriptors.append(descriptor);
                    break;
                }
                case QKnxDatapointType::Type::DptWindSpeed: {
                    // Check if we already added a generic wind speed sensor device
                    ParamList params;
                    params.append(Param(knxGenericWindSpeedSensorThingKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericWindSpeedSensorThingKnxAddressParamTypeId, addressInfo.address().toString()));
                    Thing *thing = myThings().findByParams(params);

                    // If there is already a device with this params, continue
                    if (thing) continue;

                    qCDebug(dcKnx()) << "Found new wind speed sensor" << params;
                    ThingDescriptor descriptor(knxGenericWindSpeedSensorThingClassId, addressInfo.name() + " (" + tr("Generic wind speed sensor") + ")", addressInfo.address().toString(), parentThing->id());
                    descriptor.setParams(params);
                    knxGenericWindSpeedSensorThingDescriptors.append(descriptor);
                    break;
                }
                default:
                    qCWarning(dcKnx()) << "Unkandled DataPoint Type" << addressInfo.datapointType();
                    break;
                }
            }
        }
    }

    if (!knxGenericSwitchThingDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericSwitchThingDescriptors.count() << "new KNX switch devices";
        emit autoThingsAppeared(knxGenericSwitchThingDescriptors);
    }

    if (!knxGenericUpDownThingDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericUpDownThingDescriptors.count() << "new KNX shutter devices";
        emit autoThingsAppeared(knxGenericUpDownThingDescriptors);
    }

    if (!knxGenericScalingThingDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericScalingThingDescriptors.count() << "new KNX dimmer devices";
        emit autoThingsAppeared(knxGenericScalingThingDescriptors);
    }

    if (!knxGenericTemperatureSensorThingDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericTemperatureSensorThingDescriptors.count() << "new KNX temperature sensor devices";
        emit autoThingsAppeared(knxGenericTemperatureSensorThingDescriptors);
    }

    if (!knxGenericLightSensorThingDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericLightSensorThingDescriptors.count() << "new KNX light sensor devices";
        emit autoThingsAppeared(knxGenericLightSensorThingDescriptors);
    }

    if (!knxGenericWindSpeedSensorThingDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericWindSpeedSensorThingDescriptors.count() << "new KNX wind speed sensor devices";
        emit autoThingsAppeared(knxGenericWindSpeedSensorThingDescriptors);
    }
}

void IntegrationPluginKnx::destroyGenericDevices(Thing *parentThing)
{
    foreach (Thing *t, myThings()) {
        if (t->parentId() == parentThing->id()) {
            if (t->thingClassId() == knxGenericSwitchThingClassId
                    || t->thingClassId() == knxGenericUpDownThingClassId
                    || t->thingClassId() == knxGenericScalingThingClassId) {
                qCDebug(dcKnx()) << "--> Destroy generic knx device" << t->name() << t->id();
                emit autoThingDisappeared(t->id());
            }
        }
    }
}

void IntegrationPluginKnx::onPluginTimerTimeout()
{
    qCDebug(dcKnx()) << "Refresh sensor data from KNX devices";
    foreach (Thing *thing, myThings()) {

        // Refresh temperature sensor data
        if (thing->thingClassId() == knxGenericTemperatureSensorThingClassId) {
            KnxTunnel *tunnel = getTunnelForDevice(thing);
            if (!tunnel) {
                qCWarning(dcKnx()) << "Could not find tunnel for this device";
                return;
            }

            if (tunnel->connected()) {
                QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericTemperatureSensorThingKnxAddressParamTypeId).toString());
                tunnel->readKnxGroupValue(knxAddress);
            }
        }

        // Refresh light sensor data
        if (thing->thingClassId() == knxGenericLightSensorThingClassId) {
            KnxTunnel *tunnel = getTunnelForDevice(thing);
            if (!tunnel) {
                qCWarning(dcKnx()) << "Could not find tunnel for this device";
                return;
            }

            if (tunnel->connected()) {
                QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, thing->paramValue(knxGenericLightSensorThingKnxAddressParamTypeId).toString());
                tunnel->readKnxGroupValue(knxAddress);
            }
        }
    }
}

void IntegrationPluginKnx::onPluginConfigurationChanged(const ParamTypeId &paramTypeId, const QVariant &value)
{
    if (paramTypeId == knxPluginGenericDevicesEnabledParamTypeId) {
        if (value.toBool()) {
            qCDebug(dcKnx()) << "Generic Knx devices enabled.";
            // Add all generic devices
            foreach (Thing *knxNetIpServer, m_tunnels.values()) {
                qCDebug(dcKnx()) << "Start adding generic knx devices from project file of" << knxNetIpServer->name() << knxNetIpServer->id().toString();
                createGenericDevices(knxNetIpServer);
            }

        } else {
            qCDebug(dcKnx()) << "Generic Knx devices disabled";
            // Remove all generic devices
            foreach (Thing *knxNetIpServer, m_tunnels.values()) {
                qCDebug(dcKnx()) << "Start removing generic knx devices from project file of" << knxNetIpServer->name() << knxNetIpServer->id().toString();
                destroyGenericDevices(knxNetIpServer);
            }
        }
    }
}

void IntegrationPluginKnx::onTunnelConnectedChanged()
{
    KnxTunnel *tunnel = static_cast<KnxTunnel *>(sender());
    Thing *thing = m_tunnels.value(tunnel);
    if (!thing) return;
    thing->setStateValue(knxNetIpServerConnectedStateTypeId, tunnel->connected());

    // Update child devices connected state
    foreach (Thing *t, myThings()) {
        if (t->parentId() == thing->id()) {
            if (t->thingClassId() == knxGenericSwitchThingClassId) {
                t->setStateValue(knxGenericSwitchConnectedStateTypeId, tunnel->connected());
                // Read initial state
                if (tunnel->connected()) {
                    QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, t->paramValue(knxGenericSwitchThingKnxAddressParamTypeId).toString());
                    tunnel->readKnxGroupValue(knxAddress);
                }
            } else if (t->thingClassId() == knxGenericUpDownThingClassId) {
                t->setStateValue(knxGenericUpDownConnectedStateTypeId, tunnel->connected());
            } else if (t->thingClassId() == knxGenericScalingThingClassId) {
                t->setStateValue(knxGenericScalingConnectedStateTypeId, tunnel->connected());
            } else if (t->thingClassId() == knxGenericTemperatureSensorThingClassId) {
                t->setStateValue(knxGenericTemperatureSensorConnectedStateTypeId, tunnel->connected());
                // Read initial state
                if (tunnel->connected()) {
                    QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, t->paramValue(knxGenericTemperatureSensorThingKnxAddressParamTypeId).toString());
                    tunnel->readKnxGroupValue(knxAddress);
                }
            } else if (t->thingClassId() == knxGenericLightSensorThingClassId) {
                t->setStateValue(knxGenericLightSensorConnectedStateTypeId, tunnel->connected());
                // Read initial state
                if (tunnel->connected()) {
                    QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, t->paramValue(knxGenericLightSensorThingKnxAddressParamTypeId).toString());
                    tunnel->readKnxGroupValue(knxAddress);
                }
            } else if (t->thingClassId() == knxGenericWindSpeedSensorThingClassId) {
                t->setStateValue(knxGenericWindSpeedSensorConnectedStateTypeId, tunnel->connected());
                // Read initial state
                if (tunnel->connected()) {
                    QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, t->paramValue(knxGenericWindSpeedSensorThingKnxAddressParamTypeId).toString());
                    tunnel->readKnxGroupValue(knxAddress);
                }
            } else if (t->thingClassId() == knxTriggerThingClassId) {
                t->setStateValue(knxTriggerConnectedStateTypeId, tunnel->connected());
            } else if (t->thingClassId() == knxShutterThingClassId) {
                t->setStateValue(knxShutterConnectedStateTypeId, tunnel->connected());
            } else if (t->thingClassId() == knxLightThingClassId) {
                t->setStateValue(knxLightConnectedStateTypeId, tunnel->connected());
                // Read initial state
                if (tunnel->connected()) {
                    QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, t->paramValue(knxLightThingKnxAddressParamTypeId).toString());
                    tunnel->readKnxGroupValue(knxAddress);
                }
            } else if (t->thingClassId() == knxDimmableLightThingClassId) {
                t->setStateValue(knxDimmableLightConnectedStateTypeId, tunnel->connected());
                // Read initial state
                if (tunnel->connected()) {
                    QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, t->paramValue(knxDimmableLightThingKnxSwitchAddressParamTypeId).toString());
                    tunnel->readKnxGroupValue(knxAddress);
                    // TODO: read brightness
                }
            } else {
                qCWarning(dcKnx()) << "Unhandled device class on tunnel connected changed" << t->thingClassId();
            }
        }
    }
}

void IntegrationPluginKnx::onTunnelFrameReceived(const QKnxLinkLayerFrame &frame)
{
    foreach (Thing *thing, myThings()) {

        // Generic switch
        if (thing->thingClassId() == knxGenericSwitchThingClassId) {
            if (thing->paramValue(knxGenericSwitchThingKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                bool power = static_cast<bool>(frame.tpdu().data().toByteArray().at(0));
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic Switch notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1").arg(power) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    thing->setStateValue(knxGenericSwitchPowerStateTypeId, power);
                }
            }
        }

        // Generic UpDown
        if (thing->thingClassId() == knxGenericUpDownThingClassId) {
            if (thing->paramValue(knxGenericUpDownThingKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic UpDown notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray();
            }
        }

        // Generic Scaling
        if (thing->thingClassId() == knxGenericScalingThingClassId) {
            if (thing->paramValue(knxGenericScalingThingKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                QKnxScaling knxScaling;
                knxScaling.setBytes(frame.tpdu().data(), 0, 1);
                int scaling = static_cast<int>(knxScaling.value());
                int percentage = qRound(scaling * 100.0 / 255.0);
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic Scaling notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1 %2 [%]").arg(scaling).arg(percentage)  : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    thing->setStateValue(knxGenericScalingScaleStateTypeId, percentage);
                }
            }
        }

        // Generic temperature sensor
        if (thing->thingClassId() == knxGenericTemperatureSensorThingClassId) {
            if (thing->paramValue(knxGenericTemperatureSensorThingKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {

                QKnxTemperatureCelsius knxTemperature;
                knxTemperature.setBytes(frame.tpdu().data(), 0, 2);
                double temperature = static_cast<double>(knxTemperature.value());
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic Temperature sensor notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1  [°C]").arg(temperature) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    thing->setStateValue(knxGenericTemperatureSensorTemperatureStateTypeId, temperature);
                }
            }
        }


        // Generic light sensor
        if (thing->thingClassId() == knxGenericLightSensorThingClassId) {
            if (thing->paramValue(knxGenericLightSensorThingKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {

                QKnxValueLux knxLightIntensity;
                knxLightIntensity.setBytes(frame.tpdu().data(), 0, 2);
                double lightIntensity = static_cast<double>(knxLightIntensity.value());
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic light sensor notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1 [Lux]").arg(lightIntensity) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    thing->setStateValue(knxGenericLightSensorLightIntensityStateTypeId, lightIntensity);
                }
            }
        }

        // Generic wind speed sensor
        if (thing->thingClassId() == knxGenericWindSpeedSensorThingClassId) {
            if (thing->paramValue(knxGenericWindSpeedSensorThingKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {

                QKnxWindSpeed knxWindSpeed;
                knxWindSpeed.setBytes(frame.tpdu().data(), 0, 2);
                double windSpeed = static_cast<double>(knxWindSpeed.value());
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic light sensor notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1 [m/s]").arg(windSpeed) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    thing->setStateValue(knxGenericWindSpeedSensorWindSpeedStateTypeId, windSpeed);
                }
            }
        }

        // Trigger
        if (thing->thingClassId() == knxTriggerThingClassId) {
            if (thing->paramValue(knxShutterThingKnxAddressStepParamTypeId).toString() == frame.destinationAddress().toString()) {
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Trigger notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray();

                emitEvent(Event(knxTriggerTriggeredEventTypeId, thing->id()));
            }
        }

        // Shutter
        if (thing->thingClassId() == knxShutterThingClassId) {
            if (thing->paramValue(knxShutterThingKnxAddressStepParamTypeId).toString() == frame.destinationAddress().toString()) {
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Shutter Step notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray();


            } else if (thing->paramValue(knxShutterThingKnxAddressUpDownParamTypeId).toString() == frame.destinationAddress().toString()) {
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Shutter UpDown notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray();
            }
        }

        // Light
        if (thing->thingClassId() == knxLightThingClassId) {
            if (thing->paramValue(knxLightThingKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                bool power = static_cast<bool>(frame.tpdu().data().toByteArray().at(0));
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Light notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1").arg(power) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    thing->setStateValue(knxLightPowerStateTypeId, power);
                }
            }
        }

        // Dimmable light
        if (thing->thingClassId() == knxDimmableLightThingClassId) {

            // Switch
            if (thing->paramValue(knxDimmableLightThingKnxSwitchAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                bool power = static_cast<bool>(frame.tpdu().data().toByteArray().at(0));
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Dimmable light Switch notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1").arg(power) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    thing->setStateValue(knxDimmableLightPowerStateTypeId, power);
                }
            }

            // Scale
            if (thing->paramValue(knxDimmableLightThingKnxScalingAddressParamTypeId).toString() == frame.destinationAddress().toString()) {

                QKnxScaling knxScaling;
                knxScaling.setBytes(frame.tpdu().data(), 0, 1);
                double scaling = knxScaling.value();
                int percentage = qRound(scaling * 100.0 / 255.0);
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Dimmable light scaling notification"
                                 << thing->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1 %2 [%]").arg(scaling).arg(percentage)  : "");

                //                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                //                    device->setStateValue(knxDimmableLightBrightnessStateTypeId, percentage);
                //                }
            }

        }
    }
}


