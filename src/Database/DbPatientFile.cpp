#include "DbPatientFile.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include "Database/Database.h"
#include "Model/User.h"

static const char* columns =
	"rowid, patient_rowid, dentist_rowid, stored_name, original_name, name, type, size, uploaded, description";

static PatientFile readFile(Db& db)
{
	PatientFile f;

	f.rowid = db.asRowId(0);
	f.patientRowid = db.asRowId(1);
	f.dentistRowid = db.asRowId(2);
	f.storedName = db.asString(3);
	f.originalName = db.asString(4);
	f.name = db.asString(5);
	f.type = db.asInt(6);
	f.size = db.asLongLong(7);
	f.uploaded = db.asString(8);
	f.description = db.asString(9);

	return f;
}

QString DbPatientFile::storageFolder(long long patientRowid)
{
	QDir dbFolder = QFileInfo(QString::fromStdString(Db::getFilePath())).absoluteDir();

	return dbFolder.filePath("patient_files/" + QString::number(patientRowid));
}

QString DbPatientFile::filePath(const PatientFile& file)
{
	return QDir(storageFolder(file.patientRowid)).filePath(QString::fromStdString(file.storedName));
}

std::vector<PatientFile> DbPatientFile::getFiles(long long patientRowid)
{
	std::vector<PatientFile> result;

	Db db(
		std::string("SELECT ") + columns + " FROM patient_file "
		"WHERE patient_rowid = " + std::to_string(patientRowid) + " ORDER BY uploaded DESC, rowid DESC"
	);

	while (db.hasRows()) result.push_back(readFile(db));

	return result;
}

int DbPatientFile::count(long long patientRowid)
{
	Db db("SELECT COUNT(*) FROM patient_file WHERE patient_rowid = " + std::to_string(patientRowid));

	int result = 0;

	while (db.hasRows()) result = db.asInt(0);

	return result;
}

std::optional<PatientFile> DbPatientFile::importFile(long long patientRowid, const QString& sourcePath)
{
	QFileInfo source(sourcePath);

	if (!source.exists() || !source.isFile() || patientRowid <= 0) return {};

	auto folder = storageFolder(patientRowid);

	if (!QDir().mkpath(folder)) return {};

	PatientFile f;
	f.patientRowid = patientRowid;
	f.dentistRowid = User::dentist().rowID;
	f.originalName = source.fileName().toStdString();
	f.name = source.completeBaseName().toStdString();
	f.type = PatientFile::detectType(source.fileName());
	f.uploaded = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();

	//unique name inside the folder, the original extension is kept so the file opens with the right program
	auto storedName = QUuid::createUuid().toString(QUuid::WithoutBraces);
	if (source.suffix().size()) storedName += "." + source.suffix().toLower();
	f.storedName = storedName.toStdString();

	auto destination = filePath(f);

	if (!QFile::copy(sourcePath, destination)) return {};

	//the copy must be complete before the file is registered
	QFileInfo copied(destination);

	if (copied.size() != source.size()) {
		QFile::remove(destination);
		return {};
	}

	f.size = copied.size();

	//stored files are managed by the application
	QFile::setPermissions(destination, QFile::permissions(destination) | QFileDevice::WriteOwner);

	Db db(
		"INSERT INTO patient_file "
		"(patient_rowid, dentist_rowid, stored_name, original_name, name, type, size, uploaded, description) "
		"VALUES (?,?,?,?,?,?,?,?,?)"
	);

	db.bind(1, f.patientRowid);
	if (f.dentistRowid) db.bind(2, f.dentistRowid); else db.bindNull(2);
	db.bind(3, f.storedName);
	db.bind(4, f.originalName);
	db.bind(5, f.name);
	db.bind(6, f.type);
	db.bind(7, f.size);
	db.bind(8, f.uploaded);
	db.bind(9, f.description);

	if (!db.execute()) {
		QFile::remove(destination);
		return {};
	}

	f.rowid = db.lastInsertedRowID();

	return f;
}

bool DbPatientFile::update(const PatientFile& f)
{
	Db db("UPDATE patient_file SET name=?, type=?, description=? WHERE rowid=?");

	db.bind(1, f.name);
	db.bind(2, f.type);
	db.bind(3, f.description);
	db.bind(4, f.rowid);

	return db.execute();
}

bool DbPatientFile::remove(const PatientFile& f)
{
	Db db("DELETE FROM patient_file WHERE rowid=?");

	db.bind(1, f.rowid);

	if (!db.execute()) return false;

	auto path = filePath(f);

	return !QFile::exists(path) || QFile::remove(path);
}

bool DbPatientFile::exportFile(const PatientFile& f, const QString& destinationPath)
{
	if (QFile::exists(destinationPath) && !QFile::remove(destinationPath)) return false;

	return QFile::copy(filePath(f), destinationPath);
}

void DbPatientFile::removePatientFolder(long long patientRowid)
{
	if (patientRowid <= 0) return;

	QDir folder(storageFolder(patientRowid));

	if (folder.exists()) folder.removeRecursively();
}
