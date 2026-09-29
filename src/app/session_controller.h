#pragma once

#include "api_client.h"
#include "home_json.h"

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class SessionController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString firstName READ firstName NOTIFY profileChanged)
    Q_PROPERTY(QString city READ city NOTIFY profileChanged)
    Q_PROPERTY(QString greeting READ greeting NOTIFY greetingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString weatherLine READ weatherLine NOTIFY weatherChanged)
    Q_PROPERTY(QString weatherIcon READ weatherIcon NOTIFY weatherChanged)
    Q_PROPERTY(QVariantList rooms READ rooms NOTIFY roomsChanged)
    Q_PROPERTY(bool applicationActive READ applicationActive NOTIFY applicationActiveChanged)

public:
    explicit SessionController(QObject* parent = nullptr);

    bool signedIn() const { return signedIn_; }
    QString firstName() const { return firstName_; }
    QString city() const { return city_; }
    QString greeting() const;
    QString statusMessage() const { return statusMessage_; }
    QString weatherLine() const { return weatherLine_; }
    QString weatherIcon() const;
    QVariantList rooms() const;
    bool applicationActive() const;

    Q_INVOKABLE bool signIn(const QString& email, const QString& password);
    Q_INVOKABLE bool registerAccount(const QString& firstName, const QString& email, const QString& password);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void reload();
    Q_INVOKABLE bool createRoom(const QString& name);
    Q_INVOKABLE bool renameRoom(const QString& id, const QString& name);
    Q_INVOKABLE void deleteRoom(const QString& id);
    Q_INVOKABLE bool createDevice(const QString& roomId, const QString& name, const QString& kind);
    Q_INVOKABLE bool renameDevice(const QString& id, const QString& name);
    Q_INVOKABLE void setDeviceOn(const QString& id, bool on);
    Q_INVOKABLE void deleteDevice(const QString& id);
    Q_INVOKABLE bool saveSettings(const QString& firstName, const QString& city);

signals:
    void signedInChanged();
    void profileChanged();
    void greetingChanged();
    void statusChanged();
    void weatherChanged();
    void roomsChanged();
    void applicationActiveChanged();

private:
    void onCompleted(const QString& op, int status, const QByteArray& body);
    void onFailed(const QString& op, int status, const QString& message);
    void applySession(const SessionTokens& session);
    void applyProfile(const ProfileRow& profile);
    void authed(const std::function<void()>& call);
    void startRefresh();
    void clearLocal();
    void setStatus(const QString& message);
    void walkStaleReadings();
    bool requireFields(const QString& name, const QString& email, const QString& password, bool withName);
    int nextRoomPosition() const;
    int nextDevicePosition(const QString& roomId) const;

    ApiClient api_;
    QString accessToken_;
    QString refreshToken_;
    QString userId_;
    QString firstName_;
    QString city_;
    QString statusMessage_;
    QString weatherLine_;
    QString weatherIconFile_;
    QList<RoomRow> rooms_;
    QList<std::function<void()>> afterRefresh_;
    std::function<void()> lastCall_;
    QSet<QString> pendingReadings_;
    QString toggleRestoreId_;
    std::optional<bool> toggleRestoreOn_;
    bool signedIn_ = false;
    bool refreshRunning_ = false;
    bool didRetry_ = false;
};
