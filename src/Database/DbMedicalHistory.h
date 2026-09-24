#pragma once
#include <optional>
#include <vector>
#include <string>

#include "Model/MedicalHistory.h"

namespace DbMedicalHistory
{
	struct VersionInfo
	{
		long long rowid{ 0 };
		std::string saved;
		long long dentistRowid{ 0 };
	};

	//the latest version of the patient's medical history
	std::optional<MedicalHistory> getCurrent(long long patientRowid);
	MedicalHistory get(long long rowid);
	//all saved versions, the newest first
	std::vector<VersionInfo> getVersions(long long patientRowid);
	//saves the history as a new version (previous versions are kept); returns its rowid or 0
	long long insertVersion(const MedicalHistory& history);
}
