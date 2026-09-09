#include <QCoreApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextStream>
#include <cmath>
#include <functional>
#include "player/MpvEngine.h"
#include "storage/Library.h"

bool waitFor(const std::function<bool()>& condition, int timeout = 8000)
{
    QElapsedTimer clock; clock.start();
    while(clock.elapsed()<timeout){QCoreApplication::processEvents();if(condition())return true;QThread::msleep(5);}return false;
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv); QTextStream out(stdout);
    if(argc!=3){out<<"Usage: vision-core-checks media expected-pts.json\n";return 2;}
    QString path=QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    QFile expectedFile(QString::fromLocal8Bit(argv[2]));if(!expectedFile.open(QIODevice::ReadOnly))return 2;
    auto expected=QJsonDocument::fromJson(expectedFile.readAll()).array();if(expected.size()<16)return 2;
    QTemporaryDir temp;
    {
        vision::Library library(temp.path());
        if(!library.ready()||!library.saveSettings({{"volume",42}})||library.settings()["volume"].toInt()!=42)return 3;
        if(!library.save(path,1.25,4,{{"sid","no"}})||!library.toggleFavorite(path)||!library.save(path,1.5,4,{{"sid","no"}})||!library.favorite(path))return 3;
    }
    {
        vision::Library library(temp.path());
        if(library.state(path)["position"].toDouble()!=1.5||!library.clearHistory()||!library.entries().isEmpty()||library.entries(true).size()!=1)return 3;
    }
    if(vision::MpvEngine::isLocalMedia("https://example.com/video.mp4")||vision::MpvEngine::isLocalMedia("\\\\server\\video.mp4"))return 3;
    out<<"PASS local storage, restart, favorites, history clearing, input rejection\n";out.flush();
    vision::MpvEngine engine;QString error;
    QObject::connect(&engine,&vision::MpvEngine::error,&app,[&](const QString& e){error=e;out<<"ERROR "<<e<<'\n';out.flush();});
    if(!engine.initialize(0,false,true)){out<<engine.failure()<<'\n';return 4;}
    int idleUpdates=0;auto idleConnection=QObject::connect(&engine,&vision::MpvEngine::changed,&app,[&]{++idleUpdates;});QElapsedTimer idleTime;idleTime.start();waitFor([&]{return idleTime.elapsed()>=350;});QObject::disconnect(idleConnection);if(idleUpdates>1)return 17;out<<"PASS idle event handling without periodic polling\n";
    bool loaded=false;QObject::connect(&engine,&vision::MpvEngine::loaded,&app,[&]{loaded=true;});
    if(!engine.open(path)||!waitFor([&]{return loaded&&!engine.text("time-pos").isEmpty();})){out<<"load timeout loaded="<<loaded<<" path="<<engine.text("path")<<" expected="<<path<<" time="<<engine.text("time-pos")<<" pts="<<engine.text("video-pts")<<" vid="<<engine.text("vid")<<'\n';return 5;}
    out<<engine.text("mpv-version")<<" | FPS "<<engine.text("container-fps")<<'\n';out.flush();
    engine.set("video-aspect-override","4:3");engine.set("panscan","1");engine.set("video-zoom","1");
    engine.originalSize();
    out<<"unscaled="<<engine.text("video-unscaled")<<" aspect="<<engine.text("video-aspect-override")<<" zoom="<<engine.number("video-zoom")<<" panscan="<<engine.number("panscan")<<Qt::endl;
    if(engine.text("video-unscaled")!="no"||engine.number("video-zoom")!=0||engine.number("panscan")!=0||engine.number("video-aspect-override")!=-2)return 14;
    engine.set("video-unscaled","no");
    out<<"unscaled="<<engine.text("video-unscaled")<<" aspect="<<engine.text("video-aspect-override")<<" zoom="<<engine.number("video-zoom")<<" panscan="<<engine.number("panscan")<<Qt::endl;
    if(engine.text("video-unscaled")!="no")return 14;
    out<<"PASS native-fit resets aspect/crop/zoom without cropping\n";
    QElapsedTimer settle;settle.start();waitFor([&]{return settle.elapsed()>=250&&!engine.flag("seeking");});
    int presented=0;QObject::connect(&engine,&vision::MpvEngine::framePresented,&app,[&](double){++presented;});
    int index=0;
    auto step=[&](int direction){
        int before=presented;engine.step(direction);index+=direction;
        if(!waitFor([&]{return presented>before||!error.isEmpty();}))return false;
        double pts=engine.number("video-pts",engine.number("time-pos"));
        double wanted=expected[index].toDouble();out<<"frame "<<index<<" pts "<<QString::number(pts,'f',6)<<" expected "<<QString::number(wanted,'f',6)<<'\n';out.flush();
        return error.isEmpty()&&std::abs(pts-wanted)<0.002&&engine.flag("pause");
    };
    for(int i=0;i<15;++i)if(!step(1))return 6;
    for(int i=0;i<15;++i)if(!step(-1))return 7;
    for(int i=0;i<3;++i){if(!step(1)||!step(-1))return 8;}
    engine.set("speed","2");if(!step(1))return 9;
    const int beforeQueue=presented;engine.step(1);engine.step(1);engine.step(1);
    const bool queueDone=waitFor([&]{return presented>=beforeQueue+3;});out<<"queue completed="<<queueDone<<" frames="<<presented-beforeQueue<<" pts="<<engine.number("time-pos")<<" wanted="<<expected[4].toDouble()<<'\n';out.flush();
    if(!queueDone||std::abs(engine.number("time-pos")-expected[4].toDouble())>0.002)return 11;
    const auto tracks=engine.tracks();QString firstAudio,secondAudio,subtitle;
    for(const auto& track:tracks){if(track["type"]=="audio"){if(firstAudio.isEmpty())firstAudio=track["id"].toString();else secondAudio=track["id"].toString();}if(track["type"]=="sub")subtitle=track["id"].toString();}
    if(!secondAudio.isEmpty()){engine.set("aid",secondAudio);if(!waitFor([&]{return engine.text("aid")==secondAudio;}))return 12;engine.set("aid",firstAudio);out<<"PASS multiple audio tracks\n";}
    if(!subtitle.isEmpty()){engine.set("sid",subtitle);if(engine.text("sid")!=subtitle)return 13;engine.set("sid","no");if(engine.text("sid")!="no")return 13;out<<"PASS embedded subtitle selection\n";}
    engine.set("audio-delay","0.05");engine.set("sub-delay","0.1");
    if(std::abs(engine.number("audio-delay")-0.05)>0.0001||std::abs(engine.number("sub-delay")-0.1)>0.0001)return 10;
    if(!firstAudio.isEmpty()){
        for(const QString& layout:QStringList{"auto-safe","mono","stereo","fl-fr-fc","fl-fr-bc","fl-fr-fc-bc","fl-fr-bl-br","fl-fr-fc-bl-br","fl-fr-fc-bc-sl-sr","fl-fr-fc-bl-br-sl-sr","fc-lfe","fl-fr-lfe","fl-fr-fc-lfe","fl-fr-bc-lfe","fl-fr-fc-bc-lfe","fl-fr-bl-br-lfe","5.1","6.1","7.1","auto"}){
            engine.set("audio-channels",layout);if(!error.isEmpty())return 15;
        }
        engine.set("audio-channels","mono");
        if(!waitFor([&]{return engine.number("audio-out-params/channel-count")==1;}))return 16;
        engine.set("audio-channels","stereo");
        if(!waitFor([&]{return engine.number("audio-out-params/channel-count")==2;}))return 16;
        out<<"PASS 20 output layouts accepted, actual mono/stereo output confirmed (null audio device)\n";
    }
    out<<"PASS forward/backward source frames, GOP crossing, round trips, speed-independent steps, sync properties\n";
    return 0;
}
