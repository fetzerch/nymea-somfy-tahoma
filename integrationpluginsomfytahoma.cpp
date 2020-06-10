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
    QByteArray body;
    body.append("userId=" + username);
    body.append("&userPassword=" + password);
    SomfyTahomaPostRequest *request = new SomfyTahomaPostRequest(hardwareManager()->networkManager(), "/login", "application/x-www-form-urlencoded", body, this);
    connect(request, &SomfyTahomaPostRequest::error, info, [info](){
        info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Failed to login to Somfy Tahoma."));
    });
    connect(request, &SomfyTahomaPostRequest::finished, info, [this, info, username, password](const QVariant &/*result*/){
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
        QByteArray body;
        pluginStorage()->beginGroup(info->thing()->id().toString());
        body.append("userId=" + pluginStorage()->value("username").toString());
        body.append("&userPassword=" + pluginStorage()->value("password").toString());
        pluginStorage()->endGroup();
        SomfyTahomaPostRequest *request = new SomfyTahomaPostRequest(hardwareManager()->networkManager(), "/login", "application/x-www-form-urlencoded", body, this);
        connect(request, &SomfyTahomaPostRequest::error, info, [info](){
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Failed to login to Somfy Tahoma."));
        });
        connect(request, &SomfyTahomaPostRequest::finished, info, [this, info](const QVariant &/*result*/){
            QUuid id = info->thing()->id();
            SomfyTahomaGetRequest *request = new SomfyTahomaGetRequest(hardwareManager()->networkManager(), "/setup", this);
            connect(request, &SomfyTahomaGetRequest::finished, this, [this, id](const QVariant &result){
                QList<ThingDescriptor> unknownDevices;
                foreach (const QVariant &deviceVariant, result.toMap()["devices"].toList()) {
                    QVariantMap deviceMap = deviceVariant.toMap();
                    QString type = deviceMap.value("uiClass").toString();
                    QString deviceUrl = deviceMap.value("deviceURL").toString();
                    QString label = deviceMap.value("label").toString();
                    if (type == QStringLiteral("RollerShutter")) {
                        Thing *thing = myThings().findByParams(ParamList() << Param(shutterThingDeviceUrlParamTypeId, deviceUrl));
                        if (thing) {
                            qCDebug(dcSomfyTahoma()) << "Found existing Somfy device:" << label << type << deviceUrl;
                        } else {
                            qCInfo(dcSomfyTahoma) << "Found new Somfy device:" << label << type << deviceUrl;
                            ThingDescriptor descriptor(shutterThingClassId, label, QString(), id);
                            descriptor.setParams(ParamList() << Param(shutterThingDeviceUrlParamTypeId, deviceUrl));
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

    else if (info->thing()->thingClassId() == shutterThingClassId) {
        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginSomfyTahoma::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == tahomaThingClassId) {
        SomfyTahomaGetRequest *setupRequest = new SomfyTahomaGetRequest(hardwareManager()->networkManager(), "/setup", this);
        connect(setupRequest, &SomfyTahomaGetRequest::finished, this, [this](const QVariant &result){
            foreach (const QVariant &deviceVariant, result.toMap()["devices"].toList()) {
                QVariantMap deviceMap = deviceVariant.toMap();
                QString type = deviceMap.value("uiClass").toString();
                QString deviceUrl = deviceMap.value("deviceURL").toString();
                QString label = deviceMap.value("label").toString();
                if (type == QStringLiteral("RollerShutter")) {
                    Thing *thing = myThings().findByParams(ParamList() << Param(shutterThingDeviceUrlParamTypeId, deviceUrl));
                    if (thing) {
                        qCDebug(dcSomfyTahoma()) << "Setting initial state existing Somfy device:" << label << deviceUrl;
                        foreach (const QVariant &stateVariant, deviceMap["states"].toList()) {
                            QVariantMap stateMap = stateVariant.toMap();
                            if (stateMap["name"] == "core:ClosureState") {
                                thing->setStateValue(shutterPercentageStateTypeId, stateMap["value"]);
                            } else if (stateMap["name"] == "core:StatusState") {
                                thing->setStateValue(shutterConnectedStateTypeId, stateMap["value"] == "available");
                            } else if (stateMap["name"] == "core:RSSILevelState") {
                                thing->setStateValue(shutterSignalStrengthStateTypeId, stateMap["value"]);
                            }
                        }
                    }
                }
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
                SomfyTahomaPostRequest *eventFetchRequest = new SomfyTahomaPostRequest(hardwareManager()->networkManager(), "/events/" + eventListenerId + "/fetch", "application/json", QByteArray(), this);
                connect(eventFetchRequest, &SomfyTahomaPostRequest::error, this, [this](){
                    qCWarning(dcSomfyTahoma()) << "Failed to fetch events. Stopping timer.";

                    // TODO: Implement better error handling. For now stop the timer to avoid flooding the web service unnecessarily.
                    m_eventPollTimer->stop();
                });
                connect(eventFetchRequest, &SomfyTahomaPostRequest::finished, this, [this](const QVariant &result){
                    if (!result.toList().empty()) {
                        qCDebug(dcSomfyTahoma()) << "Got events:" << result;
                    }

                    foreach (const QVariant &eventVariant, result.toList()) {
                        QVariantMap eventMap = eventVariant.toMap();
                        if (eventMap["name"] == "DeviceStateChangedEvent") {
                            Thing *thing = myThings().findByParams(ParamList() << Param(shutterThingDeviceUrlParamTypeId, eventMap["deviceURL"]));
                            if (thing) {
                                foreach (const QVariant &stateVariant, eventMap["deviceStates"].toList()) {
                                    QVariantMap stateMap = stateVariant.toMap();
                                    if (stateMap["name"] == "core:ClosureState") {
                                        thing->setStateValue(shutterPercentageStateTypeId, stateMap["value"]);
                                    } else if (stateMap["name"] == "core:StatusState") {
                                        thing->setStateValue(shutterConnectedStateTypeId, stateMap["value"] == "available");
                                    } else if (stateMap["name"] == "core:RSSILevelState") {
                                        thing->setStateValue(shutterSignalStrengthStateTypeId, stateMap["value"]);
                                    }
                                }
                            }
                        } else if (eventMap["name"] == "ExecutionRegisteredEvent") {
                            QList<Thing *> things;
                            foreach (const QVariant &action, eventMap["actions"].toList()) {
                                Thing *thing = myThings().findByParams(ParamList() << Param(shutterThingDeviceUrlParamTypeId, action.toMap()["deviceURL"]));
                                if (thing) {
                                    qCInfo(dcSomfyTahoma()) << "Shutter execution registered. Setting moving state.";
                                    thing->setStateValue(shutterMovingStateTypeId, true);
                                    things.append(thing);
                                }
                            }
                            qCInfo(dcSomfyTahoma()) << "ExecutionRegisteredEvent" << eventMap["execId"];
                            m_currentExecutions.insert(eventMap["execId"].toString(), things);
                        } else if (eventMap["name"] == "ExecutionStateChangedEvent" &&
                                   (eventMap["newState"] == "COMPLETED" || eventMap["newState"] == "FAILED")) {
                            QList<Thing *> things = m_currentExecutions.take(eventMap["execId"].toString());
                            foreach (Thing *thing, things) {
                                if (thing->thingClassId() == shutterThingClassId) {
                                    qCInfo(dcSomfyTahoma()) << "Shutter execution finished. Clearing moving state.";
                                    thing->setStateValue(shutterMovingStateTypeId, false);
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
                        }
                    }
                });
            });
        });
    }
}

void IntegrationPluginSomfyTahoma::executeAction(ThingActionInfo *info)
{
    qCInfo(dcSomfyTahoma()) << "Action request:" << info->thing() << info->action().actionTypeId() << info->action().params();

    QString actionName;
    QJsonArray actionParameters;

    if (info->thing()->thingClassId() == shutterThingClassId) {
        if (info->action().actionTypeId() == shutterPercentageActionTypeId) {
            actionName = "setClosureAndLinearSpeed";
            actionParameters = { info->action().param(shutterPercentageActionPercentageParamTypeId).value().toInt(), "lowspeed" };
        } else if (info->action().actionTypeId() == shutterOpenActionTypeId) {
            actionName = "setClosureAndLinearSpeed";
            actionParameters = { 0, "lowspeed" };
        } else if (info->action().actionTypeId() == shutterCloseActionTypeId) {
            actionName = "setClosureAndLinearSpeed";
            actionParameters = { 100, "lowspeed" };
        } else if (info->action().actionTypeId() == shutterStopActionTypeId) {
            actionName = "stop";
        } else {
            /* Intentially left blank */
        }
    }

    if (!actionName.isEmpty()) {
        QJsonDocument jsonRequest{QJsonObject
        {
            {"label", "test command"},
            {"actions", QJsonArray{QJsonObject{{"deviceURL", info->thing()->paramValue(shutterThingDeviceUrlParamTypeId).toString()},
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
