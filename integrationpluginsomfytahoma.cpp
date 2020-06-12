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

#include "integrationpluginsomfytahoma.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "network/networkaccessmanager.h"

#include "plugininfo.h"
#include "somfytahomarequests.h"

void IntegrationPluginSomfyTahoma::startPairing(ThingPairingInfo *info)
{
    info->finish(Thing::ThingErrorNoError, QT_TR_NOOP("Please enter the login credentials for Somfy Tahoma."));
}

void IntegrationPluginSomfyTahoma::confirmPairing(ThingPairingInfo *info, const QString &username, const QString &password)
{
    SomfyTahomaLoginRequest *request = new SomfyTahomaLoginRequest(hardwareManager()->networkManager(), username, password, this);
    connect(request, &SomfyTahomaLoginRequest::error, info, [info](){
        info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Failed to login to Somfy Tahoma."));
    });
    connect(request, &SomfyTahomaLoginRequest::finished, info, [this, info, username, password](const QVariant &/*result*/){
        pluginStorage()->beginGroup(info->thingId().toString());
        pluginStorage()->setValue("username", username);
        pluginStorage()->setValue("password", password);
        pluginStorage()->endGroup();
        info->finish(Thing::ThingErrorNoError);
    });
}

void IntegrationPluginSomfyTahoma::setupThing(ThingSetupInfo *info)
{
    if (info->thing()->thingClassId() == tahomaThingClassId) {
        SomfyTahomaLoginRequest *request = createLoginRequestWithStoredCredentials(info->thing());
        connect(request, &SomfyTahomaLoginRequest::error, info, [info](){
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Failed to login to Somfy Tahoma."));
        });
        connect(request, &SomfyTahomaLoginRequest::finished, info, [this, info](const QVariant &/*result*/){
            QUuid accountId = info->thing()->id();
            SomfyTahomaGetRequest *request = new SomfyTahomaGetRequest(hardwareManager()->networkManager(), "/setup", this);
            connect(request, &SomfyTahomaGetRequest::finished, this, [this, accountId](const QVariant &result){
                QList<ThingDescriptor> unknownDevices;
                QMap<QString, QUuid> gatewayToThingId;
                foreach (const QVariant &gatewayVariant, result.toMap()["gateways"].toList()) {
                    QVariantMap gatewayMap = gatewayVariant.toMap();
                    QString gatewayId = gatewayMap.value("gatewayId").toString();
                    Thing *thing = myThings().findByParams(ParamList() << Param(gatewayThingGatewayIdParamTypeId, gatewayId));
                    if (thing) {
                        qCDebug(dcSomfyTahoma()) << "Found existing gateway:" << gatewayId;
                        gatewayToThingId[gatewayId] = thing->id();
                    } else {
                        qCInfo(dcSomfyTahoma()) << "Found new gateway:" << gatewayId;
                        ThingDescriptor descriptor(gatewayThingClassId, "TaHoma Gateway", QString(), accountId);
                        descriptor.setParams(ParamList() << Param(gatewayThingGatewayIdParamTypeId, gatewayId));
                        unknownDevices.append(descriptor);
                        gatewayToThingId[gatewayId] = descriptor.thingId();
                    }
                }
                foreach (const QVariant &deviceVariant, result.toMap()["devices"].toList()) {
                    QVariantMap deviceMap = deviceVariant.toMap();
                    QString type = deviceMap.value("uiClass").toString();
                    QUrl deviceUrl = QUrl(deviceMap.value("deviceURL").toString());
                    QString label = deviceMap.value("label").toString();
                    if (type == QStringLiteral("RollerShutter")) {
                        Thing *thing = myThings().findByParams(ParamList() << Param(rollershutterThingDeviceUrlParamTypeId, deviceUrl));
                        if (thing) {
                            qCDebug(dcSomfyTahoma()) << "Found existing roller shutter:" << label << deviceUrl;
                        } else {
                            qCInfo(dcSomfyTahoma()) << "Found new roller shutter:" << label << deviceUrl;
                            ThingDescriptor descriptor(rollershutterThingClassId, label, QString(), gatewayToThingId[deviceUrl.host()]);
                            descriptor.setParams(ParamList() << Param(rollershutterThingDeviceUrlParamTypeId, deviceUrl));
                            unknownDevices.append(descriptor);
                        }
                    } else if (type == QStringLiteral("ExteriorVenetianBlind")) {
                            Thing *thing = myThings().findByParams(ParamList() << Param(venetianblindThingDeviceUrlParamTypeId, deviceUrl));
                            if (thing) {
                                qCDebug(dcSomfyTahoma()) << "Found existing venetian blind:" << label << deviceUrl;
                            } else {
                                qCInfo(dcSomfyTahoma()) << "Found new venetian blind:" << label << deviceUrl;
                                ThingDescriptor descriptor(venetianblindThingClassId, label, QString(), gatewayToThingId[deviceUrl.host()]);
                                descriptor.setParams(ParamList() << Param(venetianblindThingDeviceUrlParamTypeId, deviceUrl));
                                unknownDevices.append(descriptor);
                            }

                    } else {
                        qCDebug(dcSomfyTahoma()) << "Found unsupperted Somfy device:" << label << type << deviceUrl;
                    }
                }
                if (!unknownDevices.isEmpty()) {
                    emit autoThingsAppeared(unknownDevices);
                }
            });
            info->finish(Thing::ThingErrorNoError);
        });
    }

    else if (info->thing()->thingClassId() == gatewayThingClassId ||
             info->thing()->thingClassId() == rollershutterThingClassId ||
             info->thing()->thingClassId() == venetianblindThingClassId) {
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginSomfyTahoma::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == tahomaThingClassId) {
        SomfyTahomaGetRequest *setupRequest = new SomfyTahomaGetRequest(hardwareManager()->networkManager(), "/setup", this);
        connect(setupRequest, &SomfyTahomaGetRequest::finished, this, [this, thing](const QVariant &result){
            foreach (const QVariant &gatewayVariant, result.toMap()["gateways"].toList()) {
                QVariantMap gatewayMap = gatewayVariant.toMap();
                QString gatewayId = gatewayMap.value("gatewayId").toString();
                Thing *thing = myThings().findByParams(ParamList() << Param(gatewayThingGatewayIdParamTypeId, gatewayId));
                if (thing) {
                    qCDebug(dcSomfyTahoma()) << "Setting initial state for gateway:" << gatewayId;
                    thing->setStateValue(gatewayConnectedStateTypeId, gatewayMap["connectivity"].toMap()["status"] == "OK");
                }
            }
            foreach (const QVariant &deviceVariant, result.toMap()["devices"].toList()) {
                updateThingStates(deviceVariant.toMap()["deviceURL"].toString(), deviceVariant.toMap()["states"].toList());
            }
        });

        SomfyTahomaPostRequest *eventRegistrationRequest = new SomfyTahomaPostRequest(hardwareManager()->networkManager(), "/events/register", "application/json", QByteArray(), this);
        connect(eventRegistrationRequest, &SomfyTahomaPostRequest::error, this, [](){
            qCWarning(dcSomfyTahoma()) << "Failed to register for events.";
        });
        connect(eventRegistrationRequest, &SomfyTahomaPostRequest::finished, this, [this](const QVariant &result){
            QString eventListenerId = result.toMap()["id"].toString();

            m_eventPollTimer = hardwareManager()->pluginTimerManager()->registerTimer(2);
            connect(m_eventPollTimer, &PluginTimer::timeout, this, [this, eventListenerId](){
                SomfyTahomaEventFetchRequest *eventFetchRequest = new SomfyTahomaEventFetchRequest(hardwareManager()->networkManager(), eventListenerId, this);
                connect(eventFetchRequest, &SomfyTahomaEventFetchRequest::error, this, [this](){
                    qCWarning(dcSomfyTahoma()) << "Failed to fetch events. Stopping timer.";

                    // TODO: Implement better error handling. For now stop the timer to avoid flooding the web service unnecessarily.
                    m_eventPollTimer->stop();
                });
                connect(eventFetchRequest, &SomfyTahomaPostRequest::finished, this, [this](const QVariant &result){
                    if (!result.toList().empty()) {
                        qCDebug(dcSomfyTahoma()) << "Got events:" << result;
                    }

                    Thing *thing;
                    foreach (const QVariant &eventVariant, result.toList()) {
                        QVariantMap eventMap = eventVariant.toMap();
                        if (eventMap["name"] == "DeviceStateChangedEvent") {
                            updateThingStates(eventMap["deviceURL"].toString(), eventMap["deviceStates"].toList());
                        } else if (eventMap["name"] == "ExecutionRegisteredEvent") {
                            QList<Thing *> things;
                            foreach (const QVariant &action, eventMap["actions"].toList()) {
                                thing = myThings().findByParams(ParamList() << Param(rollershutterThingDeviceUrlParamTypeId, action.toMap()["deviceURL"]));
                                if (thing) {
                                    qCInfo(dcSomfyTahoma()) << "Roller shutter execution registered. Setting moving state.";
                                    thing->setStateValue(rollershutterMovingStateTypeId, true);
                                    things.append(thing);
                                    continue;
                                }
                                thing = myThings().findByParams(ParamList() << Param(venetianblindThingDeviceUrlParamTypeId, action.toMap()["deviceURL"]));
                                if (thing) {
                                    qCInfo(dcSomfyTahoma()) << "Venetian blind execution registered. Setting moving state.";
                                    thing->setStateValue(venetianblindMovingStateTypeId, true);
                                    things.append(thing);
                                }
                            }
                            qCInfo(dcSomfyTahoma()) << "ExecutionRegisteredEvent" << eventMap["execId"];
                            m_currentExecutions.insert(eventMap["execId"].toString(), things);
                        } else if (eventMap["name"] == "ExecutionStateChangedEvent" &&
                                   (eventMap["newState"] == "COMPLETED" || eventMap["newState"] == "FAILED")) {
                            QList<Thing *> things = m_currentExecutions.take(eventMap["execId"].toString());
                            foreach (Thing *thing, things) {
                                if (thing->thingClassId() == rollershutterThingClassId) {
                                    qCInfo(dcSomfyTahoma()) << "Roller shutter execution finished. Clearing moving state.";
                                    thing->setStateValue(rollershutterMovingStateTypeId, false);
                                } else if (thing->thingClassId() == venetianblindThingClassId) {
                                    qCInfo(dcSomfyTahoma()) << "Venetian blind execution finished. Clearing moving state.";
                                    thing->setStateValue(venetianblindMovingStateTypeId, false);
                                }
                            }

                            QPointer<ThingActionInfo> thingActionInfo = m_pendingActions.take(eventMap["execId"].toString());
                            if (!thingActionInfo.isNull()) {
                                if (eventMap["newState"] == "COMPLETED") {
                                    qCInfo(dcSomfyTahoma()) << "Action finished" << thingActionInfo->thing() << thingActionInfo->action().actionTypeId();
                                    thingActionInfo->finish(Thing::ThingErrorNoError);
                                } else if (eventMap["newState"] == "FAILED") {
                                    qCInfo(dcSomfyTahoma()) << "Action failed" << thingActionInfo->thing() << thingActionInfo->action().actionTypeId();
                                    thingActionInfo->finish(Thing::ThingErrorHardwareFailure);
                                } else {
                                    qCWarning(dcSomfyTahoma()) << "Action in unknown state" << thingActionInfo->thing() << thingActionInfo->action().actionTypeId() << eventMap["newState"].toString();
                                    thingActionInfo->finish(Thing::ThingErrorHardwareFailure);
                                }
                            }
                        } else if (eventMap["name"] == "GatewayAliveEvent") {
                            thing = myThings().findByParams(ParamList() << Param(gatewayThingGatewayIdParamTypeId, eventMap["gatewayId"]));
                            if (thing) {
                                qCInfo(dcSomfyTahoma()) << "Gateway connected event received:" << eventMap["gatewayId"];
                                thing->setStateValue(gatewayConnectedStateTypeId, true);
                            } else {
                                qCWarning(dcSomfyTahoma()) << "Ignoring gateway connected event for unknown gateway" << eventMap["gatewayId"];
                            }
                        } else if (eventMap["name"] == "GatewayDownEvent") {
                            thing = myThings().findByParams(ParamList() << Param(gatewayThingGatewayIdParamTypeId, eventMap["gatewayId"]));
                            if (thing) {
                                qCInfo(dcSomfyTahoma()) << "Gateway disconnected event received:" << eventMap["gatewayId"];
                                thing->setStateValue(gatewayConnectedStateTypeId, true);
                            } else {
                                qCWarning(dcSomfyTahoma()) << "Ignoring gateway disconnected event for unknown gateway" << eventMap["gatewayId"];
                            }
                        }
                    }
                });
            });
        });
    }
}

void IntegrationPluginSomfyTahoma::updateThingStates(const QString &deviceUrl, const QVariantList &stateList)
{
    Thing *thing = myThings().findByParams(ParamList() << Param(rollershutterThingDeviceUrlParamTypeId, deviceUrl));
    if (thing) {
        foreach (const QVariant &stateVariant, stateList) {
            QVariantMap stateMap = stateVariant.toMap();
            if (stateMap["name"] == "core:ClosureState") {
                thing->setStateValue(rollershutterPercentageStateTypeId, stateMap["value"]);
            } else if (stateMap["name"] == "core:StatusState") {
                thing->setStateValue(rollershutterConnectedStateTypeId, stateMap["value"] == "available");
            } else if (stateMap["name"] == "core:RSSILevelState") {
                thing->setStateValue(rollershutterSignalStrengthStateTypeId, stateMap["value"]);
            }
        }
        return;
    }
    thing = myThings().findByParams(ParamList() << Param(venetianblindThingDeviceUrlParamTypeId, deviceUrl));
    if (thing) {
        foreach (const QVariant &stateVariant, stateList) {
            QVariantMap stateMap = stateVariant.toMap();
            if (stateMap["name"] == "core:ClosureState") {
                thing->setStateValue(venetianblindPercentageStateTypeId, stateMap["value"]);
            } else if (stateMap["name"] == "core:SlateOrientationState") {
                // Convert percentage (0%/100%, 50%=open) into degree (-90/+90)
                int degree = (stateMap["value"].toInt() * 1.8) - 90;
                thing->setStateValue(venetianblindAngleStateTypeId, degree);
            } else if (stateMap["name"] == "core:StatusState") {
                thing->setStateValue(venetianblindConnectedStateTypeId, stateMap["value"] == "available");
            } else if (stateMap["name"] == "core:RSSILevelState") {
                thing->setStateValue(venetianblindSignalStrengthStateTypeId, stateMap["value"]);
            }
        }
        return;
    }
}

void IntegrationPluginSomfyTahoma::executeAction(ThingActionInfo *info)
{
    qCInfo(dcSomfyTahoma()) << "Action request:" << info->thing() << info->action().actionTypeId() << info->action().params();

    QString deviceUrl;
    QString actionName;
    QJsonArray actionParameters;

    if (info->thing()->thingClassId() == rollershutterThingClassId) {
        deviceUrl = info->thing()->paramValue(rollershutterThingDeviceUrlParamTypeId).toString();
        if (info->action().actionTypeId() == rollershutterPercentageActionTypeId) {
            actionName = "setClosureAndLinearSpeed";
            actionParameters = { info->action().param(rollershutterPercentageActionPercentageParamTypeId).value().toInt(), "lowspeed" };
        } else if (info->action().actionTypeId() == rollershutterOpenActionTypeId) {
            actionName = "setClosureAndLinearSpeed";
            actionParameters = { 0, "lowspeed" };
        } else if (info->action().actionTypeId() == rollershutterCloseActionTypeId) {
            actionName = "setClosureAndLinearSpeed";
            actionParameters = { 100, "lowspeed" };
        } else if (info->action().actionTypeId() == rollershutterStopActionTypeId) {
            actionName = "stop";
        }
    } else if (info->thing()->thingClassId() == venetianblindThingClassId) {
        deviceUrl = info->thing()->paramValue(venetianblindThingDeviceUrlParamTypeId).toString();
        if (info->action().actionTypeId() == venetianblindPercentageActionTypeId) {
            actionName = "setClosure";
            actionParameters = { info->action().param(venetianblindPercentageActionPercentageParamTypeId).value().toInt() };
        } else if (info->action().actionTypeId() == venetianblindAngleActionTypeId) {
            actionName = "setOrientation";
            // Convert degree (-90/+90) into percentage (0%/100%, 50%=open)
            int degree = (info->action().param(venetianblindAngleActionAngleParamTypeId).value().toInt() + 90) / 1.8;
            actionParameters = { degree };
        } else if (info->action().actionTypeId() == venetianblindOpenActionTypeId) {
            actionName = "open";
        } else if (info->action().actionTypeId() == venetianblindCloseActionTypeId) {
            actionName = "close";
        } else if (info->action().actionTypeId() == venetianblindStopActionTypeId) {
            actionName = "stop";
        }
    }

    if (!actionName.isEmpty()) {
        QJsonDocument jsonRequest{QJsonObject
        {
            {"label", "test command"},
            {"actions", QJsonArray{QJsonObject{{"deviceURL", deviceUrl},
                                               {"commands", QJsonArray{QJsonObject{{"name", actionName},
                                                                                   {"parameters", actionParameters}}}}}}}
        }};
        SomfyTahomaPostRequest *request = new SomfyTahomaPostRequest(hardwareManager()->networkManager(), "/exec/apply", "application/json", jsonRequest.toJson(QJsonDocument::Compact), this);
        connect(request, &SomfyTahomaPostRequest::error, info, [info](){
            info->finish(Thing::ThingErrorHardwareFailure);
        });
        connect(request, &SomfyTahomaPostRequest::finished, info, [this, info](const QVariant &result){
            qCInfo(dcSomfyTahoma()) << "Action started" << info->thing() << info->action().actionTypeId();
            m_pendingActions.insert(result.toMap()["execId"].toString(), info);
        });
    } else {
        info->finish(Thing::ThingErrorActionTypeNotFound);
    }
}

SomfyTahomaLoginRequest *IntegrationPluginSomfyTahoma::createLoginRequestWithStoredCredentials(Thing *thing)
{
    pluginStorage()->beginGroup(thing->id().toString());
    QString username = pluginStorage()->value("username").toString();
    QString password = pluginStorage()->value("password").toString();
    pluginStorage()->endGroup();
    return new SomfyTahomaLoginRequest(hardwareManager()->networkManager(), username, password, this);
}
