#include "knxtunnel.h"

#include "extern-plugininfo.h"

#include <QNetworkInterface>

#include <QKnxTpdu>
#include <QKnx1Bit>
#include <QKnxLinkLayerFrame>
#include <QKnxLinkLayerFrameBuilder>


KnxTunnel::KnxTunnel(const QHostAddress &remoteAddress, QObject *parent) :
    QObject(parent),
    m_remoteAddress(remoteAddress)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &KnxTunnel::onTimeout);

    m_tunnel = new QKnxNetIpTunnel(this);
    m_tunnel->setLocalPort(0);
    m_tunnel->setHeartbeatTimeout(1000);

    connect(m_tunnel, &QKnxNetIpTunnel::frameReceived, this, &KnxTunnel::onTunnelFrameReceived);
    connect(m_tunnel, &QKnxNetIpTunnel::connected, this, &KnxTunnel::onTunnelConnected);
    connect(m_tunnel, &QKnxNetIpTunnel::disconnected, this, &KnxTunnel::onTunnelDisconnected);
    connect(m_tunnel, &QKnxNetIpTunnel::stateChanged, this, &KnxTunnel::onTunnelStateChanged);
}

QHostAddress KnxTunnel::remoteAddress() const
{
    return m_remoteAddress;
}

void KnxTunnel::setRemoteAddress(const QHostAddress &remoteAddress)
{
    m_remoteAddress = remoteAddress;
}

bool KnxTunnel::connected() const
{
    return m_tunnel->state() == QKnxNetIpTunnel::State::Connected;
}

bool KnxTunnel::connectTunnel()
{
    // Get the local address for this tunnel remote address
    QHostAddress localAddress = getLocalAddress(m_remoteAddress);
    if (localAddress.isNull()) {
        qCWarning(dcKnx()) << "Could connect to " << m_remoteAddress.toString() <<". There is no local interface for this server address. Make sure this device is connected to the correct network.";
        return false;
    }

    m_tunnel->setLocalAddress(localAddress);
    qCDebug(dcKnx()) << "Connecting tunnel to" << m_remoteAddress.toString() << "using" << m_tunnel->localAddress();
    m_tunnel->connectToHost(m_remoteAddress, m_port);
    return true;
}

void KnxTunnel::disconnectTunnel()
{
    qCDebug(dcKnx()) << "Disonnecting tunnel from" << m_remoteAddress.toString();
    m_tunnel->disconnectFromHost();
}

void KnxTunnel::collectAddress(const QKnxAddress &address)
{
    bool newAddress = false;
    switch (address.type()) {
    case QKnxAddress::Type::Individual:
        if (!m_knxDeviceAddresses.contains(address)) {
            m_knxDeviceAddresses.append(address);
            newAddress = true;
        }
        break;
    case QKnxAddress::Type::Group:
        if (!m_knxGroupAddresses.contains(address)) {
            m_knxGroupAddresses.append(address);
            newAddress = true;
        }
        break;
    }

    if (newAddress) {
        qCDebug(dcKnx()) << "Device addresses:";
        foreach (const QKnxAddress &address, m_knxDeviceAddresses) {
            qCDebug(dcKnx()) << "    " << address.toString();
        }

        qCDebug(dcKnx()) << "Group addresses:";
        foreach (const QKnxAddress &address, m_knxGroupAddresses) {
            qCDebug(dcKnx()) << "    " << address.toString();
        }
    }
}

void KnxTunnel::printFrame(const QKnxLinkLayerFrame &frame)
{
    qCDebug(dcKnx()) << "    Message code:" << frame.messageCode();
    qCDebug(dcKnx()) << "    MediumType" << frame.mediumType();
    qCDebug(dcKnx()) << "    Source address" << frame.sourceAddress().toString();
    qCDebug(dcKnx()) << "    Destination address" << frame.destinationAddress().toString();
    qCDebug(dcKnx()) << "    Control field:" << frame.controlField();
    qCDebug(dcKnx()) << "    Extended control field:" << frame.extendedControlField();
    qCDebug(dcKnx()) << "    Additional infos:" << frame.additionalInfos();
    qCDebug(dcKnx()) << "    Bytes:" << frame.bytes() << frame.bytes().toHex().toByteArray();
    qCDebug(dcKnx()) << "    TPDU:" << frame.tpdu();
    qCDebug(dcKnx()) << "        Transport control field:" << frame.tpdu().transportControlField();
    qCDebug(dcKnx()) << "        Application control field:" << frame.tpdu().applicationControlField();
    qCDebug(dcKnx()) << "        MediumType:" << frame.tpdu().mediumType();
    qCDebug(dcKnx()) << "        Sequence number:" << frame.tpdu().sequenceNumber();
    qCDebug(dcKnx()) << "        Size:" << frame.tpdu().size();
    qCDebug(dcKnx()) << "        Data:" << frame.tpdu().data();
}

QHostAddress KnxTunnel::getLocalAddress(const QHostAddress &remoteAddress)
{
    QHostAddress localAddress;
    foreach (const QNetworkInterface &interface, QNetworkInterface::allInterfaces()) {
        qCDebug(dcKnx()) << "Network interface" << interface.name() << interface.type();
        foreach (const QNetworkAddressEntry &addressEntry, interface.addressEntries()) {
            if (addressEntry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                qCDebug(dcKnx()) << "    - " << addressEntry.ip().toString() << addressEntry.netmask().toString();
                if (remoteAddress.isInSubnet(addressEntry.ip(), addressEntry.prefixLength())) {
                    qCDebug(dcKnx()) << "Found local interface address for" << remoteAddress.toString() << "-->" << addressEntry.ip().toString() << interface.name();
                    localAddress = addressEntry.ip();
                }
            }
        }
    }

    return localAddress;
}

void KnxTunnel::switchLight(const QKnxAddress &knxAddress, bool power)
{
    qCDebug(dcKnx()) << "Switching light" << knxAddress.toString() << (power ? "ON" : "OFF");

    QKnxTpdu tpdu;
    tpdu.setTransportControlField(QKnxTpdu::TransportControlField::DataGroup);
    tpdu.setApplicationControlField(QKnxTpdu::ApplicationControlField::GroupValueWrite);
    tpdu.setData(QKnxSwitch(power ? QKnxSwitch::State::On : QKnxSwitch::State::Off).bytes());

    QKnxLinkLayerFrame frame = QKnxLinkLayerFrame::builder()
            .setMedium(QKnx::MediumType::NetIP)
            .setDestinationAddress(knxAddress)
            .setTpdu(tpdu).createFrame();

    qCDebug(dcKnx()) << "--> Sending frame" << frame;
    printFrame(frame);
    m_tunnel->sendFrame(frame);
}

void KnxTunnel::onTimeout()
{
    qCDebug(dcKnx()) << "Tunnel reconnection timeout.";
    connectTunnel();
}

void KnxTunnel::onTunnelConnected()
{
    qCDebug(dcKnx()) << "Tunnel connected.";
    emit connectedChanged();

    // Stop the reconnect timer
    m_timer->stop();

    //readDeviceInfos(QKnxAddress(QKnxAddress::Type::Group, QLatin1String("0/0/3")));
    //switchLight(QKnxAddress(QKnxAddress::Type::Group, QLatin1String("0/0/3")), false);
}

void KnxTunnel::onTunnelDisconnected()
{
    qCDebug(dcKnx()) << "Tunnel disconnected.";
    emit connectedChanged();

    // Start the reconnect timer
    m_timer->start();
}

void KnxTunnel::onTunnelStateChanged(QKnxNetIpEndpointConnection::State state)
{
    qCDebug(dcKnx()) << "Tunnel state changed" << state;
}

void KnxTunnel::onTunnelFrameReceived(const QKnxLinkLayerFrame &frame)
{
    qCDebug(dcKnx()) << "<-- Tunnel frame received" << frame;
    printFrame(frame);

    // Store groups and devices for collecting addresses
    collectAddress(frame.sourceAddress());
    collectAddress(frame.destinationAddress());
}

