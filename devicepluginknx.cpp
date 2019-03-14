/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2019 Simon Stürz <simon.stuerz@guh.io>                   *
 *                                                                         *
 *  This file is part of nymea.                                            *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation; either           *
 *  version 2.1 of the License, or (at your option) any later version.     *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "plugininfo.h"
#include "devicepluginknx.h"

#include <QObject>
#include <QDateTime>
#include <QKnxAddress>
#include <QNetworkInterface>
#include <QKnxGroupAddressInfos>

DevicePluginKnx::DevicePluginKnx()
{

}

void DevicePluginKnx::init()
{
    m_discovery = new KnxServerDiscovery(this);
    connect(m_discovery, &KnxServerDiscovery::discoveryFinished, this, &DevicePluginKnx::onDiscoveryFinished);
}

void DevicePluginKnx::startMonitoringAutoDevices()
{
    // Start seaching for devices which can be discovered and added automatically
}

DeviceManager::DeviceError DevicePluginKnx::discoverDevices(const DeviceClassId &deviceClassId, const ParamList &params)
{
    Q_UNUSED(params)

    if (deviceClassId == knxNetIpServerDeviceClassId) {
        if (!m_discovery->startDisovery()) {
            return DeviceManager::DeviceErrorDeviceInUse;
        } else {
            return DeviceManager::DeviceErrorAsync;
        }
    }

    return DeviceManager::DeviceErrorNoError;
}

DeviceManager::DeviceSetupStatus DevicePluginKnx::setupDevice(Device *device)
{
    qCDebug(dcKnx()) << "Setup device" << device->name() << device->params();

    if (device->deviceClassId() == knxNetIpServerDeviceClassId) {
        QHostAddress remoteAddress = QHostAddress(device->paramValue(knxNetIpServerDeviceAddressParamTypeId).toString());
        KnxTunnel *tunnel = new KnxTunnel(remoteAddress, this);
        connect(tunnel, &KnxTunnel::connectedChanged, this, &DevicePluginKnx::onTunnelConnectedChanged);
        connect(tunnel, &KnxTunnel::frameReceived, this, &DevicePluginKnx::onTunnelFrameReceived);
        m_tunnels.insert(tunnel, device);
    }

    if (device->deviceClassId() == knxShutterDeviceClassId) {
        QHostAddress tunnelAddress =  QHostAddress(device->paramValue(knxShutterDeviceAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for address" << tunnelAddress.toString();
            return DeviceManager::DeviceSetupStatusFailure;
        }

        // TODO: remove generic devices for this shutter

        device->setParentId(m_tunnels.value(tunnel)->id());
    }

    if (device->deviceClassId() == knxLightDeviceClassId) {
        QHostAddress tunnelAddress =  QHostAddress(device->paramValue(knxLightDeviceAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for address" << tunnelAddress.toString();
            return DeviceManager::DeviceSetupStatusFailure;
        }

        // TODO: remove generic device for this shutter

        device->setParentId(m_tunnels.value(tunnel)->id());
    }

    if (device->deviceClassId() == knxDimmableLightDeviceClassId) {
        QHostAddress tunnelAddress =  QHostAddress(device->paramValue(knxDimmableLightDeviceAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for address" << tunnelAddress.toString();
            return DeviceManager::DeviceSetupStatusFailure;
        }

        // TODO: remove generic device for this shutter

        device->setParentId(m_tunnels.value(tunnel)->id());
    }

    return DeviceManager::DeviceSetupStatusSuccess;
}

void DevicePluginKnx::postSetupDevice(Device *device)
{
    qCDebug(dcKnx()) << "Post setup device" << device->name() << device->params();
    if (device->deviceClassId() == knxNetIpServerDeviceClassId) {
        KnxTunnel *tunnel = m_tunnels.key(device);
        tunnel->connectTunnel();
        snycProjectFile(device);
    }

    if (device->deviceClassId() == knxShutterDeviceClassId) {
        QHostAddress tunnelAddress =  QHostAddress(device->paramValue(knxShutterDeviceAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (tunnel) device->setStateValue(knxShutterConnectedStateTypeId, tunnel->connected());
    }

    if (device->deviceClassId() == knxLightDeviceClassId) {
        QHostAddress tunnelAddress =  QHostAddress(device->paramValue(knxLightDeviceAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (tunnel) device->setStateValue(knxLightConnectedStateTypeId, tunnel->connected());
    }

    if (device->deviceClassId() == knxDimmableLightDeviceClassId) {
        QHostAddress tunnelAddress =  QHostAddress(device->paramValue(knxDimmableLightDeviceAddressParamTypeId).toString());
        KnxTunnel *tunnel = nullptr;
        foreach (KnxTunnel *knxTunnel, m_tunnels.keys()) {
            if (knxTunnel->remoteAddress() == tunnelAddress) {
                tunnel = knxTunnel;
            }
        }

        if (tunnel) device->setStateValue(knxDimmableLightConnectedStateTypeId, tunnel->connected());
    }
}

void DevicePluginKnx::deviceRemoved(Device *device)
{
    qCDebug(dcKnx()) << "Remove device" << device->name() << device->params();
    if (device->deviceClassId() == knxNetIpServerDeviceClassId) {
        KnxTunnel *tunnel = m_tunnels.key(device);
        m_tunnels.remove(tunnel);
        tunnel->disconnectTunnel();
        tunnel->deleteLater();
    }
}

DeviceManager::DeviceError DevicePluginKnx::executeAction(Device *device, const Action &action)
{
    qCDebug(dcKnx()) << "Executing action for device" << device->name() << action.actionTypeId().toString() << action.params();

    // Generic switch
    if (device->deviceClassId() == knxGenericSwitchDeviceClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(device);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxGenericSwitchDeviceKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxGenericSwitchPowerActionTypeId) {
            tunnel->sendKnxDpdSwitchFrame(knxAddress, action.param(knxGenericSwitchPowerActionPowerParamTypeId).value().toBool());
        }

        if (action.actionTypeId() == knxGenericSwitchReadActionTypeId) {
            tunnel->readKnxDpdSwitchState(knxAddress);
        }
    }

    // Generic UpDown
    if (device->deviceClassId() == knxGenericUpDownDeviceClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(device);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }
        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxGenericUpDownDeviceKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxGenericUpDownOpenActionTypeId) {
            tunnel->sendKnxDpdUpDownFrame(knxAddress, true);
        }

        if (action.actionTypeId() == knxGenericUpDownCloseActionTypeId) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxGenericUpDownDeviceKnxAddressParamTypeId).toString());
            tunnel->sendKnxDpdUpDownFrame(knxAddress, false);
        }
    }

    // Generic Scaling
    if (device->deviceClassId() == knxGenericScalingDeviceClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(device);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxGenericScalingDeviceKnxAddressParamTypeId).toString());
        int scaling = action.param(knxGenericScalingScaleActionScaleParamTypeId).value().toInt();

        tunnel->sendKnxDpdScalingFrame(knxAddress, scaling);
    }

    // Shutter
    if (device->deviceClassId() == knxShutterDeviceClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(device);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        QKnxAddress knxAddressStep = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxShutterDeviceKnxAddressStepParamTypeId).toString());
        QKnxAddress knxAddressUpDown = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxShutterDeviceKnxAddressUpDownParamTypeId).toString());

        if (action.actionTypeId() == knxShutterOpenActionTypeId) {
            // Note: first send step, then delayed the up/down command
            tunnel->sendKnxDpdStepFrame(knxAddressStep, false);
            QTimer::singleShot(150, [tunnel, knxAddressUpDown]() { tunnel->sendKnxDpdUpDownFrame(knxAddressUpDown, true); });
            return DeviceManager::DeviceErrorNoError;
        }

        if (action.actionTypeId() == knxShutterCloseActionTypeId) {
            // Note: first send step, then delayed the up/down command
            tunnel->sendKnxDpdStepFrame(knxAddressStep, true);
            QTimer::singleShot(150, [tunnel, knxAddressUpDown]() { tunnel->sendKnxDpdUpDownFrame(knxAddressUpDown, false); });
            return DeviceManager::DeviceErrorNoError;
        }

        if (action.actionTypeId() == knxShutterStopActionTypeId) {
            tunnel->sendKnxDpdStepFrame(knxAddressStep, true);
            return DeviceManager::DeviceErrorNoError;
        }
    }

    // Light
    if (device->deviceClassId() == knxLightDeviceClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(device);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxLightDeviceKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxLightPowerActionTypeId) {
            tunnel->sendKnxDpdSwitchFrame(knxAddress, action.param(knxLightPowerActionPowerParamTypeId).value().toBool());
        }

        if (action.actionTypeId() == knxLightReadActionTypeId) {
            tunnel->readKnxDpdSwitchState(knxAddress);
        }
    }

    //  Dimmable Light
    if (device->deviceClassId() == knxDimmableLightDeviceClassId) {
        KnxTunnel *tunnel = getTunnelForDevice(device);
        if (!tunnel) {
            qCWarning(dcKnx()) << "Could not find tunnel for this device";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        if (!tunnel->connected()) {
            qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
            return DeviceManager::DeviceErrorHardwareNotAvailable;
        }

        QKnxAddress knxSwitchAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxDimmableLightDeviceKnxSwitchAddressParamTypeId).toString());
        QKnxAddress knxScalingAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxDimmableLightDeviceKnxScalingAddressParamTypeId).toString());

        if (action.actionTypeId() == knxDimmableLightPowerActionTypeId) {
            tunnel->sendKnxDpdSwitchFrame(knxSwitchAddress, action.param(knxDimmableLightPowerActionPowerParamTypeId).value().toBool());
        }

        if (action.actionTypeId() == knxDimmableLightBrightnessActionTypeId) {
            tunnel->sendKnxDpdSwitchFrame(knxScalingAddress, action.param(knxDimmableLightBrightnessActionBrightnessParamTypeId).value().toUInt());
        }
    }


    return DeviceManager::DeviceErrorNoError;
}

KnxTunnel *DevicePluginKnx::getTunnelForDevice(Device *device)
{
    Device *parentDevice = nullptr;
    foreach (Device *d, myDevices()) {
        if (d->id() == device->parentId()) {
            parentDevice = d;
        }
    }

    if (!parentDevice) {
        qCWarning(dcKnx()) << "Could not find parent device for" << device->name() << device->id().toString();
        return nullptr;
    }

    return m_tunnels.key(parentDevice);
}

void DevicePluginKnx::snycProjectFile(Device *knxNetIpServerDevice)
{
    QString projectFilePath = knxNetIpServerDevice->paramValue(knxNetIpServerDeviceKnxProjectFileParamTypeId).toString();

    QKnxGroupAddressInfos groupInformation(projectFilePath);
    qCDebug(dcKnx()) << "Opening project file" << groupInformation.projectFile();
    if (!groupInformation.parse()) {
        qCWarning(dcKnx()) << "Could not parse project file" << groupInformation.projectFile() << groupInformation.errorString();
        return;
    }

    QList<DeviceDescriptor> knxGenericSwitchDeviceDescriptors;
    QList<DeviceDescriptor> knxGenericUpDownDeviceDescriptors;
    QList<DeviceDescriptor> knxGenericScalingDeviceDescriptors;

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
                    params.append(Param(knxGenericSwitchDeviceKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericSwitchDeviceKnxAddressParamTypeId, addressInfo.address().toString()));
                    Device *device = findDeviceByParams(params);

                    // If there is already a device with this params, continue
                    if (device) continue;

                    qCDebug(dcKnx()) << "Found new switch" << params;

                    DeviceDescriptor descriptor(knxGenericSwitchDeviceClassId, addressInfo.name() + " (" + tr("Generic Switch") + ")", addressInfo.address().toString(), knxNetIpServerDevice->id());
                    descriptor.setParams(params);
                    knxGenericSwitchDeviceDescriptors.append(descriptor);
                    break;
                }
                case QKnxDatapointType::Type::DptUpDown: {
                    // Check if we already added a shutter device
                    ParamList params;
                    params.append(Param(knxGenericUpDownDeviceKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericUpDownDeviceKnxAddressParamTypeId, addressInfo.address().toString()));
                    Device *device = findDeviceByParams(params);

                    // If there is already a device with this params, continue
                    if (device) continue;

                    qCDebug(dcKnx()) << "Found new shutter" << params;

                    DeviceDescriptor descriptor(knxGenericUpDownDeviceClassId, addressInfo.name() + " (" + tr("Generic UpDown") + ")", addressInfo.address().toString(), knxNetIpServerDevice->id());
                    descriptor.setParams(params);
                    knxGenericUpDownDeviceDescriptors.append(descriptor);
                    break;
                }
                case QKnxDatapointType::Type::DptScaling: {
                    // Check if we already added a dimmer device
                    ParamList params;
                    params.append(Param(knxGenericScalingDeviceKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxGenericScalingDeviceKnxAddressParamTypeId, addressInfo.address().toString()));
                    Device *device = findDeviceByParams(params);

                    // If there is already a device with this params, continue
                    if (device) continue;

                    qCDebug(dcKnx()) << "Found new dimmer" << params;
                    DeviceDescriptor descriptor(knxGenericScalingDeviceClassId, addressInfo.name() + " (" + tr("Generic Scaling") + ")", addressInfo.address().toString(), knxNetIpServerDevice->id());
                    descriptor.setParams(params);
                    knxGenericScalingDeviceDescriptors.append(descriptor);
                    break;
                }
                default:
                    qCWarning(dcKnx()) << "Unkandled DataPoint Type" << addressInfo.datapointType();
                    break;
                }
            }
        }
    }

    if (!knxGenericSwitchDeviceDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericSwitchDeviceDescriptors.count() << "new KNX switch devices";
        emit autoDevicesAppeared(knxGenericSwitchDeviceClassId, knxGenericSwitchDeviceDescriptors);
    }

    if (!knxGenericUpDownDeviceDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericUpDownDeviceDescriptors.count() << "new KNX shutter devices";
        emit autoDevicesAppeared(knxGenericUpDownDeviceClassId, knxGenericUpDownDeviceDescriptors);
    }

    if (!knxGenericScalingDeviceDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "--> Found" << knxGenericScalingDeviceDescriptors.count() << "new KNX dimmer devices";
        emit autoDevicesAppeared(knxGenericScalingDeviceClassId, knxGenericScalingDeviceDescriptors);
    }

}

void DevicePluginKnx::onDiscoveryFinished()
{
    qCDebug(dcKnx()) << "Discovery finished.";

    QList<DeviceDescriptor> deviceDescriptors;
    foreach (const QKnxNetIpServerInfo &serverInfo, m_discovery->discoveredServers()) {
        qCDebug(dcKnx()) << "Found server:" << QString("%1:%2").arg(serverInfo.controlEndpointAddress().toString()).arg(serverInfo.controlEndpointPort());
        KnxServerDiscovery::printServerInfo(serverInfo);
        DeviceDescriptor descriptor(knxNetIpServerDeviceClassId, "KNX NetIp Server", QString("%1:%2").arg(serverInfo.controlEndpointAddress().toString()).arg(serverInfo.controlEndpointPort()));
        ParamList params;
        params.append(Param(knxNetIpServerDeviceAddressParamTypeId, serverInfo.controlEndpointAddress().toString()));
        params.append(Param(knxNetIpServerDevicePortParamTypeId, serverInfo.controlEndpointPort()));
        descriptor.setParams(params);
        foreach (Device *existingDevice, myDevices()) {
            if (existingDevice->paramValue(knxNetIpServerDeviceAddressParamTypeId).toString() == serverInfo.controlEndpointAddress().toString()) {
                descriptor.setDeviceId(existingDevice->id());
                break;
            }
        }
        deviceDescriptors.append(descriptor);
    }

    emit devicesDiscovered(knxNetIpServerDeviceClassId, deviceDescriptors);
}

void DevicePluginKnx::onTunnelConnectedChanged()
{
    KnxTunnel *tunnel = static_cast<KnxTunnel *>(sender());
    Device *device = m_tunnels.value(tunnel);
    if (!device) return;
    device->setStateValue(knxNetIpServerConnectedStateTypeId, tunnel->connected());

    foreach (Device *d, myDevices()) {
        if (d->parentId() == device->id()) {
            if (d->deviceClassId() == knxGenericSwitchDeviceClassId) {
                d->setStateValue(knxGenericSwitchConnectedStateTypeId, tunnel->connected());
            } else if (d->deviceClassId() == knxGenericUpDownDeviceClassId) {
                d->setStateValue(knxGenericUpDownConnectedStateTypeId, tunnel->connected());
            } else if (d->deviceClassId() == knxGenericScalingDeviceClassId) {
                d->setStateValue(knxGenericScalingConnectedStateTypeId, tunnel->connected());
            } else if (d->deviceClassId() == knxShutterDeviceClassId) {
                d->setStateValue(knxShutterConnectedStateTypeId, tunnel->connected());
            } else if (d->deviceClassId() == knxLightDeviceClassId) {
                d->setStateValue(knxLightConnectedStateTypeId, tunnel->connected());
            } else if (d->deviceClassId() == knxDimmableLightDeviceClassId) {
                d->setStateValue(knxDimmableLightConnectedStateTypeId, tunnel->connected());
            } else {
                qCWarning(dcKnx()) << "Unhandled device class on tunnel connected changed" << d->deviceClassId();
            }
        }
    }
}

void DevicePluginKnx::onTunnelFrameReceived(const QKnxLinkLayerFrame &frame)
{
    foreach (Device *device, myDevices()) {
        if (device->deviceClassId() == knxGenericSwitchDeviceClassId) {
            if (device->paramValue(knxGenericSwitchDeviceKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                bool power = static_cast<bool>(frame.tpdu().data().toByteArray().at(0));
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic Switch notification"
                                 << device->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1").arg(power) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    device->setStateValue(knxGenericSwitchPowerStateTypeId, power);
                }
            }
        }

        if (device->deviceClassId() == knxGenericUpDownDeviceClassId) {
            if (device->paramValue(knxGenericUpDownDeviceKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic UpDown notification"
                                 << device->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray();
            }
        }

        if (device->deviceClassId() == knxGenericScalingDeviceClassId) {
            if (device->paramValue(knxGenericScalingDeviceKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                int scale = static_cast<int>(static_cast<quint8>(frame.tpdu().data().toByteArray().at(0)));
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Generic Scaling notification"
                                 << device->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1").arg(scale) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    device->setStateValue(knxGenericScalingScaleStateTypeId, scale);
                }
            }
        }

        if (device->deviceClassId() == knxShutterDeviceClassId) {
            if (device->paramValue(knxShutterDeviceKnxAddressStepParamTypeId).toString() == frame.destinationAddress().toString()) {
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Shutter Step notification"
                                 << device->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray();


            } else if (device->paramValue(knxShutterDeviceKnxAddressUpDownParamTypeId).toString() == frame.destinationAddress().toString()) {
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Shutter UpDown notification"
                                 << device->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray();
            }
        }

        if (device->deviceClassId() == knxLightDeviceClassId) {
            if (device->paramValue(knxLightDeviceKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                bool power = static_cast<bool>(frame.tpdu().data().toByteArray().at(0));
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Light notification"
                                 << device->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1").arg(power) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    device->setStateValue(knxLightPowerStateTypeId, power);
                }
            }
        }

        if (device->deviceClassId() == knxDimmableLightDeviceClassId) {
            if (device->paramValue(knxDimmableLightDeviceKnxSwitchAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                bool power = static_cast<bool>(frame.tpdu().data().toByteArray().at(0));
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Dimmable light Switch notification"
                                 << device->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1").arg(power) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    device->setStateValue(knxDimmableLightPowerStateTypeId, power);
                }
            }

            if (device->paramValue(knxDimmableLightDeviceKnxScalingAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                int scale = static_cast<int>(static_cast<quint8>(frame.tpdu().data().toByteArray().at(0)));
                qCDebug(dcKnx()) << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss")
                                 << "Dimmable light Scaling notification"
                                 << device->name()
                                 << frame.sourceAddress().toString() << "-->" << frame.destinationAddress().toString()
                                 << frame.tpdu().data().toHex().toByteArray()
                                 << (!frame.tpdu().data().toByteArray().isEmpty() ? QString("%1").arg(scale) : "");

                if (!frame.tpdu().data().toByteArray().isEmpty()) {
                    device->setStateValue(knxDimmableLightBrightnessStateTypeId, scale);
                }
            }
        }
    }
}

