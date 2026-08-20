#pragma once

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QList>
#include <sys/utsname.h>

namespace act {

// ---------------- KDL subset ----------------
struct KdlNode {
    QString name;
    QStringList args;
    QList<KdlNode> children;
};

inline QStringList kdlTokens(const QString &line)
{
    QStringList out;
    int i = 0;
    const int n = line.size();
    while (i < n) {
        const QChar c = line[i];
        if (c == '"') {
            QString s;
            ++i;
            while (i < n && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < n) {
                    const QChar e = line[++i];
                    if (e == 'n') s += '\n';
                    else if (e == 't') s += '\t';
                    else s += e;
                } else {
                    s += line[i];
                }
                ++i;
            }
            ++i;
            out << s;
        } else if (c == '{' || c == '}' || c == ';') {
            out << QString(c);
            ++i;
        } else if ((c == '/' && i + 1 < n && line[i + 1] == '/') || c == '#') {
            break;
        } else if (!c.isSpace()) {
            QString t;
            while (i < n && !line[i].isSpace() && line[i] != '"' &&
                   line[i] != '{' && line[i] != '}' && line[i] != ';') {
                if (line[i] == '/' && i + 1 < n && line[i + 1] == '/')
                    break;
                t += line[i++];
            }
            if (!t.isEmpty())
                out << t;
            continue;
        } else {
            ++i;
        }
    }
    return out;
}

inline QList<KdlNode> parseKdl(const QString &text)
{
    QList<KdlNode> root;
    int block = -1;
    const auto lines = text.split('\n');
    for (QString raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QStringLiteral("//")) || line.startsWith('#'))
            continue;
        if (line == QStringLiteral("}")) {
            block = -1;
            continue;
        }
        const QStringList toks = kdlTokens(line);
        if (toks.isEmpty())
            continue;
        KdlNode node;
        node.name = toks.first();
        bool open = false;
        for (int i = 1; i < toks.size(); ++i) {
            if (toks[i] == QStringLiteral("{")) { open = true; break; }
            if (toks[i] == QStringLiteral(";")) break;
            node.args << toks[i];
        }
        if (block >= 0 && block < root.size())
            root[block].children << node;
        else
            root << node;
        if (open && block < 0)
            block = root.size() - 1;
    }
    return root;
}

// ---------------- paths & state ----------------
inline QString configPath() { return QDir::homePath() + QStringLiteral("/activate.kdl"); }
inline QString cacheDir()   { return QDir::homePath() + QStringLiteral("/.cache/activate-linux"); }
inline QString lockPath()   { return cacheDir() + QStringLiteral("/daemon.lock"); }

inline const char *kTemplate()
{
    return R"KDL(// =====================================================================
//  ~/activate.kdl
//  State + lifecycle hooks for the `activate` / `activate-daemon`
//  suite.  Syntax: a tiny KDL subset (nodes, quoted strings, one level
//  of {} blocks, // comments), similar in spirit to niri's config.kdl.
//
//  The binaries are plain executors: the commands listed in each block
//  are run verbatim at the corresponding moment.
//
//  Verbs available inside a block:
//    spawn    "prog" "arg1" ...    start detached, never wait
//    exec     "prog" "arg1" ...    start and wait (max 5 s)
//    sh       "command line"       run via /bin/sh -c, detached
//    sh-wait  "command line"       run via /bin/sh -c, wait (max 5 s)
// =====================================================================

// ---- state (written by the activator; safe to edit by hand) --------
activated false
product-key ""

// ---- once at boot, when NOT activated (activate-daemon) ------------
on-boot-unactivated {
    // Raise the "Activate Linux" watermark.  The pgrep guard keeps it
    // from ever starting twice.  Delete the line to disable watermark.
    sh "command -v activate-linux >/dev/null 2>&1 && { pgrep -x activate-linux >/dev/null 2>&1 || activate-linux -t 'Activate Linux' -m 'Go to Settings to activate Linux.'; }"

    // Add your own, e.g.:
    // spawn "notify-send" "Linux is not activated"
}

// ---- every ~3 s while unactivated (activate-daemon) ----------------
// Keeps the watermark alive if the user kills it.  Empty this block
// if you prefer one-shot behaviour.
on-unactivated-tick {
    sh "command -v activate-linux >/dev/null 2>&1 && { pgrep -x activate-linux >/dev/null 2>&1 || activate-linux -t 'Activate Linux' -m 'Go to Settings to activate Linux.'; }"
}

// ---- once at boot, when already activated --------------------------
// activate-daemon exits right after this block on activated machines.
on-boot-activated {
    // spawn "notify-send" "Linux is activated"
}

// ---- the unactivated -> activated transition, exactly once ---------
// Run by activate-daemon if resident, otherwise by the activation UI.
on-activate {
    // Kill the watermark.
    exec "pkill" "-x" "activate-linux"

    // spawn "notify-send" "Linux is activated, thank you"
}

// ---- every failed activation attempt (activation UI) ---------------
on-activation-failed {
    sh "command -v activate-linux >/dev/null 2>&1 && { pgrep -x activate-linux >/dev/null 2>&1 || activate-linux -t 'Activate Linux' -m 'Go to Settings to activate Linux.'; }"
}
)KDL";
}

struct Config {
    bool activated = false;
    QString productKey;
    QList<KdlNode> root;
};

inline Config loadConfig()
{
    Config cfg;
    if (!QFile::exists(configPath())) {
        QFile f(configPath());
        if (f.open(QFile::WriteOnly | QFile::Truncate))
            f.write(kTemplate());
    }
    QFile f(configPath());
    if (!f.open(QFile::ReadOnly))
        return cfg;
    cfg.root = parseKdl(QString::fromUtf8(f.readAll()));
    for (const KdlNode &n : cfg.root) {
        if (n.name == QStringLiteral("activated"))
            cfg.activated = n.args.value(0) == QStringLiteral("true");
        else if (n.name == QStringLiteral("product-key"))
            cfg.productKey = n.args.value(0);
    }
    return cfg;
}

// 定向替换状态行, 保留用户的注释与钩子
inline void setState(bool activated, const QString &key)
{
    QFile f(configPath());
    if (!f.open(QFile::ReadOnly))
        return;
    QString text = QString::fromUtf8(f.readAll());
    f.close();

    QRegularExpression reA(QStringLiteral("(?m)^\\s*activated\\s+(?:true|false)\\s*$"));
    if (reA.match(text).hasMatch())
        text.replace(reA, QStringLiteral("activated %1").arg(activated ? QStringLiteral("true") : QStringLiteral("false")));
    else
        text += QStringLiteral("\nactivated %1\n").arg(activated ? QStringLiteral("true") : QStringLiteral("false"));

    if (!key.isEmpty()) {
        QRegularExpression reK(QStringLiteral("(?m)^\\s*product-key\\s+\"[^\"]*\"\\s*$"));
        if (reK.match(text).hasMatch())
            text.replace(reK, QStringLiteral("product-key \"%1\"").arg(key));
        else
            text += QStringLiteral("\nproduct-key \"%1\"\n").arg(key);
    }

    f.open(QFile::WriteOnly | QFile::Truncate);
    f.write(text.toUtf8());
}

// ---------------- misc utils ----------------
inline bool has(const QString &cmd) { return !QStandardPaths::findExecutable(cmd).isEmpty(); }
inline bool isX11()     { return !qEnvironmentVariable("DISPLAY").isEmpty(); }
inline bool isWayland() { return !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty(); }

inline bool daemonRunning()
{
    QLockFile probe(lockPath());
    probe.setStaleLockTime(0);
    if (probe.tryLock(100)) {
        probe.unlock();
        return false;
    }
    return true;
}

inline QString osPrettyName()
{
    QFile f(QStringLiteral("/etc/os-release"));
    if (!f.open(QFile::ReadOnly)) return QStringLiteral("Linux");
    const auto lines = QString::fromUtf8(f.readAll()).split('\n');
    for (const QString &l : lines)
        if (l.startsWith(QStringLiteral("PRETTY_NAME=")))
            return l.mid(12).remove('"').trimmed();
    return QStringLiteral("Linux");
}

inline QString kernelString()
{
    utsname u{};
    if (uname(&u) != 0) return QStringLiteral("unknown");
    return QStringLiteral("%1 (%2)").arg(u.release, u.machine);
}

inline QString normalizeKey(const QString &raw)
{
    QString out;
    for (QChar c : raw) if (c.isLetterOrNumber()) out += c.toUpper();
    QString grouped;
    for (int i = 0; i < out.size(); ++i) {
        if (i && i % 5 == 0) grouped += '-';
        grouped += out[i];
    }
    return grouped;
}

// ---------------- the executor ----------------
inline void runBlock(const QList<KdlNode> &root, const QString &block)
{
    for (const KdlNode &n : root) {
        if (n.name != block)
            continue;
        for (const KdlNode &c : n.children) {
            if (c.args.isEmpty())
                continue;
            if (c.name == QStringLiteral("spawn")) {
                QProcess::startDetached(c.args.first(), c.args.mid(1));
            } else if (c.name == QStringLiteral("exec")) {
                QProcess p;
                p.start(c.args.first(), c.args.mid(1));
                p.waitForFinished(5000);
            } else if (c.name == QStringLiteral("sh")) {
                QProcess::startDetached(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), c.args.first() });
            } else if (c.name == QStringLiteral("sh-wait")) {
                QProcess p;
                p.start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), c.args.first() });
                p.waitForFinished(5000);
            }
        }
    }
}

} // namespace act
