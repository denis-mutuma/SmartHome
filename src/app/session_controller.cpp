#include "session_controller.h"

#include "home_json.h"
#include "home_rules.h"
#include "token_store.h"

#include <utility>

SessionController::SessionController(QObject* parent)
    : SessionController(QStringLiteral(SMARTHOME_SUPABASE_URL), QStringLiteral(SMARTHOME_SUPABASE_ANON_KEY),
          sessionFilePath(), parent)
{
}

SessionController::SessionController(QString baseUrl, QString anonKey, QString tokenFilePath, QObject* parent)
    : QObject(parent)
    , api_(std::move(baseUrl), std::move(anonKey))
    , tokenFilePath_(std::move(tokenFilePath))
{
    connect(&api_, &ApiClient::completed, this, &SessionController::onCompleted);
    connect(&api_, &ApiClient::failed, this, &SessionController::onFailed);
    connect(&api_, &ApiClient::authenticationRequired, this, &SessionController::onAuthenticationRequired);
    connect(&refreshTimer_, &QTimer::timeout, this, &SessionController::refreshIfNeeded);
    refreshTimer_.setInterval(15000);
    refreshTimer_.start();
    refreshToken_ = loadRefreshToken(tokenFilePath_);
    if (!refreshToken_.isEmpty()) {
        startRefresh();
    }
}

bool SessionController::signIn(const QString& email, const QString& password)
{
    const QString normalizedEmail = email.trimmed().toLower();
    if (!isEmailOk(normalizedEmail)) {
        setStatus(tr("Enter a valid email."));
        return false;
    }
    if (!isPasswordOk(password)) {
        setStatus(tr("Use 8 to 72 characters."));
        return false;
    }
    setStatus({});
    api_.signIn(normalizedEmail, password);
    return true;
}

bool SessionController::registerAccount(const QString& firstName, const QString& email, const QString& password)
{
    const QString normalizedName = firstName.trimmed();
    const QString normalizedEmail = email.trimmed().toLower();
    if (!isNameOk(normalizedName)) {
        setStatus(tr("Use 1 to 40 characters for the name."));
        return false;
    }
    if (!isEmailOk(normalizedEmail)) {
        setStatus(tr("Enter a valid email."));
        return false;
    }
    if (!isPasswordOk(password)) {
        setStatus(tr("Use 8 to 72 characters."));
        return false;
    }
    setStatus({});
    api_.signUp(normalizedEmail, password, normalizedName);
    return true;
}

void SessionController::signOut()
{
    api_.cancelPendingRequests();
    refreshInFlight_ = false;
    if (!accessToken_.isEmpty()) {
        api_.logOut();
    }
    const bool tokenCleared = clearRefreshToken(tokenFilePath_);
    clearLocal();
    setStatus(tokenCleared ? QString() : tr("Could not clear the session."));
}

void SessionController::onCompleted(const QString& op, quint64, int, const QByteArray& body)
{
    if (op != QLatin1String("login") && op != QLatin1String("signup") && op != QLatin1String("refresh")) {
        return;
    }
    const bool isRefresh = op == QLatin1String("refresh");
    const std::optional<SessionTokens> session = parseSession(body);
    if (!session.has_value()) {
        if (isRefresh) {
            refreshInFlight_ = false;
            clearRefreshToken(tokenFilePath_);
            clearLocal();
        }
        setStatus(tr("The service could not complete the request."));
        return;
    }
    if (isRefresh) {
        refreshInFlight_ = false;
    }
    applySession(*session, isRefresh);
}

void SessionController::onFailed(const QString& op, quint64, int, const QString& message)
{
    if (op == QLatin1String("refresh")) {
        refreshInFlight_ = false;
        clearRefreshToken(tokenFilePath_);
        clearLocal();
        setStatus(message);
    } else if (op == QLatin1String("login") || op == QLatin1String("signup")) {
        setStatus(message);
    }
}

void SessionController::onAuthenticationRequired(const QString&, quint64)
{
    startRefresh();
}

void SessionController::startRefresh()
{
    if (refreshInFlight_) {
        return;
    }
    if (refreshToken_.isEmpty()) {
        api_.cancelPendingRequests();
        clearLocal();
        setStatus(tr("The service could not complete the request."));
        return;
    }
    refreshInFlight_ = true;
    api_.refresh(refreshToken_);
}

void SessionController::refreshIfNeeded()
{
    if (signedIn_ && accessTokenExpiresAt_.isValid()
        && QDateTime::currentDateTimeUtc().addSecs(60) >= accessTokenExpiresAt_) {
        startRefresh();
    }
}

void SessionController::applySession(const SessionTokens& session, bool isRefresh)
{
    if (!saveRefreshToken(session.refreshToken, tokenFilePath_)) {
        if (isRefresh) {
            clearRefreshToken(tokenFilePath_);
            clearLocal();
        }
        setStatus(tr("Could not save the session."));
        return;
    }
    const bool wasSignedIn = signedIn_;
    accessToken_ = session.accessToken;
    refreshToken_ = session.refreshToken;
    accessTokenExpiresAt_ = jwtExpiryUtc(accessToken_);
    if (!accessTokenExpiresAt_.isValid() && session.expiresIn > 0) {
        accessTokenExpiresAt_ = QDateTime::currentDateTimeUtc().addSecs(session.expiresIn);
    }
    email_ = session.email;
    api_.setAccessToken(accessToken_);
    signedIn_ = true;
    if (!wasSignedIn) {
        emit signedInChanged();
    }
    emit emailChanged();
    setStatus({});
    if (isRefresh) {
        api_.retryQueuedRequests();
    }
}

void SessionController::clearLocal()
{
    const bool wasSignedIn = signedIn_;
    const bool hadEmail = !email_.isEmpty();
    signedIn_ = false;
    accessToken_.clear();
    refreshToken_.clear();
    accessTokenExpiresAt_ = {};
    refreshInFlight_ = false;
    email_.clear();
    api_.setAccessToken({});
    if (wasSignedIn) {
        emit signedInChanged();
    }
    if (hadEmail) {
        emit emailChanged();
    }
}

void SessionController::setStatus(const QString& message)
{
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusChanged();
}