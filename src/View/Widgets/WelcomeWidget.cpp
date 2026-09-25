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

    //invoices are not used for now: the tile is only hidden (delete this block to show it again).
    //The tiles have fixed positions: "Browse Documents" takes the place of the hidden tile
    //and the remaining 3 x 2 tiles are centred (half a column to the right).
    {
        ui.invoiceButton->setVisible(false);
        ui.label_3->setVisible(false);

        ui.browser->move(ui.invoiceButton->pos());
        ui.label_4->move(ui.label_3->pos());

        int halfColumn = (ui.browser->x() - ui.perioButton->x()) / 2;

        for (auto w : ui.frame->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            w->move(w->x() + halfColumn, w->y());
        }
    }
    connect(ui.browser, &QPushButton::clicked, this, [&] { MainPresenter::get().showBrowser(); });
    connect(ui.settingsButton, &QPushButton::clicked, this, [&] { MainPresenter::get().settingsPressed(); });
    connect(ui.calendar, &QPushButton::clicked, this, [&] { MainPresenter::get().openCalendar(); });
    connect(ui.notifButton, &QPushButton::clicked, this, [&] { MainPresenter::get().notificationPressed(); });
}

WelcomeWidget::~WelcomeWidget()
{}
