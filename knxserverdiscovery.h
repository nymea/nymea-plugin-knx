#ifndef KNXSERVERDISCOVERY_H
#define KNXSERVERDISCOVERY_H

#include <QObject>
#include <QHostAddress>
#include <QKnxNetIpServerDiscoveryAgent>
#include <QKnxNetIpServerDescriptionAgent>

class KnxServerDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit KnxServerDiscovery(QObject *parent = nullptr);
    bool startDisovery();

private:
    int m_discoveryTimeout = 10000;
    QList<QKnxNetIpServerDiscoveryAgent *> m_runningDiscoveryAgents;
    QList<QKnxNetIpServerInfo> m_discoveredServers;

signals:
    void discoveryFinished();

private slots:
    void onDiscoveryAgentErrorOccured(QKnxNetIpServerDiscoveryAgent::Error error);
    void onDiscoveryAgentFinished();


public slots:
};

#endif // KNXSERVERDISCOVERY_H
