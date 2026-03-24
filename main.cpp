#include "mainwindow.h"

#include <QApplication>
#include <QEventLoop>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QDesktopWidget>

namespace
{
    //第一次进入软件的预加载界面，问就是觉得好看，因为是阻塞的，并没有什么实际功能，后续可以考虑改成非阻塞的
    class StartupSplash : public QWidget
    {
    public:
        explicit StartupSplash(const QIcon &appIcon, QWidget *parent = nullptr)
            : QWidget(parent)
        {
            // 设置全屏和无边框
            setWindowFlags(Qt::FramelessWindowHint | Qt::SplashScreen | Qt::WindowStaysOnTopHint);
            setAttribute(Qt::WA_TranslucentBackground, false);

            // 设置浅橙色背景
            setStyleSheet("background-color: #fff0e6;");

            // 获取屏幕尺寸并设置为全屏
            QScreen *screen = QGuiApplication::primaryScreen();
            if (screen)
            {
                QRect screenGeometry = screen->geometry();
                setGeometry(screenGeometry);
            }
            else
            {
                // 备用方案：使用桌面尺寸
                //resize(QApplication::desktop()->screenGeometry().size());
            }

            QVBoxLayout *root = new QVBoxLayout(this);
            root->setContentsMargins(0, 0, 0, 0);
            root->setSpacing(20);

            // 图片容器
            QLabel *imageLabel = new QLabel(this);
            imageLabel->setAlignment(Qt::AlignCenter);

            // 计算图片大小：原为180，放大1.2倍
            const int baseImageSize = 180;
            const int splashImgSize = static_cast<int>(baseImageSize * 1.2);
            imageLabel->setFixedSize(splashImgSize, splashImgSize);

            // 加载图片
            QPixmap pix(QStringLiteral(":/img/MousePlan"));
            if (pix.isNull())
            {
                pix = appIcon.pixmap(splashImgSize, splashImgSize);
            }

            // 缩放图片并保持平滑
            QPixmap scaledPix = pix.scaled(splashImgSize, splashImgSize,
                                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
            imageLabel->setPixmap(scaledPix);
            imageLabel->setStyleSheet("border: none;");

            // 标题标签
            QLabel *titleLabel = new QLabel(QStringLiteral("MousePlan"), this);
            QFont titleFont;
            titleFont.setPointSize(36); // 稍微增大字体以适应全屏
            titleFont.setBold(true);
            titleLabel->setFont(titleFont);
            titleLabel->setAlignment(Qt::AlignCenter);
            titleLabel->setStyleSheet("color:#f08b2d; border: none;");

            // 添加弹性空间和内容
            root->addStretch();
            root->addWidget(imageLabel, 0, Qt::AlignHCenter);
            root->addWidget(titleLabel, 0, Qt::AlignHCenter);
            root->addStretch();
        }

        // 以全屏方式显示一个窗口，在指定的毫秒数后自动隐藏
        void showForMs(int durationMs)
        {
            // 显示为全屏
            showFullScreen();
            raise();
            activateWindow();

            // 确保窗口在最前面
            setWindowState(windowState() | Qt::WindowActive);

            QEventLoop loop;
            QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
            loop.exec();
            hide();
        }
    };

}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && !defined(Q_OS_ANDROID)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    //应用程序初始化
    QApplication a(argc, argv);
    QApplication::setApplicationName("MousePlan");

    // 字体抗锯齿优化
    QFont appFont = a.font();
    appFont.setStyleStrategy(QFont::PreferAntialias);
    a.setFont(appFont);

    // 自定义图标入口
    const QString iconPath = QDir::currentPath() + "/config/app_icon.png";
    QIcon appIcon;
    if (QFile::exists(iconPath))
    {
        appIcon = QIcon(iconPath);
        a.setWindowIcon(appIcon);
    }
    else
    {
        appIcon = a.style()->standardIcon(QStyle::SP_ComputerIcon);
        a.setWindowIcon(appIcon);
    }

    // 显示启动界面
    StartupSplash splash(appIcon);
    splash.showForMs(1500);

    //主界面显示和程序事件循环
    MainWindow w;
    w.setWindowTitle("MousePlan");
    w.show();
    return a.exec();
}
