#pragma once
#include <QObject>
#include <QLibrary>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include <QVariantMap>
#include <QSize>
#include <atomic>
#include <mpv/client.h>

namespace vision {
class MpvEngine final : public QObject {
    Q_OBJECT
public:
    explicit MpvEngine(QObject* parent = nullptr);
    ~MpvEngine() override;
    bool initialize(quintptr window, bool hardware, bool headless = false);
    bool open(const QString& path);
    static bool isLocalMedia(const QString& path);
    void command(const QStringList& args);
    void set(const QString& property, const QString& value);
    QString text(const char* property) const;
    double number(const char* property, double fallback = 0) const;
    bool flag(const char* property) const;
    QList<QVariantMap> tracks() const;
    void step(int direction);
    void seek(double seconds, bool absolute = false);
    void togglePause();
    void originalSize();
    QSize displaySize() const;
    bool ready() const { return loaded_; }
    bool stepping() const { return stepping_; }
    QString path() const { return path_; }
    QString failure() const { return failure_; }
signals:
    void changed();
    void loaded();
    void ended();
    void error(const QString& message);
    void framePresented(double pts);
private:
    void poll();
    void startStep();
    QLibrary library_;
    mpv_handle* handle_ = nullptr;
#define MPV_API(name) decltype(&mpv_##name) name##_ = nullptr
    MPV_API(create); MPV_API(initialize); MPV_API(terminate_destroy);
    MPV_API(set_option_string); MPV_API(command_async); MPV_API(set_property_string);
    MPV_API(get_property); MPV_API(get_property_string); MPV_API(free);
    MPV_API(free_node_contents); MPV_API(wait_event); MPV_API(error_string);
    MPV_API(set_wakeup_callback);
#undef MPV_API
    QTimer timer_;
    QElapsedTimer stepClock_;
    QElapsedTimer refreshClock_;
    std::atomic_bool wakePending_{false};
    QQueue<int> steps_;
    QString path_, failure_;
    bool loaded_ = false, stepping_ = false, manualStep_ = false, eof_ = false;
    double stepStart_ = 0;
    double firstPts_ = 0;
};
}
