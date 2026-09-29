#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QNetworkRequest>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class ApiClient : public QObject
{
    Q_OBJECT

public:
    ApiClient(QString baseUrl, QString anonKey, QObject* parent = nullptr);

    void setAccessToken(const QString& token);

    void signUp(const QString& email, const QString& password, const QString& firstName);
    void signIn(const QString& email, const QString& password);
    void refresh(const QString& refreshToken);
    void logOut();

    void fetchProfile();
    void updateProfile(const QString& userId, const QString& firstName, const QString& city);
    void fetchRooms();
    void insertRoom(const QString& name, int position);
    void updateRoom(const QString& id, const QString& name);
    void deleteRoom(const QString& id);
    void insertDevice(const QString& roomId, const QString& name, const QString& kind, int position);
    void setDeviceOn(const QString& id, bool on);
    void setReading(const QString& id, double celsius, const QDateTime& readingAt);
    void updateDeviceName(const QString& id, const QString& name);
    void deleteDevice(const QString& id);

    void geocode(const QString& city);
    void forecast(double latitude, double longitude);

signals:
    void completed(const QString& op, int status, const QByteArray& body);
    void failed(const QString& op, int status, const QString& message);

private:
    QNetworkRequest makeRequest(const QUrl& url, bool authorize, bool represent, bool includeKey) const;
    void send(const QString& op, const QByteArray& method, const QUrl& url, const QByteArray& body, bool authorize, bool represent, bool includeKey);
    bool guardConfig(const QString& op);
    bool guardUuid(const QString& op, const QString& id);

    QNetworkAccessManager* network_ = nullptr;
    QString baseUrl_;
    QString anonKey_;
    QString accessToken_;
};
