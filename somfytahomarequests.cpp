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

#include "somfytahomarequests.h"

#include <QJsonDocument>

#include "network/networkaccessmanager.h"

#include "extern-plugininfo.h"

SomfyTahomaPostRequest::SomfyTahomaPostRequest(NetworkAccessManager *networkManager, const QString &path, const QString &contentType, QByteArray &body, QObject *parent):
    QObject(parent)
{
    QUrl url("https://tahomalink.com/enduser-mobile-web/enduserAPI" + path);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, contentType);
    QNetworkReply *reply = networkManager->post(request, body);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    connect(reply, &QNetworkReply::finished, this, [this, reply, path] {
        deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(dcSomfyTahoma()) << "Request for" << path << "failed:" << reply->errorString();
            emit error(reply->error());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qCWarning(dcSomfyTahoma()) << "Json parse error in reply for" << path << ":" << parseError.errorString();
            emit error(QNetworkReply::UnknownContentError);
            return;
        }

        emit finished(jsonDoc.toVariant().toMap());
    });
}

SomfyTahomaGetRequest::SomfyTahomaGetRequest(NetworkAccessManager *networkManager, const QString &path, QObject *parent):
    QObject(parent)
{
    QUrl url("https://tahomalink.com/enduser-mobile-web/enduserAPI" + path);
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    connect(reply, &QNetworkReply::finished, this, [this, reply, path] {
        deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(dcSomfyTahoma()) << "Request for" << path << "failed:" << reply->errorString();
            emit error(reply->error());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qCWarning(dcSomfyTahoma()) << "Json parse error in reply for" << path << ":" << parseError.errorString();
            emit error(QNetworkReply::UnknownContentError);
            return;
        }

        emit finished(jsonDoc.toVariant().toMap());
    });
}
