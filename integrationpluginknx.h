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

#ifndef INTEGRATIONPLUGINKNX_H
#define INTEGRATIONPLUGINKNX_H

#include "plugintimer.h"
#include "integrations/integrationplugin.h"

#include "knxtunnel.h"
#include "knxserverdiscovery.h"


class IntegrationPluginKnx: public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginknx.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginKnx();

    void init() override;
    void startMonitoringAutoThings() override;

    void discoverThings(ThingDiscoveryInfo *info) override;
    void setupThing(ThingSetupInfo *info) override;

    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

    void executeAction(ThingActionInfo *info) override;

private:
    PluginTimer *m_pluginTimer = nullptr;

    KnxServerDiscovery *m_discovery = nullptr;
    QHash<KnxTunnel *, Thing *> m_tunnels;

    KnxTunnel *getTunnelForDevice(Thing *thing);

    void autoCreateKnownDevices(Thing *parentThing);

    void createGenericDevices(Thing *parentThing);
    void destroyGenericDevices(Thing *parentThing);

private slots:
    void onPluginTimerTimeout();
    void onPluginConfigurationChanged(const ParamTypeId &paramTypeId, const QVariant &value);

    void onTunnelConnectedChanged();
    void onTunnelFrameReceived(const QKnxLinkLayerFrame &frame);

};

#endif // INTEGRATIONPLUGINKNX_H
