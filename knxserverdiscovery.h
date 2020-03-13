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

    QList<QKnxNetIpServerInfo> discoveredServers() const;

    static QString serviceFamilyToString(QKnx::NetIp::ServiceFamily id);
    static void printServerInfo(const QKnxNetIpServerInfo &serverInfo);

private:
    int m_discoveryTimeout = 5000;
    QList<QKnxNetIpServerDiscoveryAgent *> m_runningDiscoveryAgents;
    QList<QKnxNetIpServerInfo> m_discoveredServers;

signals:
    void discoveryFinished();

private slots:
    void onDiscoveryAgentErrorOccured(QKnxNetIpServerDiscoveryAgent::Error error);
    void onDiscoveryAgentFinished();


public slots:
    bool startDisovery();

};

#endif // KNXSERVERDISCOVERY_H
