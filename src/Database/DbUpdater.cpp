#include "DbUpdater.h"

#include <QObject>
#include <QFile>

#include "Database/Database.h"
#include "View/ModalDialogBuilder.h"
#include "Version.h"
#include "Resources.h"
#include "GlobalSettings.h"

inline int currentVersion = 11;

void backupDatabase()
{
	QFile::copy(Db::getFilePath().c_str(), GlobalSettings::getDbBackupFilepath().c_str());
}

void commonUpdate(int toVersion) {

	if (Db::version() != toVersion-1) return;

	Db db;
	
	for (auto& query : Resources::getMigrationScript(toVersion))
	{
		db.execute(query);
	}

}

void DbUpdater::updateDb()
{
	if (Db::version() == currentVersion) return;

	if (Db::version() > currentVersion) {

		ModalDialogBuilder::showMessage(
			QObject::tr("Database version is older than the software version. Please update software!").toStdString()
		);

		return;
	}

	backupDatabase();
	
	commonUpdate(1);
	commonUpdate(2);
	commonUpdate(3);
	commonUpdate(4);
	commonUpdate(5);
	commonUpdate(6);
	commonUpdate(7);
	commonUpdate(8);
	commonUpdate(9);
	commonUpdate(10);
	commonUpdate(11);
}
