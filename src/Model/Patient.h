#pragma once
#include <string>
#include <vector>
#include <array>

#include "Date.h"

typedef std::array<std::string, 32>TeethNotes;

struct Patient
{
	enum Sex { Male = 0, Female = 1 };

	long long rowid{ 0 };

	//"Identifier": permanent UUID of the patient (e.g. 7f3c9b2e-6e2a-4c91-9b7a-3a1f8d52c614),
	//assigned once when the patient is created and never changed afterwards.
	//It is the patient's only identifier outside the database (e.g. the periodontal chart application).
	std::string id;
	Date birth;
	Sex sex{ Male };

	std::string firstName;
	std::string lastName;
	std::string address;
	std::string phone;
	std::string referringDoctor;

	TeethNotes teethNotes;
	std::string patientNotes;

	std::string colorNameRgb;

	//a new random identifier (lowercase UUID without braces)
	static std::string newId();

	int getAge(const Date& currentDate = Date::currentDate())  const;
	std::string firstLastName() const;

	~Patient();
};
