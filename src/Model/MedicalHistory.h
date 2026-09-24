#pragma once
#include <string>
#include <vector>
#include <map>

#include <QCoreApplication>

#include "Model/Date.h"

//Yes/No questions of the medical history (anamnesis).
//The key is stored in the database, the label is shown to the user.
namespace MedicalHistoryItems
{
	enum class Section { Condition, Allergy, Risk, Lifestyle, Family, Dental };

	struct Definition
	{
		const char* key;
		const char* label;
		Section section;
	};

	const std::vector<Definition>& all();
	std::vector<Definition> ofSection(Section section);
	QString label(const Definition& d);
}

struct MedicalHistoryAnswer
{
	enum Value { NotAnswered = 0, No = 1, Yes = 2 };

	int value{ NotAnswered };
	std::string details;

	bool operator==(const MedicalHistoryAnswer&) const = default;
};

struct Medication
{
	std::string name;
	std::string dose;
	std::string frequency;
	std::string indication;
	std::string notes;

	bool operator==(const Medication&) const = default;
};

struct PastSurgery
{
	std::string surgery;
	std::string date;
	std::string reason;
	std::string hospital;
	std::string notes;

	bool operator==(const PastSurgery&) const = default;
};

//One saved version of the patient's medical history.
//Every save creates a new version, so the previous ones are preserved.
struct MedicalHistory
{
	Q_DECLARE_TR_FUNCTIONS(MedicalHistory)

public:
	enum GeneralHealth { HealthNotSpecified = 0, Good, Fair, Poor };
	enum Smoking { SmokingNotSpecified = 0, NonSmoker, FormerSmoker, Smoker };

	long long rowid{ 0 };
	long long patientRowid{ 0 };
	long long dentistRowid{ 0 };
	std::string saved; //ISO 8601 date and time of this version

	Date date{ Date::currentDate() }; //date on which the history was taken
	int generalHealth{ HealthNotSpecified };
	std::string physician;
	std::string physicianContact;

	std::map<std::string, MedicalHistoryAnswer> answers;
	std::vector<Medication> medications;
	std::vector<PastSurgery> surgeries;

	std::string allergyReaction;

	int smoking{ SmokingNotSpecified };
	int cigarettesPerDay{ 0 };
	int smokingYears{ 0 };
	std::string smokingQuit;
	std::string lifestyleOther;

	std::string oralHygiene;
	std::string dentalOther;

	std::string notes;

	MedicalHistoryAnswer answer(const std::string& key) const;

	//Warnings built only from what was entered: allergies and bleeding/medical risk items answered "Yes"
	QStringList alerts(bool riskDetails = true) const;
	//Medical conditions answered "Yes"
	QStringList conditions() const;
	//Read-only summary of everything that was entered
	QString toHtml() const;

	static QString savedToLocalFormat(const std::string& saved);
	//Human readable list of the changes between two versions
	static QStringList differences(const MedicalHistory& before, const MedicalHistory& after);
};
