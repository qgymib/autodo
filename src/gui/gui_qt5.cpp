#include <QApplication>
#include <QDialog>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QAction>
#include <QMenu>
#include "gui.h"
#include "utils.h"

class AutoMainWindow : public QDialog
{
    Q_OBJECT

public:
    AutoMainWindow(auto_gui_startup_info_t* info)
    {
        m_info = info;
        m_tray_icon = new QSystemTrayIcon(QIcon(":/appicon.png"), this);

        QAction* quit_action = new QAction("Exit", m_tray_icon);
        QObject::connect(quit_action, &QAction::triggered, this, &AutoMainWindow::OnExit);
        QObject::connect(this, &AutoMainWindow::RequireExit, this, &AutoMainWindow::OnExit);

        m_tray_icon_menu = new QMenu;
        m_tray_icon_menu->addAction(quit_action);
        m_tray_icon->setContextMenu(m_tray_icon_menu);

        m_tray_icon->show();
    }
    virtual ~AutoMainWindow()
    {
        delete m_tray_icon;
        delete m_tray_icon_menu;
    }

signals:
    void RequireExit();

public:
    void OnExit()
    {
        auto_gui_msg_t msg;
        msg.event = AUTO_GUI_QUIT;
        m_info->on_event(&msg, m_info->udata);

        QApplication::exit(0);
    }

private:
    QSystemTrayIcon*            m_tray_icon;
    QMenu*                      m_tray_icon_menu;
    auto_gui_startup_info_t*    m_info;
};

#include "gui_qt5.moc"

static AutoMainWindow* m_main_window;

int auto_gui(auto_gui_startup_info_t* info)
{
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(info->argc, info->argv);

    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        QMessageBox::critical(nullptr, QObject::tr("Systray"),
                              QObject::tr("I couldn't detect any system tray "
                                          "on this system."));
        return 1;
    }

    QApplication::setQuitOnLastWindowClosed(false);

    AutoMainWindow main_window(info);
    m_main_window = &main_window;
    //main_window.show();

    {
        auto_gui_msg_t msg;
        msg.event = AUTO_GUI_READY;
        info->on_event(&msg, info->udata);
    }

    return app.exec();
}

void auto_gui_exit(void)
{
    m_main_window->RequireExit();
}
