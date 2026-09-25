#include "MainPresenter.h"
#include "GoogleCalendar/GoogleCalendarSync.h"
#include "Presenter/RecallNotifier.h"


#include "Model/User.h"
#include "GlobalSettings.h"

#include "Database/DbCompany.h"
#include "Database/DbDentist.h"
#include "Database/DbNotification.h"
#include "Database/Database.h"

#include "Presenter/PatientDialogPresenter.h"
#include "Presenter/LoginPresenter.h"
#include "Presenter/RecipientPresenter.h"
#include "Presenter/NewDocPresenter.h"
#include "Presenter/CalendarPresenter.h"

#include "View/Widgets/BrowserDialog.h"
#include "View/Widgets/SplashScreen.h"
#include "View/Widgets/NotificationListDialog.h"
#include "View/Widgets/QDento.h"

MainPresenter MainPresenter::s_singleton;

void MainPresenter::setView(QDento* view)
{
    this->view = view;

    firstTimeLogic();

    LoginPresenter login;
    
    auto autologinLPK = DbDentist::getDoctorAutoLoginRowid();
    
    if (autologinLPK) {
        login.login(autologinLPK);
        view->m_loggedIn = true;
    }
    else
    {
        view->m_loggedIn = login.successful();
    }

    if (!view->m_loggedIn) return;
    
    view->setNotificationIcon(DbNotification::hasNotifications());

    User::setCurrentCompany(DbCompany::getInvoiceData());

    User::ADA_num = GlobalSettings::isADANum();

    view->setUserLabel(
        User::dentist().getFullName(),
        User::company().name
    );

    //the application starts in the calendar
    openCalendar();

    //Google Calendar synchronization of the appointments (if it is connected)
    GoogleCalendarSync::get().start();

    //periodontal recalls which need attention: badge and the notice of the day
    RecallNotifier::get().start();
}

void MainPresenter::newAmbPressed()
{

    NewDocPresenter p(QObject::tr("New Dental Visit").toStdString());

    auto patient = p.open();

    if (patient.has_value())
        TabPresenter::get().openList(patient.value());
}

void MainPresenter::newPerioPressed()
{
    NewDocPresenter p(QObject::tr("New Periodontal Measurment").toStdString());

    auto patient = p.open();

    if (patient.has_value())
        TabPresenter::get().openPerio(patient.value());
}

void MainPresenter::newInvoicePressed()
{
    RecipientPresenter p;

    auto result = p.openDialog();

    if (!result.has_value()) {
        return;
    }

    TabPresenter::get().openInvoice(result.value());
}

void MainPresenter::notificationPressed()
{
    NotificationListDialog d;
    d.exec();

    view->setNotificationIcon(DbNotification::hasNotifications());
}

void MainPresenter::openCalendar()
{
    TabPresenter::get().openCalendar();
}

void MainPresenter::openRecall()
{
    TabPresenter::get().openRecall();
}

void MainPresenter::showBrowser()
{
    BrowserDialog d;
    d.exec();
}

void MainPresenter::settingsPressed()
{
    ModalDialogBuilder::openSettingsDialog(ModalDialogBuilder::SettingsTab::General);

    view->setUserLabel(
        User::dentist().getFullName(),
        User::company().name
    );

    //refreshing the data to the view (e.g. issuer in invoices)
    auto currentTab = TabPresenter::get().currentTab();
    if (currentTab) {
        currentTab->setCurrent();
    }
    
}

bool MainPresenter::save() 
{
    if(TabPresenter::get().currentTab())
        return TabPresenter::get().currentTab()->save();

    return true;
}

void MainPresenter::logOut()
{
    if (!closeAllTabs()) return;

    view->setUserLabel("", "");
   
    DbDentist::setAutoLogin(User::dentist().rowID, false);

    view->setNotificationIcon(0);
    view->setRecallBadge(0, 0);

    LoginPresenter login;

    if (login.successful() == false)
    {
        view->exitProgram();
        return;
    }

    view->setUserLabel(
        User::dentist().getFullName(),
        User::company().name
        );

    view->setNotificationIcon(DbNotification::hasNotifications());

    openCalendar();

    GoogleCalendarSync::get().start();

    RecallNotifier::get().start();
}

void MainPresenter::userSettingsPressed()
{
    ModalDialogBuilder::openSettingsDialog(ModalDialogBuilder::SettingsTab::Dentist);

    view->setUserLabel(
        User::dentist().getFullName(),
        User::company().name
    );

    if (!User::isValid()) return;

}

bool MainPresenter::closeAllTabs()
{
    return TabPresenter::get().permissionToLogOut();
}

void MainPresenter::firstTimeLogic()
{
    if (DbCompany::noInvoiceData()) {
        Company p;
        p.name = "QDento";
        p.currency = "$";
        DbCompany::insertInvoiceData(p);
    }

    if (DbDentist::getList().empty()) {

        Dentist d;
        d.fname = "John";
        d.lname = "Smith";

        d.rowID = DbDentist::insert(d);

        DbDentist::setAutoLogin(d.rowID, true);

        ModalDialogBuilder::showMessage(
            QObject::tr("A placeholder profile has been created. "
            "Enter your credentials from settings").toStdString()
        );
    }
}
