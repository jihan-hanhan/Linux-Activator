// activate-daemon —— 生命周期执行器
#include <QCoreApplication>
#include <QTimer>
#include <QLockFile>
#include <QDir>
#include <QFile>
#include <unistd.h>

#include "common.h"

// 尽力补全显示环境(兜底, 正常由 DE autostart 提供完整环境)
static void bootstrapDisplayEnv()
{
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") || qEnvironmentVariableIsSet("DISPLAY"))
        return;

    const QString runtime = qEnvironmentVariable(
        "XDG_RUNTIME_DIR",
        QStringLiteral("/run/user/%1").arg(::getuid()));

    const QStringList socks = QDir(runtime).entryList(
        { QStringLiteral("wayland-*") }, QDir::System);
    if (!socks.isEmpty()) {
        qputenv("WAYLAND_DISPLAY", socks.first().toUtf8());
        return;
    }

    if (QFile::exists(QStringLiteral("/tmp/.X11-unix/X0"))) {
        qputenv("DISPLAY", ":0");
        const QString xauth = QDir::homePath() + QStringLiteral("/.Xauthority");
        if (!qEnvironmentVariableIsSet("XAUTHORITY") && QFile::exists(xauth))
            qputenv("XAUTHORITY", xauth.toUtf8());
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QDir().mkpath(act::cacheDir());

    QLockFile lock(act::lockPath());
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100))
        return 0; // 已有实例

    bootstrapDisplayEnv();

    // 等桌面环境准备好(5s)再执行开机钩子
    QTimer::singleShot(5000, [] {
        bootstrapDisplayEnv();

        const act::Config cfg = act::loadConfig();
        if (cfg.activated) {
            act::runBlock(cfg.root, QStringLiteral("on-boot-activated"));
            qApp->quit();
            return;
        }

        act::runBlock(cfg.root, QStringLiteral("on-boot-unactivated"));

        auto *timer = new QTimer;
        QObject::connect(timer, &QTimer::timeout, [] {
            bootstrapDisplayEnv();
            const act::Config now = act::loadConfig();
            if (now.activated) {
                act::runBlock(now.root, QStringLiteral("on-activate"));
                qApp->quit();
                return;
            }
            act::runBlock(now.root, QStringLiteral("on-unactivated-tick"));
        });
        timer->start(3000);
    });

    return QCoreApplication::exec();
}
