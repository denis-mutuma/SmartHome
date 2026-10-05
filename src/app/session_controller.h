#pragma once

#include "api_client.h"
#include "home_json.h"

#include <QDateTime>
#include <QList>
#include <QObject>
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
    Q_PROPERTY(QString firstName READ firstName NOTIFY profileChanged)
    Q_PROPERTY(QString city READ city NOTIFY profileChanged)
    Q_PROPERTY(QVariantList rooms READ rooms NOTIFY roomsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)

public:
    explicit SessionController(QObject* parent = nullptr);
    SessionController(QString baseUrl, QString anonKey, QString tokenFilePath, QObject* parent = nullptr);

    bool signedIn() const { return signedIn_; }
    QString email() const { return email_; }
    QString firstName() const { return firstName_; }
    QString city() const { return city_; }
    QVariantList rooms() const;
    QString statusMessage() const { return statusMessage_; }

    Q_INVOKABLE bool signIn(const QString& email, const QString& password);
    Q_INVOKABLE bool registerAccount(const QString& firstName, const QString& email, const QString& password);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE bool saveSettings(const QString& firstName, const QString& city);
    Q_INVOKABLE bool createRoom(const QString& name);

signals:
    void signedInChanged();
    void emailChanged();
    void profileChanged();
    void roomsChanged();
    void statusChanged();

private slots:
    void refreshIfNeeded();

private:
    void onCompleted(const QString& op, quint64 requestId, int status, const QByteArray& body);
    void onFailed(const QString& op, quint64 requestId, int status, const QString& message);
    void onAuthenticationRequired(const QString& op, quint64 requestId);
    void startRefresh();
    void applySession(const SessionTokens& session, bool isRefresh);
    void clearLocal();
    int nextRoomPosition() const;
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
    QList<RoomRow> roomRows_;
    QString statusMessage_;
    bool signedIn_ = false;
    bool refreshInFlight_ = false;
    QTimer refreshTimer_;
};
