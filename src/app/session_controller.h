#pragma once

#include "api_client.h"
#include "home_json.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class SessionController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString email READ email NOTIFY emailChanged)
    Q_PROPERTY(QString greeting READ greeting NOTIFY profileChanged)
    Q_PROPERTY(QString firstName READ firstName NOTIFY profileChanged)
    Q_PROPERTY(QString city READ city NOTIFY profileChanged)
    Q_PROPERTY(QString weatherLine READ weatherLine NOTIFY weatherChanged)
    Q_PROPERTY(QString weatherIcon READ weatherIcon NOTIFY weatherChanged)
    Q_PROPERTY(QVariantList rooms READ rooms NOTIFY roomsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)

public:
    static constexpr int ActiveRefreshIntervalMs = 20000;

    explicit SessionController(QObject* parent = nullptr);
    SessionController(QString baseUrl, QString anonKey, QString tokenFilePath, QObject* parent = nullptr);
    SessionController(QString baseUrl, QString anonKey, QString tokenFilePath,
        ApiClient::WeatherEndpoints weatherEndpoints, QObject* parent = nullptr);
    SessionController(QString baseUrl, QString anonKey, QString tokenFilePath,
        ApiClient::WeatherEndpoints weatherEndpoints, int activeRefreshIntervalMs, QObject* parent = nullptr);

    bool signedIn() const { return signedIn_; }
    QString email() const { return email_; }
    QString greeting() const;
    QString firstName() const { return firstName_; }
    QString city() const { return city_; }
    QString weatherLine() const { return weatherLine_; }
    QString weatherIcon() const;
    QVariantList rooms() const;
    QString statusMessage() const { return statusMessage_; }

    Q_INVOKABLE bool signIn(const QString& email, const QString& password);
    Q_INVOKABLE bool registerAccount(const QString& firstName, const QString& email, const QString& password);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void reload();
    Q_INVOKABLE bool saveSettings(const QString& firstName, const QString& city);
    Q_INVOKABLE bool createRoom(const QString& name);
    Q_INVOKABLE bool renameRoom(const QString& roomId, const QString& name);
    Q_INVOKABLE bool deleteRoom(const QString& roomId);
    Q_INVOKABLE bool createDevice(const QString& roomId, const QString& name, const QString& kind);
    Q_INVOKABLE bool setDeviceOn(const QString& deviceId, bool on);
    Q_INVOKABLE bool renameDevice(const QString& deviceId, const QString& name);
    Q_INVOKABLE bool deleteDevice(const QString& deviceId);

signals:
    void signedInChanged();
    void emailChanged();
    void profileChanged();
    void weatherChanged();
    void roomsChanged();
    void statusChanged();
    void roomCreated(const QString& name);
    void roomCreateFailed(const QString& name);
    void deviceCreated(const QString& roomId, const QString& name);
    void deviceCreateFailed(const QString& roomId, const QString& name);

private slots:
    void refreshIfNeeded();
    void updatePolling(Qt::ApplicationState applicationState);

private:
    void onCompleted(const QString& op, quint64 requestId, int status, const QByteArray& body);
    void onFailed(const QString& op, quint64 requestId, int status, const QString& message);
    bool isCurrentResponse(const QString& op, quint64 requestId) const;
    void finishTrackedResponse(quint64 requestId);
    void onAuthenticationRequired(const QString& op, quint64 requestId);
    void startRefresh();
    void applySession(const SessionTokens& session, bool isRefresh);
    void clearLocal();
    void walkStaleReadings();
    void updateWeather();
    int nextRoomPosition() const;
    int nextDevicePosition(const QString& roomId) const;
    void clearPendingDeviceToggle(const QString& deviceId, bool restore);
    void setStatus(const QString& message);

    ApiClient api_;
    QString tokenFilePath_;
    QString accessToken_;
    QString refreshToken_;
    QDateTime accessTokenExpiresAt_;
    QString email_;
    QString userId_;
    QString firstName_;
    QString city_;
    QString weatherLine_;
    QString weatherIconFile_;
    QString weatherLocationCity_;
    QString pendingGeocodingCity_;
    std::optional<GeoHit> weatherLocation_;
    QList<RoomRow> roomRows_;
    QHash<QString, bool> pendingDeviceOnStates_;
    QString pendingReadingId_;
    QString pendingRoomCreateName_;
    QString pendingDeviceCreateRoomId_;
    QString pendingDeviceCreateName_;
    QString statusMessage_;
    bool signedIn_ = false;
    bool refreshInFlight_ = false;
    bool refreshRetryPending_ = false;
    QTimer refreshTimer_;
    QTimer activeRefreshTimer_;
    quint64 latestProfileRequestId_ = 0;
    quint64 latestProfileUpdateRequestId_ = 0;
    quint64 latestRoomsRequestId_ = 0;
    quint64 latestWeatherRequestId_ = 0;
    quint64 profileMutationGeneration_ = 0;
    quint64 homeMutationGeneration_ = 0;
    QHash<quint64, quint64> profileRequestGenerations_;
    QHash<quint64, bool> profileRequestsDuringMutation_;
    QSet<quint64> pendingProfileMutations_;
    QHash<quint64, quint64> roomRequestGenerations_;
    QHash<quint64, bool> roomRequestsDuringMutation_;
    QSet<quint64> pendingHomeMutations_;
    QHash<quint64, QString> entityKeysByRequestId_;
    QHash<QString, quint64> latestEntityRequestIds_;
    QSet<QString> pendingEntityMutations_;
};
