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

    if (device->deviceClassId() == knxSwitchDeviceClassId) {
        if (action.actionTypeId() == knxSwitchPowerActionTypeId) {
            KnxTunnel *tunnel = getTunnelForDevice(device);
            if (!tunnel) {
                qCWarning(dcKnx()) << "Could not find tunnel for this device";
                return DeviceManager::DeviceErrorHardwareNotAvailable;
            }

            if (!tunnel->connected()) {
                qCWarning(dcKnx()) << "The corresponding tunnel is not connected.";
                return DeviceManager::DeviceErrorHardwareNotAvailable;
            }

            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxSwitchDeviceKnxAddressParamTypeId).toString());
            tunnel->switchKnxDevice(knxAddress, action.param(knxSwitchPowerActionPowerParamTypeId).value().toBool());
        }
    }

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
        QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxShutterDeviceKnxAddressParamTypeId).toString());

        if (action.actionTypeId() == knxShutterOpenActionTypeId) {
            tunnel->switchKnxShutter(knxAddress, true);
        }

        if (action.actionTypeId() == knxShutterOpenActionTypeId) {
            QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxShutterDeviceKnxAddressParamTypeId).toString());
            tunnel->switchKnxShutter(knxAddress, false);
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

    QList<DeviceDescriptor> knxSwitchDeviceDescriptors;
    QList<DeviceDescriptor> knxShutterDeviceDescriptors;

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
                    params.append(Param(knxSwitchDeviceKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxSwitchDeviceKnxAddressParamTypeId, addressInfo.address().toString()));
                    Device *device = findDeviceByParams(params);

                    // If there is already a device with this params, continue
                    if (device) continue;

                    DeviceDescriptor descriptor(knxSwitchDeviceClassId, addressInfo.name(), addressInfo.address().toString(), knxNetIpServerDevice->id());
                    descriptor.setParams(params);
                    knxSwitchDeviceDescriptors.append(descriptor);
                    break;
                }
                case QKnxDatapointType::Type::DptUpDown: {
                    // Check if we already added a shutter device
                    ParamList params;
                    params.append(Param(knxShutterDeviceKnxNameParamTypeId, addressInfo.name()));
                    params.append(Param(knxShutterDeviceKnxAddressParamTypeId, addressInfo.address().toString()));
                    Device *device = findDeviceByParams(params);

                    // If there is already a device with this params, continue
                    if (device) continue;

                    DeviceDescriptor descriptor(knxShutterDeviceClassId, addressInfo.name(), addressInfo.address().toString(), knxNetIpServerDevice->id());
                    descriptor.setParams(params);
                    knxShutterDeviceDescriptors.append(descriptor);
                    break;
                }
                default:
                    qCWarning(dcKnx()) << "Unkandled DataPoint Type" << addressInfo.datapointType();
                    break;
                }
            }
        }
    }

    if (!knxSwitchDeviceDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "Found" << knxSwitchDeviceDescriptors.count() << "new KNX switch devices";
        emit autoDevicesAppeared(knxSwitchDeviceClassId, knxSwitchDeviceDescriptors);
    }

    if (!knxShutterDeviceDescriptors.isEmpty()) {
        qCDebug(dcKnx()) << "Found" << knxShutterDeviceDescriptors.count() << "new KNX shutter devices";
        emit autoDevicesAppeared(knxShutterDeviceClassId, knxShutterDeviceDescriptors);
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
            if (d->deviceClassId() == knxSwitchDeviceClassId) {
                d->setStateValue(knxSwitchConnectedStateTypeId, tunnel->connected());
            } else if (d->deviceClassId() == knxShutterDeviceClassId) {
                d->setStateValue(knxShutterConnectedStateTypeId, tunnel->connected());
            }
        }
    }

}

void DevicePluginKnx::onTunnelFrameReceived(const QKnxLinkLayerFrame &frame)
{
    foreach (Device *device, myDevices()) {
        if (device->deviceClassId() == knxSwitchDeviceClassId) {
            if (device->paramValue(knxSwitchDeviceKnxAddressParamTypeId).toString() == frame.destinationAddress().toString()) {
                bool power = static_cast<bool>(frame.tpdu().data().toByteArray().at(0));
                qCDebug(dcKnx()) << "Light switch notification" << device->name() << frame.destinationAddress().toString() << power;
                device->setStateValue(knxSwitchPowerStateTypeId, power);
            }
        }

    }
}
