#ifndef KNXTUNNEL_H
#define KNXTUNNEL_H

#include <QTimer>
#include <QObject>

#include <QHostAddress>
#include <QKnxNetIpTunnel>

class KnxTunnel : public QObject
{
    Q_OBJECT
public:
    explicit KnxTunnel(const QHostAddress &remoteAddress, QObject *parent = nullptr);

    QHostAddress remoteAddress() const;
    void setRemoteAddress(const QHostAddress &remoteAddress);

    bool connected() const;

    bool connectTunnel();
    void disconnectTunnel();

    void switchKnxDevice(const QKnxAddress &knxAddress, bool power);
    void switchKnxShutter(const QKnxAddress &knxAddress, bool power);

private:
    QHostAddress m_localAddress;
    QHostAddress m_remoteAddress;
    quint16 m_port = 3671;

    QTimer *m_timer = nullptr;
    QKnxNetIpTunnel *m_tunnel = nullptr;

    void readManufacturer(const QKnxAddress &knxAddress);

    // Helper
    void sendFrame(const QKnxLinkLayerFrame &frame);
    void collectAddress(const QKnxAddress &address);
    void printFrame(const QKnxLinkLayerFrame &frame);
    QHostAddress getLocalAddress(const QHostAddress &remoteAddress);

signals:
    void connectedChanged();
    void frameReceived(const QKnxLinkLayerFrame &frame);

private slots:
    void onTimeout();

    void onTunnelConnected();
    void onTunnelDisconnected();
    void onTunnelStateChanged(QKnxNetIpEndpointConnection::State state);
    void onTunnelFrameReceived(const QKnxLinkLayerFrame &frame);
};

#endif // KNXTUNNEL_H
