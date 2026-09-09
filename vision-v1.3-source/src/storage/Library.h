#pragma once
#include <QJsonObject>
#include <QSqlDatabase>
#include <QVariantMap>

namespace vision {
class Library {
public:
    explicit Library(const QString& root = {});
    ~Library();
    bool ready() const { return db_.isOpen(); }
    QString error() const;
    QJsonObject settings() const;
    bool saveSettings(const QJsonObject& settings);
    QJsonObject state(const QString& path) const;
    bool save(const QString& path, double position, double duration, const QJsonObject& state);
    QList<QVariantMap> entries(bool favorites = false) const;
    bool favorite(const QString& path) const;
    bool toggleFavorite(const QString& path);
    bool clearHistory();
    QString root() const { return root_; }
private:
    QString root_, connection_;
    QSqlDatabase db_;
};
}
