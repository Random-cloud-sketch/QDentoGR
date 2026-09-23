#include "Resources.h"
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

std::string Resources::fromPath(const char* path)
{
	QFile file(path);
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	QTextStream in(&file);

	QString result = in.readAll();

	return result.toStdString();
}

//Default procedures and diagnoses inserted in a newly created database.
//Only the displayed names are translated - codes, types and the schema stay the same.
static QString translateSeedData(QString line)
{
	static const char* seedNames[] = {
		QT_TRANSLATE_NOOP("DbSeed", "Full Oral Examination"),
		QT_TRANSLATE_NOOP("DbSeed", "Restoration"),
		QT_TRANSLATE_NOOP("DbSeed", "Extraction"),
		QT_TRANSLATE_NOOP("DbSeed", "Crown"),
		QT_TRANSLATE_NOOP("DbSeed", "Bridge"),
		QT_TRANSLATE_NOOP("DbSeed", "Intraoral X-ray"),
		QT_TRANSLATE_NOOP("DbSeed", "Remove restoration"),
		QT_TRANSLATE_NOOP("DbSeed", "Implant"),
		QT_TRANSLATE_NOOP("DbSeed", "Full Cleaning"),
		QT_TRANSLATE_NOOP("DbSeed", "Adhesive Bridge"),
		QT_TRANSLATE_NOOP("DbSeed", "Remove post"),
		QT_TRANSLATE_NOOP("DbSeed", "Denture"),
		QT_TRANSLATE_NOOP("DbSeed", "Denture Pair"),
		QT_TRANSLATE_NOOP("DbSeed", "Post Core"),
		QT_TRANSLATE_NOOP("DbSeed", "Remove Crown/Bridge"),
		QT_TRANSLATE_NOOP("DbSeed", "Multiple Extraction"),
		QT_TRANSLATE_NOOP("DbSeed", "Root Canal Treatment"),
		QT_TRANSLATE_NOOP("DbSeed", "Apical Periodontitis"),
		QT_TRANSLATE_NOOP("DbSeed", "Caries"),
		QT_TRANSLATE_NOOP("DbSeed", "Gingivitis"),
		QT_TRANSLATE_NOOP("DbSeed", "Necrosis"),
		QT_TRANSLATE_NOOP("DbSeed", "Pulpitis")
	};

	if (!line.startsWith("INSERT INTO procedure_list") && !line.startsWith("INSERT INTO diagnosis")) {
		return line;
	}

	for (auto name : seedNames)
	{
		QString translated = QCoreApplication::translate("DbSeed", name);

		if (translated == name) continue;

		translated.replace("'", "''");

		line.replace("'" + QString(name) + "'", "'" + translated + "'");
	}

	return line;
}

std::vector<std::string> Resources::dbSchema() {

	std::vector<std::string> result;

	QFile inputFile(":/db/dbSchema.txt");
	if (inputFile.open(QIODeviceBase::ReadOnly))
	{
		QTextStream in(&inputFile);
		while (!in.atEnd())
        {

            result.emplace_back(translateSeedData(in.readLine()).toStdString());
		}
		inputFile.close();
	}
	return result;
}

std::vector<std::string> Resources::getMigrationScript(int version)
{
	std::vector<std::string> result;
	//the file has to be included in qrc
	QString path = ":/migrations/db_migrateTo";
	path += QString::number(version);
	path += ".txt";

	QFile inputFile(path);
	if (inputFile.open(QIODevice::ReadOnly))
	{
		QTextStream in(&inputFile);
		while (!in.atEnd())
		{
            result.emplace_back(in.readLine().toStdString());
		}
		inputFile.close();
	}
	return result;
}

