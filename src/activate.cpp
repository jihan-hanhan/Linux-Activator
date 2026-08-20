// activate —— 激活入口 (CLI + 向导 UI)
#include <QApplication>
#include <QWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QMouseEvent>
#include <QRandomGenerator>

#include <cstring>

#include "common.h"

class ActivationWindow : public QWidget {
public:
    ActivationWindow()
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        setFixedSize(560, 280);
        setStyleSheet(QStringLiteral(
            "QWidget#body{background:#106EBE;}"
            "QLabel{color:#FFFFFF;background:transparent;}"
            "QLineEdit{background:#FFFFFF;color:#000000;border:none;padding:4px 6px;}"
            "QPushButton{background:transparent;border:1px solid #FFFFFF;color:#FFFFFF;padding:5px 20px;}"
            "QPushButton:hover{background:rgba(255,255,255,0.2);}"
            "QPushButton:disabled{color:rgba(255,255,255,0.4);border-color:rgba(255,255,255,0.4);}"));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto *titleBar = new QWidget(this);
        titleBar->setFixedHeight(30);
        titleBar->setStyleSheet(QStringLiteral("background:#FFFFFF;"));
        m_title = new QLabel(QStringLiteral("输入产品密钥"), titleBar);
        m_title->setStyleSheet(QStringLiteral("color:#000000;padding-left:10px;font-size:12px;"));
        auto *tl = new QHBoxLayout(titleBar);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->addWidget(m_title);
        root->addWidget(titleBar);

        m_body = new QWidget(this);
        m_body->setObjectName(QStringLiteral("body"));
        root->addWidget(m_body, 1);

        buildKeyPage();
    }

protected:
    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev->position().y() < 30 && windowHandle()) { windowHandle()->startSystemMove(); ev->accept(); }
        else ev->ignore();
    }

private:
    void buildKeyPage()
    {
        auto *page = new QWidget;
        auto *v = new QVBoxLayout(page);
        v->setContentsMargins(24, 18, 24, 16);
        v->setSpacing(10);

        auto *h = new QLabel(QStringLiteral("输入产品密钥"));
        h->setStyleSheet(QStringLiteral("font-size:19px;font-weight:600;"));
        v->addWidget(h);

        auto *desc = new QLabel(QStringLiteral("产品密钥应该在 Linux 销售方或经销方发送给你的电子邮件中，或者在 Linux DVD 或 USB 包装盒上。"));
        desc->setWordWrap(true);
        desc->setStyleSheet(QStringLiteral("font-size:12px;"));
        v->addWidget(desc);

        v->addWidget(new QLabel(QStringLiteral("产品密钥")));

        m_keyEdit = new QLineEdit;
        m_keyEdit->setClearButtonEnabled(true);
        m_keyEdit->setFixedWidth(320);
        v->addWidget(m_keyEdit);

        m_keyError = new QLabel;
        m_keyError->setStyleSheet(QStringLiteral("font-size:12px;color:#FFD9D9;"));
        m_keyError->hide();
        v->addWidget(m_keyError);

        v->addStretch();

        auto *btnRow = new QHBoxLayout;
        btnRow->addStretch();
        m_nextBtn = new QPushButton(QStringLiteral("下一页"));
        auto *cancel = new QPushButton(QStringLiteral("取消"));
        btnRow->addWidget(m_nextBtn);
        btnRow->addSpacing(10);
        btnRow->addWidget(cancel);
        v->addLayout(btnRow);

        connect(cancel, &QPushButton::clicked, this, &QWidget::close);
        connect(m_nextBtn, &QPushButton::clicked, this, [this] {
            m_key = act::normalizeKey(m_keyEdit->text());
            QString flat = m_key;
            flat.remove('-');
            if (flat.size() != 25) {
                m_keyError->setText(QStringLiteral("产品密钥无效。请检查密钥后重试。"));
                m_keyError->setStyleSheet(QStringLiteral("font-size:12px;color:#FFD9D9;"));
                m_keyError->show();
                return;
            }

            m_nextBtn->setEnabled(false);
            m_keyError->setText(QStringLiteral("正在激活..."));
            m_keyError->setStyleSheet(QStringLiteral("font-size:12px;color:#FFFFFF;"));
            m_keyError->show();

            QTimer::singleShot(1500, this, [this] {
                const bool ok = QRandomGenerator::global()->bounded(100) < 50;
                if (ok) {
                    act::setState(true, m_key);
                    // daemon 没在跑时, 由 UI 当场执行转变钩子
                    if (!act::daemonRunning())
                        act::runBlock(act::loadConfig().root, QStringLiteral("on-activate"));
                    m_keyError->setText(QStringLiteral("Linux 已激活。你的设备已使用与此硬件关联的数字许可证激活。"));
                    m_keyError->setStyleSheet(QStringLiteral("font-size:12px;color:#FFFFFF;"));
                    QTimer::singleShot(2000, this, &QWidget::close);
                } else {
                    act::runBlock(act::loadConfig().root, QStringLiteral("on-activation-failed"));
                    m_keyError->setText(QStringLiteral("我们无法激活你的 Linux 副本。请稍后再试。错误代码: 0x803F7001"));
                    m_keyError->setStyleSheet(QStringLiteral("font-size:12px;color:#FFD9D9;"));
                    m_nextBtn->setEnabled(true);
                }
            });
        });

        auto *bl = new QVBoxLayout(m_body);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->addWidget(page);
    }

    QLabel *m_title = nullptr;
    QWidget *m_body = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    QLabel *m_keyError = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QString m_key;
};

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--init") == 0) {
        act::loadConfig(); // 不存在则生成模板
        printf("%s\n", qPrintable(act::configPath()));
        return 0;
    }

    const act::Config cfg = act::loadConfig();
    if (cfg.activated) {
        printf("%s\n%s\n\n", qPrintable(act::osPrettyName()), qPrintable(act::kernelString()));
        printf("激活状态: 已激活\n");
        if (cfg.productKey.size() >= 5)
            printf("产品密钥: *****-*****-*****-*****-%s\n", qPrintable(cfg.productKey.right(5)));
        return 0;
    }

    if (!act::isX11() && !act::isWayland()) {
        fprintf(stderr, "无法连接到桌面会话。\n");
        return 1;
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("activate"));
    app.setQuitOnLastWindowClosed(true);
    ActivationWindow w;
    w.show();
    return app.exec();
}

#include "activate.moc"
