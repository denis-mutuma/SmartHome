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
    if (!accessToken_.isEmpty()) {
        api_.logOut();
    }
    const bool tokenCleared = clearRefreshToken(tokenFilePath_);
    clearLocal();
    setStatus(tokenCleared ? QString() : tr("Could not clear the session."));
}

void SessionController::onCompleted(const QString& op, quint64, int, const QByteArray& body)
{
    if (op != QLatin1String("login") && op != QLatin1String("signup")) {
        return;
    }
    const std::optional<SessionTokens> session = parseSession(body);
    if (!session.has_value()) {
        setStatus(tr("The service could not complete the request."));
        return;
    }
    applySession(*session);
}

void SessionController::onFailed(const QString& op, quint64, int, const QString& message)
{
    if (op == QLatin1String("login") || op == QLatin1String("signup")) {
        setStatus(message);
    }
}

void SessionController::applySession(const SessionTokens& session)
{
    if (!saveRefreshToken(session.refreshToken, tokenFilePath_)) {
        setStatus(tr("Could not save the session."));
        return;
    }
    const bool wasSignedIn = signedIn_;
    accessToken_ = session.accessToken;
    email_ = session.email;
    api_.setAccessToken(accessToken_);
    signedIn_ = true;
    if (!wasSignedIn) {
        emit signedInChanged();
    }
    emit emailChanged();
    setStatus({});
}

void SessionController::clearLocal()
{
    const bool wasSignedIn = signedIn_;
    const bool hadEmail = !email_.isEmpty();
    signedIn_ = false;
    accessToken_.clear();
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