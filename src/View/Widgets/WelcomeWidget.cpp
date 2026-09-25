#include "WelcomeWidget.h"
#include "View/Theme.h"
#include "Presenter/MainPresenter.h"
#include <QDate>

WelcomeWidget::WelcomeWidget(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);

    auto date = Date::currentDate();

    ui.cornerLabel->setPixmap(QPixmap(":/icons/qDento.png"));

    setStyleSheet("color: " + Theme::colorToString(Theme::fontTurquoise) + "; background-color:" + Theme::colorToString(Theme::background));

    ui.ambButton->setIcon(QIcon(":/icons/icon_sheet.png"));
    ui.perioButton->setIcon(QIcon(":/icons/icon_periosheet.png"));
    ui.invoiceButton->setIcon(QIcon(":/icons/icon_invoice.png"));
    ui.browser->setIcon(QIcon(":/icons/icon_open.png"));
    ui.settingsButton->setIcon(QIcon(":/icons/icon_settings.png"));
    ui.calendar->setIcon(QIcon(":/icons/icon_calendar.png"));
    ui.notifButton->setIcon(QIcon(":/icons/icon_bell.png"));

    connect(ui.ambButton, &QPushButton::clicked, this, [&] { MainPresenter::get().newAmbPressed(); });
    connect(ui.perioButton, &QPushButton::clicked, this, [&] { MainPresenter::get().newPerioPressed(); });
    connect(ui.invoiceButton, &QPushButton::clicked, this, [&] { MainPresenter::get().newInvoicePressed(); });
    connect(ui.browser, &QPushButton::clicked, this, [&] { MainPresenter::get().showBrowser(); });
    connect(ui.settingsButton, &QPushButton::clicked, this, [&] { MainPresenter::get().settingsPressed(); });
    connect(ui.calendar, &QPushButton::clicked, this, [&] { MainPresenter::get().openCalendar(); });
    connect(ui.notifButton, &QPushButton::clicked, this, [&] { MainPresenter::get().notificationPressed(); });
}

WelcomeWidget::~WelcomeWidget()
{}
