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

#ifndef SOMFYTAHOMAREQUESTS_H
#define SOMFYTAHOMAREQUESTS_H

#include <QNetworkReply>

class NetworkAccessManager;

class SomfyTahomaPostRequest : public QObject
{
    Q_OBJECT

public:
    SomfyTahomaPostRequest(NetworkAccessManager *networkManager, const QString &path, const QString &contentType, const QByteArray &body, QObject *parent);

signals:
    void error(QNetworkReply::NetworkError error);
    void finished(const QVariant &results);
};

class SomfyTahomaGetRequest : public QObject
{
    Q_OBJECT

public:
    SomfyTahomaGetRequest(NetworkAccessManager *networkManager, const QString &path, QObject *parent);

signals:
    void error(QNetworkReply::NetworkError error);
    void finished(const QVariant &results);
};

class SomfyTahomaLoginRequest : public SomfyTahomaPostRequest
{
    Q_OBJECT

public:
    SomfyTahomaLoginRequest(NetworkAccessManager *networkManager, const QString &username, const QString &password, QObject *parent);
};

class SomfyTahomaEventFetchRequest : public SomfyTahomaPostRequest
{
    Q_OBJECT

public:
    SomfyTahomaEventFetchRequest(NetworkAccessManager *networkManager, const QString &eventListenerId, QObject *parent);
};

#endif // SOMFYTAHOMAREQUESTS_H
