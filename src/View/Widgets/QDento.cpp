#include "QDento.h"

#include <QAction>
#include <QMenu>

#include <QPixmap>
#include <QFileDialog>
#include <QFontDatabase>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>

#include "Presenter/MainPresenter.h"

#include "Model/User.h"
#include "View/Theme.h"
#include "View/Widgets/GlobalWidgets.h"
#include "View/Widgets/SplashScreen.h"
#include "View/Widgets/NotificationListDialog.h"

#include "Version.h"

#include "Database/DbNotification.h"
#include "Presenter/RecallNotifier.h"

#include <QLabel>

#ifdef Q_OS_WIN
#include <QWindow>
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

QDento::QDento(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    setWindowState(Qt::WindowMaximized);

#ifdef Q_OS_WIN
//change the default titlebar (windows 11 only)

/*
    auto& tbClr = Theme::mainBackgroundColor;
    auto tbTxt = QColor(Qt::black);

    const COLORREF rgb = RGB(tbClr.red(), tbClr.green(), tbClr.blue());

    const DWORD dwmCaptionAttr = 35;
    const DWORD dwmTextAttr = 36;

	auto hwnd = reinterpret_cast<HWND>(windowHandle()->winId());

    DwmSetWindowAttribute(hwnd, dwmCaptionAttr, &rgb, sizeof(rgb));

    const COLORREF white = RGB(tbTxt.red(), tbTxt.green(), tbTxt.blue());
    DwmSetWindowAttribute(hwnd, dwmTextAttr, &white, sizeof(white));
 */
#endif

    GlobalWidgets::mainWindow = this;
    GlobalWidgets::statusBar = statusBar();

    statusBar()->setStyleSheet("font-weight: bold; color:" + Theme::colorToString(Theme::fontTurquoiseClicked));
    statusBar()->setHidden(true);

    QAction* settingsAction = new QAction(tr("Settings"));
    settingsAction->setIcon(QIcon(":/icons/icon_settings.png"));
    QAction* exitAction = new QAction(tr("Exit"));
    exitAction->setIcon(QIcon(":/icons/icon_remove.png"));
    QMenu* userMenu = new QMenu(this);
    userMenu->addAction(settingsAction);
    userMenu->addAction(exitAction);
    userMenu->setStyleSheet(Theme::getPopupMenuStylesheet());

    //setting global shortcuts
    auto shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, [&] { MainPresenter::get().save(); });

    shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_O), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, [&] { MainPresenter::get().showBrowser(); });

    shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_N), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, [&] { MainPresenter::get().newAmbPressed(); });

    shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_M), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, [&] { MainPresenter::get().newPerioPressed(); });

    shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, [&] { MainPresenter::get().newInvoicePressed(); });

    //invoices are not used for now: the shortcut is only disabled (delete this line to enable it again)
    shortcut->setEnabled(false);

    //setting buttons
    ui.newButton->setIcon(QIcon(":/icons/icon_sheet.png"));
    ui.perioButton->setIcon(QIcon(":/icons/icon_periosheet.png"));
    ui.saveButton->setIcon(QIcon(":/icons/icon_save.png"));
    ui.browserButton->setIcon(QIcon(":/icons/icon_open.png"));
    ui.settingsButton->setIcon(QIcon(":/icons/icon_settings.png"));
    ui.calendarButton->setIcon(QIcon(":/icons/icon_calendar.png"));
    ui.recallButton->setIcon(QIcon(":/icons/icon_sync.png"));
    ui.invoiceButton->setIcon(QIcon(":/icons/icon_invoice.png"));
    ui.notifButton->setIcon(QIcon(":/icons/icon_bell.png"));
    ui.notifButton->setMonochrome(true);
     
    connect(ui.newButton, &QPushButton::clicked, [&] { MainPresenter::get().newAmbPressed(); });
    connect(ui.saveButton, &QPushButton::clicked, [&] { MainPresenter::get().save(); });
    connect(ui.browserButton, &QPushButton::clicked, [&] { MainPresenter::get().showBrowser(); });
    connect(ui.perioButton, &QPushButton::clicked, [&] { MainPresenter::get().newPerioPressed(); });
    connect(ui.calendarButton, &QPushButton::clicked, [&] { MainPresenter::get().openCalendar(); });
    connect(ui.recallButton, &QPushButton::clicked, [&] { MainPresenter::get().openRecall(); });

    //badge over the corner of the recall button: a child of the button, so the layout never changes
    m_recallBadge = new QLabel(ui.recallButton);
    m_recallBadge->setAlignment(Qt::AlignCenter);
    m_recallBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_recallBadge->setStyleSheet(
        "QLabel { background-color: rgb(215, 45, 45); color: white; font-weight: bold; font-size: 9px;"
        " border-radius: 7px; padding: 0px 3px; }"
    );
    m_recallBadge->hide();

    connect(&RecallNotifier::get(), &RecallNotifier::changed, this, [this](int count, int leadDays) { setRecallBadge(count, leadDays); });
    connect(settingsAction, &QAction::triggered, [&] { MainPresenter::get().userSettingsPressed();});
    connect(ui.settingsButton, &QPushButton::clicked, [&] { MainPresenter::get().settingsPressed();});
    connect(ui.invoiceButton, &QPushButton::clicked, [&] { MainPresenter::get().newInvoicePressed(); });

    //invoices are not used for now: the button is only hidden (delete this line to show it again)
    ui.invoiceButton->setVisible(false);
	connect(ui.userButton, &QPushButton::clicked, [&] { ui.userButton->showMenu(); });
    connect(exitAction, &QAction::triggered, [&] { MainPresenter::get().logOut(); });

    connect(ui.notifButton, &QPushButton::clicked, this, [&]{ MainPresenter::get().notificationPressed();});

    ui.userButton->setMenu(userMenu);
    ui.userButton->setIconSize(QSize(25, 25));

    ui.userButton->setIcon(QIcon{":/icons/icon_user.png"});

    SplashScreen::hideAndDestroy();

    ui.tabView->showWelcomeScreen();

    MainPresenter::get().setView(this);

}

TabView* QDento::tabView()
{
    return ui.tabView;
}

void QDento::setUserLabel(const std::string& doctorName, const std::string& practiceName)
{
    ui.userButton->setText(QString::fromStdString(doctorName));

    QString title = "QDento v";
    title += Version::current().toString().c_str();
    title += " ";
    title += practiceName.c_str();

    setWindowTitle(title);
}

void QDento::exitProgram()
{
    QApplication::quit();
}

bool QDento::initialized()
{
    return m_loggedIn;
}

void QDento::disableButtons(bool saveDisabled)
{
    ui.saveButton->setDisabled(saveDisabled);
}

void QDento::paintEvent(QPaintEvent*)
{
    QPainter painter;
    painter.begin(this);
    painter.fillRect(rect(), Theme::mainBackgroundColor);
    painter.fillRect(0, height() - 21, width(), 21, QColor(240, 240, 240));
    painter.end();
}

void QDento::closeEvent(QCloseEvent* event)
{
    if (!MainPresenter::get().closeAllTabs())
        event->ignore();

    foreach(QWidget * widget, QApplication::topLevelWidgets()) 
    {
        if (widget == this) continue;
        widget->close();
    }
}

void QDento::setRecallBadge(int count, int leadDays)
{
    if (count <= 0) {
        m_recallBadge->hide();
        ui.recallButton->setToolTip(tr("Periodontal recall"));
        return;
    }

    m_recallBadge->setText(count > 99 ? "99+" : QString::number(count));

    //fixed height, the width grows with the number; kept inside the top right corner of the button
    int width = std::max(14, m_recallBadge->fontMetrics().horizontalAdvance(m_recallBadge->text()) + 8);
    width = std::min(width, ui.recallButton->width());
    m_recallBadge->setGeometry(ui.recallButton->width() - width, 0, width, 14);
    m_recallBadge->show();
    m_recallBadge->raise();

    QString tip;

    if (leadDays > 0) {
        tip = count == 1 ?
            tr("1 patient with a recall due or due within %1 without an appointment").arg(RecallText::withinDays(leadDays)) :
            tr("%1 patients with a recall due or due within %2 without an appointment").arg(count).arg(RecallText::withinDays(leadDays));
    }
    else {
        tip = count == 1 ?
            tr("1 patient is due for recall without an appointment") :
            tr("%1 patients are due for recall without an appointment").arg(count);
    }

    ui.recallButton->setToolTip(tip);
}

void QDento::setNotificationIcon(int activeNotifCount)
{
    ui.notifButton->setMonochrome(!activeNotifCount);
    ui.notifButton->setIcon(activeNotifCount ? QIcon(":/icons/icon_bell_notify.png") : QIcon(":/icons/icon_bell.png"));

    switch(activeNotifCount){
        case 0: ui.notifButton->setToolTip(tr("No active reminders")); break;
        case 1: ui.notifButton->setToolTip(tr("1 active reminder")); break;
        default: ui.notifButton->setToolTip(QString::number(activeNotifCount) + tr(" active reminders"));
    }
}

QDento::~QDento()
{
    Theme::cleanUpFusionStyle();
}
