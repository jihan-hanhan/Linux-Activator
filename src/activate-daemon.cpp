// activate-daemon —— 生命周期执行器
#include <QCoreApplication>
#include <QTimer>
#include <QLockFile>
#include <QDir>

#include "common.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QDir().mkpath(act::cacheDir());

    QLockFile lock(act::lockPath());
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100))
        return 0; // 已有实例

    act::Config cfg = act::loadConfig();

    if (cfg.activated) {
        // 开机即已激活: 执行一次后退出, 不驻留
        act::runBlock(cfg.root, QStringLiteral("on-boot-activated"));
        return 0;
    }

    // 开机未激活
    act::runBlock(cfg.root, QStringLiteral("on-boot-unactivated"));

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [] {
        const act::Config now = act::loadConfig();
        if (now.activated) {
            // 未激活 -> 激活 的转变瞬间
            act::runBlock(now.root, QStringLiteral("on-activate"));
            qApp->quit();
            return;
        }
        act::runBlock(now.root, QStringLiteral("on-unactivated-tick"));
    });
    timer.start(3000);
    return QCoreApplication::exec();
}
