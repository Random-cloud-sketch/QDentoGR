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

	int getAge(const Date& currentDate = Date::currentDate())  const;
	std::string firstLastName() const;

	~Patient();
};
