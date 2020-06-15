/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2020 Christian Fetzer <fetzer.ch@gmail.com>              *
 *                                                                         *
 * This project may be redistributed and/or modified under the terms of    *
 * the GNU Lesser General Public License as published by the Free Software *
 * Foundation; version 3. This project is distributed in the hope that     *
 * it will be useful, but WITHOUT ANY WARRANTY; without even the implied   *
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.        *
 * See the GNU Lesser General Public License for more details.             *
 *                                                                         *
 * You should have received a copy of the GNU Lesser General Public        *
 * License along with this project.                                        *
 * If not, see <https://www.gnu.org/licenses/>.                            *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINSOMFYTAHOMA_H
#define INTEGRATIONPLUGINSOMFYTAHOMA_H

#include "integrations/integrationplugin.h"
#include "plugintimer.h"

class SomfyTahomaLoginRequest;

class IntegrationPluginSomfyTahoma : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginsomfytahoma.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    void startPairing(ThingPairingInfo *info) override;
    void confirmPairing(ThingPairingInfo *info, const QString &username, const QString &password) override;

    void setupThing(ThingSetupInfo *info) override;
    void postSetupThing(Thing *thing) override;
    void thingRemoved(Thing *thing) override;

    void executeAction(ThingActionInfo *info) override;

private:
    SomfyTahomaLoginRequest *createLoginRequestWithStoredCredentials(Thing *thing);
    void refreshAccount(Thing *thing);
    void handleEvents(const QVariantList &eventList);
    void updateThingStates(const QString &deviceUrl, const QVariantList &stateList);
    void markDisconnected(Thing *thing);
    void restoreChildConnectedState(Thing *thing);

private:
    QMap<Thing *, PluginTimer *> m_eventPollTimer;
    QMap<QString, QPointer<ThingActionInfo>> m_pendingActions;
    QMap<QString, QList<Thing *>> m_currentExecutions;
};

#endif // INTEGRATIONPLUGINSOMFYTAHOMA_H
