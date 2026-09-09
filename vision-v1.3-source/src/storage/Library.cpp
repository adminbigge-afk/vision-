#include "Library.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QDateTime>

namespace vision {
Library::Library(const QString& root)
{
    root_ = root.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/vision" : root;
    QDir().mkpath(root_);
    connection_ = QUuid::createUuid().toString();
    db_ = QSqlDatabase::addDatabase("QSQLITE", connection_);
    db_.setDatabaseName(root_ + "/library.sqlite");
    if (!db_.open()) return;
    QSqlQuery q(db_);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA busy_timeout=2000");
    if (!q.exec("CREATE TABLE IF NOT EXISTS media(path TEXT PRIMARY KEY, size INTEGER, modified INTEGER, position REAL DEFAULT 0, duration REAL DEFAULT 0, state TEXT DEFAULT '{}', played INTEGER DEFAULT 0, favorite INTEGER DEFAULT 0)")) db_.close();
}
Library::~Library() { db_.close(); db_ = {}; QSqlDatabase::removeDatabase(connection_); }
QString Library::error() const { return db_.lastError().text(); }
QJsonObject Library::settings() const
{
    QFile f(root_ + "/settings.json");
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
bool Library::saveSettings(const QJsonObject& settings)
{
    QSaveFile f(root_ + "/settings.json");
    if (!f.open(QIODevice::WriteOnly)) return false;
    const auto data = QJsonDocument(settings).toJson();
    return f.write(data) == data.size() && f.commit();
}
QJsonObject Library::state(const QString& path) const
{
    QSqlQuery q(db_); q.prepare("SELECT position,duration,state,size,modified FROM media WHERE path=?"); q.addBindValue(path);
    if (!q.exec() || !q.next()) return {};
    QFileInfo file(path);
    if (q.value(3).toLongLong() != file.size() || q.value(4).toLongLong() != file.lastModified().toMSecsSinceEpoch()) return {};
    auto result = QJsonDocument::fromJson(q.value(2).toByteArray()).object();
    result["position"] = q.value(0).toDouble(); result["duration"] = q.value(1).toDouble();
    return result;
}
bool Library::save(const QString& path, double position, double duration, const QJsonObject& state)
{
    QFileInfo file(path);
    if (!file.isFile()) return false;
    QSqlQuery q(db_);
    q.prepare("INSERT INTO media(path,size,modified,position,duration,state,played) VALUES(?,?,?,?,?,?,?) ON CONFLICT(path) DO UPDATE SET size=excluded.size,modified=excluded.modified,position=excluded.position,duration=excluded.duration,state=excluded.state,played=excluded.played");
    q.addBindValue(path); q.addBindValue(file.size()); q.addBindValue(file.lastModified().toMSecsSinceEpoch());
    q.addBindValue(position); q.addBindValue(duration); q.addBindValue(QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact)));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch()); return q.exec();
}
QList<QVariantMap> Library::entries(bool favorites) const
{
    QList<QVariantMap> result; QSqlQuery q(db_);
    q.exec(favorites ? "SELECT path,position,duration,favorite FROM media WHERE favorite=1 ORDER BY played DESC" : "SELECT path,position,duration,favorite FROM media WHERE played>0 ORDER BY played DESC LIMIT 1000");
    while (q.next()) result.append({{"path",q.value(0)},{"position",q.value(1)},{"duration",q.value(2)},{"favorite",q.value(3)}});
    return result;
}
bool Library::favorite(const QString& path) const
{
    QSqlQuery q(db_); q.prepare("SELECT favorite FROM media WHERE path=?"); q.addBindValue(path);
    return q.exec() && q.next() && q.value(0).toBool();
}
bool Library::toggleFavorite(const QString& path)
{
    QSqlQuery q(db_); q.prepare("INSERT INTO media(path,favorite) VALUES(?,1) ON CONFLICT(path) DO UPDATE SET favorite=1-favorite"); q.addBindValue(path); return q.exec();
}
bool Library::clearHistory()
{
    if (!db_.transaction()) return false;
    QSqlQuery q(db_);
    if (!q.exec("DELETE FROM media WHERE favorite=0") || !q.exec("UPDATE media SET played=0,position=0,duration=0,state='{}' WHERE favorite=1")) { db_.rollback(); return false; }
    return db_.commit();
}
}
