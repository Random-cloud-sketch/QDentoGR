#pragma once
#include <string>

#include <QCoreApplication>
#include <QString>

//A radiograph, CBCT/DICOM file or document attached to a patient.
//The file itself is kept in the patient's storage folder, the metadata in the database.
struct PatientFile
{
	Q_DECLARE_TR_FUNCTIONS(PatientFile)

public:
	//stored in the database - do not reorder
	enum Type { Radiograph = 0, Cbct = 1, Photo = 2, Pdf = 3, Document = 4, Other = 5, TypeCount };

	long long rowid{ 0 };
	long long patientRowid{ 0 };
	long long dentistRowid{ 0 };

	std::string storedName;    //unique file name inside the patient's storage folder
	std::string originalName;  //file name at the time of the upload
	std::string name;          //name shown to the user (can be renamed)
	int type{ Other };
	long long size{ 0 };
	std::string uploaded;      //ISO 8601 date and time
	std::string description;

	QString suffix() const;

	static Type detectType(const QString& fileName);
	static QString typeName(int type);
	static QString fileDialogFilter();
};
