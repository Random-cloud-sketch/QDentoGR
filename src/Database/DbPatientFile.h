#pragma once
#include <optional>
#include <vector>

#include <QString>

#include "Model/PatientFile.h"

//Radiographs and documents of the patients.
//The files are copied in a "patient_files" folder next to the database file
//(one subfolder per patient), the metadata is stored in the patient_file table.
namespace DbPatientFile
{
	std::vector<PatientFile> getFiles(long long patientRowid);
	int count(long long patientRowid);

	//copies the file in the patient's storage folder and registers it; returns the new record or nothing on failure
	std::optional<PatientFile> importFile(long long patientRowid, const QString& sourcePath);
	//name, type and description
	bool update(const PatientFile& file);
	//removes the record and the stored file
	bool remove(const PatientFile& file);
	//copies the stored file to the given location
	bool exportFile(const PatientFile& file, const QString& destinationPath);

	QString storageFolder(long long patientRowid);
	QString filePath(const PatientFile& file);
	//called when the patient is deleted
	void removePatientFolder(long long patientRowid);
}
