#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

class ApiClient : public QObject
{
    Q_OBJECT

public:
    ApiClient(QString baseUrl, QString anonKey, QObject* parent = nullptr);

    void setAccessToken(const QString& token);
    void cancelPendingRequests();
    bool queueRetry(quint64 requestId);
    void retryQueuedRequests();
    void discardRequest(quint64 requestId);

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
    void updateDeviceName(const QString& id, const QString& name);
    void deleteDevice(const QString& id);

    void geocode(const QString& city);
    void forecast(double latitude, double longitude);

signals:
    void requestStarted(const QString& op, quint64 requestId);
    void authenticationRequired(const QString& op, quint64 requestId);
    void completed(const QString& op, quint64 requestId, int status, const QByteArray& body);
    void failed(const QString& op, quint64 requestId, int status, const QString& message);

private:
    struct PendingRequest
    {
        QString op;
        QByteArray method;
        QUrl url;
        QByteArray body;
        bool authorize;
        bool represent;
        bool includeKey;
        int retries = 0;
    };

    QNetworkRequest makeRequest(const QUrl& url, bool authorize, bool represent, bool includeKey) const;
    void send(const QString& op, const QByteArray& method, const QUrl& url, const QByteArray& body, bool authorize, bool represent, bool includeKey);
    void sendPendingRequest(quint64 requestId);
    bool guardConfig(const QString& op);
    bool guardUuid(const QString& op, const QString& id);

    QNetworkAccessManager* network_ = nullptr;
    QString baseUrl_;
    QString anonKey_;
    QString accessToken_;
    QHash<quint64, PendingRequest> pendingRequests_;
    QList<quint64> retryQueue_;
    quint64 nextRequestId_ = 0;
    quint64 requestGeneration_ = 0;
};
