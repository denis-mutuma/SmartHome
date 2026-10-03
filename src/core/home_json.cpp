#include "home_json.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <initializer_list>

QString parseErrorMessage(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return {};
    }
    const QJsonObject object = document.object();
    for (const char* key : {"error_description", "message", "msg", "error"}) {
        const QJsonValue value = object.value(QLatin1String(key));
        if (value.isString() && !value.toString().isEmpty()) {
            return value.toString();
        }
    }
    return {};
}