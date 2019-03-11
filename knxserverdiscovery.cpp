#include "knxserverdiscovery.h"
#include "extern-plugininfo.h"

#include <QNetworkInterface>

KnxServerDiscovery::KnxServerDiscovery(QObject *parent) :
    QObject(parent)
{

}

bool KnxServerDiscovery::startDisovery()
{
    if (!m_runningDiscoveryAgents.isEmpty()) {
        qCWarning(dcKnx()) << "Could not start discovery. There are still discovery agents running. Count" << m_runningDiscoveryAgents.count();
        return false;
    }

    qCDebug(dcKnx()) << "Start KNX server discovery on all interfaces";
    m_discoveredServers.clear();

    foreach (const QNetworkInterface &interface, QNetworkInterface::allInterfaces()) {
        qCDebug(dcKnx()) << "Checking network interface" << interface.name() << interface.type();
        foreach (const QNetworkAddressEntry &addressEntry, interface.addressEntries()) {
            // If ipv4 and not local host
            if (addressEntry.ip().protocol() == QAbstractSocket::IPv4Protocol && !addressEntry.ip().isLoopback()) {
                qCDebug(dcKnx()) << "Start discovery on" << interface.name() << addressEntry.ip().toString();

                // Create discovery agent
                QKnxNetIpServerDiscoveryAgent *discovery = new QKnxNetIpServerDiscoveryAgent(this);
                discovery->setLocalAddress(addressEntry.ip());
                discovery->setLocalPort(0);
                discovery->setSearchFrequency(60);
                discovery->setResponseType(QKnxNetIpServerDiscoveryAgent::ResponseType::Unicast);
                discovery->setDiscoveryMode(QKnxNetIpServerDiscoveryAgent::DiscoveryMode::CoreV1 | QKnxNetIpServerDiscoveryAgent::DiscoveryMode::CoreV2);

                m_runningDiscoveryAgents.append(discovery);
                connect(discovery, &QKnxNetIpServerDiscoveryAgent::finished, this, &KnxServerDiscovery::onDiscoveryAgentFinished);
                connect(discovery, &QKnxNetIpServerDiscoveryAgent::errorOccurred, this, &KnxServerDiscovery::onDiscoveryAgentErrorOccured);

                // Start the discovery
                discovery->start(m_discoveryTimeout);
            }
        }
    }

    return true;
}

void KnxServerDiscovery::onDiscoveryAgentErrorOccured(QKnxNetIpServerDiscoveryAgent::Error error)
{
    QKnxNetIpServerDiscoveryAgent *discovery = static_cast<QKnxNetIpServerDiscoveryAgent *>(sender());
    qCDebug(dcKnx()) << "Discovery error occured" << discovery->localAddress().toString() << error << discovery->errorString();
}

void KnxServerDiscovery::onDiscoveryAgentFinished()
{
    QKnxNetIpServerDiscoveryAgent *discovery = static_cast<QKnxNetIpServerDiscoveryAgent *>(sender());
    qCDebug(dcKnx()) << "Discovery agent for" << discovery->localAddress() << "has finished";
    qCDebug(dcKnx()) << "Found" << discovery->discoveredServers().count() << "servers";

    m_runningDiscoveryAgents.removeAll(discovery);
    discovery->deleteLater();

    if (m_runningDiscoveryAgents.isEmpty()) {
        emit discoveryFinished();
    }
}
