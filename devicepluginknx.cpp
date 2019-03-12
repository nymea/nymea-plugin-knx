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

void DevicePluginKnx::postSetupDevice(Device *device)
{
    qCDebug(dcKnx()) << "Post setup device" << device->name() << device->params();
    if (device->deviceClassId() == knxNetIpServerDeviceClassId) {
        KnxTunnel *tunnel = m_tunnels.key(device);
        tunnel->connectTunnel();
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
        m_tunnels.insert(tunnel, device);
    }

    return DeviceManager::DeviceSetupStatusSuccess;
}

DeviceManager::DeviceError DevicePluginKnx::executeAction(Device *device, const Action &action)
{
    qCDebug(dcKnx()) << "Executing action for device" << device->name() << action.actionTypeId().toString() << action.params();

    if (device->deviceClassId() == knxNetIpServerDeviceClassId) {
        // Add a new device for this knx netip server



    }

    if (device->deviceClassId() == knxLightDeviceClassId) {
        if (action.actionTypeId() == knxLightPowerActionTypeId) {
            // TODO: get the parent netip server and switch the light with the given address

//            foreach (KnxTunnel *tunnel, m_tunnels.keys()) {
//                if (tunnel->remoteAddress().toString() == device->paramValue(knxLightDeviceKnxAddressParamTypeId).toString()) {
//                    QKnxAddress knxAddress = QKnxAddress(QKnxAddress::Type::Group, device->paramValue(knxLightDeviceKnxAddressParamTypeId).toString());
//                    tunnel->switchLight(knxAddress, action.param(knxLightPowerActionPowerParamTypeId).value().toBool());
//                }
//            }
        }
    }

    return DeviceManager::DeviceErrorNoError;
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
}
