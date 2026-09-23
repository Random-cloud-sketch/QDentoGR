#include "CustomDateTimeEdit.h"
#include "CalendarWidget.h"
#include <QLineEdit>
#include "GlobalSettings.h"

CustomDateTimeEdit::CustomDateTimeEdit(QWidget* parent) : QDateTimeEdit(parent)
{
    setDate(QDate::currentDate());
    setDisplayFormat("dd.MM.yyyy HH:mm");
    setLocale(GlobalSettings::isGreekUi() ? QLocale(QLocale::Greek, QLocale::Greece) : QLocale(QLocale::Bulgarian));
    setCalendarPopup(true);
    setCalendarWidget(new CalendarWidget(this));
    lineEdit()->setFrame(false);
}

CustomDateTimeEdit::~CustomDateTimeEdit()
{

}

