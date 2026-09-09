#include "MpvEngine.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <cmath>

namespace vision {
MpvEngine::MpvEngine(QObject* parent) : QObject(parent)
{
    connect(&timer_, &QTimer::timeout, this, &MpvEngine::poll);
}
MpvEngine::~MpvEngine()
{
    timer_.stop();
    if (handle_) { set_wakeup_callback_(handle_,nullptr,nullptr); terminate_destroy_(handle_); }
}
bool MpvEngine::initialize(quintptr window, bool hardware, bool headless)
{
    library_.setFileName(QCoreApplication::applicationDirPath() + "/libmpv-2.dll");
    if (!library_.load()) { failure_ = library_.errorString(); return false; }
#define LOAD(name) name##_ = reinterpret_cast<decltype(name##_)>(library_.resolve("mpv_" #name)); if (!name##_) { failure_ = "Missing mpv API: " #name; return false; }
    LOAD(create) LOAD(initialize) LOAD(terminate_destroy) LOAD(set_option_string)
    LOAD(command_async) LOAD(set_property_string) LOAD(get_property)
    LOAD(get_property_string) LOAD(free) LOAD(free_node_contents) LOAD(wait_event) LOAD(error_string)
    LOAD(set_wakeup_callback)
#undef LOAD
    handle_ = create_();
    if (!handle_) { failure_ = "Cannot create libmpv"; return false; }
    const QList<QPair<QByteArray,QByteArray>> options = {
        {"config","no"}, {"load-scripts","no"}, {"ytdl","no"},
        {"osc","no"}, {"load-osd-console","no"}, {"terminal","no"},
        {"input-default-bindings","no"}, {"input-vo-keyboard","no"},
        {"input-cursor","no"}, {"cursor-autohide","no"},
        {"idle","yes"}, {"keep-open","yes"}, {"pause","yes"},
        {"access-references","no"},
        {"demuxer-lavf-o","protocol_whitelist=%24%file,crypto,data,subfile"},
        {"hr-seek","yes"}, {"hr-seek-framedrop","no"},
        {"audio-pitch-correction","yes"}, {"interpolation","no"}, {"deinterlace","no"},
        {"hwdec", hardware ? "auto-safe" : "no"},
        {"vo", headless ? "null" : "gpu-next"},
        {"ao", headless ? "null" : "wasapi"}
    };
    for (const auto& option : options) {
        int result = set_option_string_(handle_, option.first.constData(), option.second.constData());
        if (result < 0) { failure_ = QString::fromUtf8(option.first) + ": " + error_string_(result); return false; }
    }
    if (!headless) {
        set_option_string_(handle_, "wid", QByteArray::number(window).constData());
        set_option_string_(handle_, "gpu-api", "d3d11");
        set_option_string_(handle_, "gpu-context", "d3d11");
    }
    int result = initialize_(handle_);
    if (result < 0) { failure_ = QString::fromUtf8(error_string_(result)); return false; }
    refreshClock_.start();
    set_wakeup_callback_(handle_,[](void* context){
        auto* self=static_cast<MpvEngine*>(context);
        if(self->wakePending_.exchange(true))return;
        // libmpv calls from its own thread; all state and UI work stays on the Qt thread.
        QMetaObject::invokeMethod(self,[self]{self->poll();},Qt::QueuedConnection);
    },this);
    return true;
}
bool MpvEngine::isLocalMedia(const QString& path)
{
    const QString normalized = QDir::fromNativeSeparators(path);
    const QFileInfo file(path);
    const QStringList formats = {"mp4","mkv","avi","mov","webm","flv"};
    return !normalized.startsWith("//") && file.isAbsolute() && file.isFile()
        && file.isReadable() && formats.contains(file.suffix().toLower());
}
bool MpvEngine::open(const QString& path)
{
    if (!handle_ || !isLocalMedia(path)) { emit error(QStringLiteral("文件不可读或格式不支持：%1").arg(path)); return false; }
    loaded_ = false; stepping_ = false; manualStep_ = false; eof_ = false; steps_.clear();
    path_ = QFileInfo(path).absoluteFilePath();
    timer_.start(100);
    command({"loadfile", path_, "replace"});
    return true;
}
void MpvEngine::command(const QStringList& args)
{
    if (!handle_) return;
    QList<QByteArray> encoded;
    for (const auto& item : args) encoded.append(item.toUtf8());
    QList<const char*> pointers;
    for (const auto& item : encoded) pointers.append(item.constData());
    pointers.append(nullptr);
    const int result = command_async_(handle_, 0, pointers.data());
    if (result < 0) emit error(QString::fromUtf8(error_string_(result)));
}
void MpvEngine::set(const QString& property, const QString& value)
{
    if (!handle_) return;
    int result = set_property_string_(handle_, property.toUtf8().constData(), value.toUtf8().constData());
    if (result < 0) emit error(property + ": " + QString::fromUtf8(error_string_(result)));
}
void MpvEngine::originalSize()
{
    if (!ready()) return;
    set("keepaspect", "yes");
    set("video-aspect-override", "no");
    set("panscan", "0");
    set("video-zoom", "0");
    set("video-pan-x", "0");
    set("video-pan-y", "0");
    set("video-unscaled", "no");
}
QSize MpvEngine::displaySize() const
{
    const double w=number("dwidth"),h=number("dheight");
    if(w<=0||h<=0)return {};
    const double angle=number("video-out-params/rotate")*3.14159265358979323846/180;
    return {qRound(std::abs(w*std::cos(angle))+std::abs(h*std::sin(angle))),
            qRound(std::abs(w*std::sin(angle))+std::abs(h*std::cos(angle)))};
}
QString MpvEngine::text(const char* property) const
{
    if (!handle_) return {};
    char* value = get_property_string_(handle_, property);
    QString result = value ? QString::fromUtf8(value) : QString();
    if (value) free_(value);
    return result;
}
double MpvEngine::number(const char* property, double fallback) const
{
    double value = fallback;
    if (!handle_ || get_property_(handle_, property, MPV_FORMAT_DOUBLE, &value) < 0 || !std::isfinite(value)) return fallback;
    return value;
}
bool MpvEngine::flag(const char* property) const
{
    int value = 0;
    return handle_ && get_property_(handle_, property, MPV_FORMAT_FLAG, &value) >= 0 && value;
}
QList<QVariantMap> MpvEngine::tracks() const
{
    QList<QVariantMap> result;
    mpv_node node{};
    if (!handle_ || get_property_(handle_, "track-list", MPV_FORMAT_NODE, &node) < 0) return result;
    if (node.format == MPV_FORMAT_NODE_ARRAY) {
        for (int i = 0; i < node.u.list->num; ++i) {
            const auto& track = node.u.list->values[i];
            if (track.format != MPV_FORMAT_NODE_MAP) continue;
            QVariantMap map;
            for (int j = 0; j < track.u.list->num; ++j) {
                const auto& value = track.u.list->values[j];
                QString key = QString::fromUtf8(track.u.list->keys[j]);
                if (value.format == MPV_FORMAT_STRING) map[key] = QString::fromUtf8(value.u.string);
                if (value.format == MPV_FORMAT_INT64) map[key] = static_cast<qlonglong>(value.u.int64);
                if (value.format == MPV_FORMAT_FLAG) map[key] = bool(value.u.flag);
            }
            result.append(map);
        }
    }
    free_node_contents_(&node);
    return result;
}
void MpvEngine::step(int direction)
{
    if (!loaded_ || text("vid") == "no" || steps_.size() >= 32) return;
    manualStep_ = true;
    steps_.enqueue(direction > 0 ? 1 : -1);
    if (!stepping_) { set("pause", "yes"); startStep(); }
}
void MpvEngine::startStep()
{
    if (steps_.isEmpty()) { stepping_ = false; if(loaded_)timer_.start(100);return; }
    const int direction = steps_.dequeue();
    stepStart_ = number("video-pts", number("time-pos"));
    if ((direction < 0 && stepStart_ <= firstPts_ + 0.000001) || (direction > 0 && flag("eof-reached"))) {
        QTimer::singleShot(0, this, &MpvEngine::startStep); return;
    }
    stepping_ = true; stepClock_.restart();
    timer_.start(20);
    if (direction > 0) command({"frame-step", "1", "mute"});
    else command({"frame-back-step"});
}
void MpvEngine::seek(double seconds, bool absolute)
{
    if (!loaded_) return;
    steps_.clear(); stepping_ = false; manualStep_ = false;timer_.start(100);
    command({"seek", QString::number(seconds, 'f', 6), absolute ? "absolute+exact" : "relative+exact"});
}
void MpvEngine::togglePause()
{
    if (!loaded_) return;
    steps_.clear(); stepping_ = false; manualStep_ = false;timer_.start(100);
    if (flag("eof-reached")) seek(0, true);
    set("pause", flag("pause") ? "no" : "yes");
}
void MpvEngine::poll()
{
    wakePending_=false;
    for (int i = 0; i < 128; ++i) {
        mpv_event* event = wait_event_(handle_, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        if (event->event_id == MPV_EVENT_FILE_LOADED && QFileInfo(text("path")).absoluteFilePath() == path_) {
            loaded_ = true; firstPts_ = number("time-pos"); emit loaded();
        }
        if (event->event_id == MPV_EVENT_COMMAND_REPLY && event->error < 0) {
            steps_.clear(); stepping_ = false;
            emit error(QString::fromUtf8(error_string_(event->error)));
        }
        if (event->event_id == MPV_EVENT_END_FILE) {
            auto* end = static_cast<mpv_event_end_file*>(event->data);
            if (end && end->reason == MPV_END_FILE_REASON_ERROR) {
                loaded_ = false; timer_.stop();emit error(QStringLiteral("无法播放：") + QString::fromUtf8(error_string_(end->error)));
            }
        }
    }
    if (stepping_) {
        const double pts = number("video-pts", number("time-pos"));
        if (flag("pause") && !flag("seeking") && std::abs(pts - stepStart_) > 0.000001) {
            emit framePresented(pts); stepping_ = false; startStep();
        } else if (stepClock_.elapsed() > 8000) {
            stepping_ = false; steps_.clear();
            timer_.start(100);
            if (!flag("eof-reached")) emit error(QStringLiteral("此处逐帧定位未完成；未使用时间跳转替代。"));
        }
    }
    const bool eof = loaded_ && flag("eof-reached");
    if (eof && !eof_ && !manualStep_) emit ended();
    eof_ = eof;
    if(refreshClock_.elapsed()>=100){refreshClock_.restart();emit changed();}
}
}
