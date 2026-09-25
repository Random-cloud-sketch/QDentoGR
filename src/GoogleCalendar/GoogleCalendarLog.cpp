#include "GoogleCalendarLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "GlobalSettings.h"

void GoogleCalendarLog::write(const QString& message)
{
	QDir folder(QString::fromStdString(GlobalSettings::getDataFolder()));

	if (!folder.exists()) folder.mkpath(".");

	QString path = folder.filePath("google_calendar.log");

	//the log is kept small: the previous part is kept as .old
	if (QFileInfo(path).size() > 1024 * 1024) {
		QFile::remove(path + ".old");
		QFile::rename(path, path + ".old");
	}

	QFile file(path);

	if (!file.open(QIODevice::Append | QIODevice::Text)) return;

	QTextStream out(&file);
	out << QDateTime::currentDateTime().toString(Qt::ISODate) << " [GoogleCalendar] " << message << "\n";
}
