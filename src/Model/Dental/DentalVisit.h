#pragma once
#include <string>
#include <array>
#include <vector>
#include <algorithm>

#include "Model/Patient.h"
#include "Model/Date.h"
#include "ToothContainer.h"
#include "Model/Dental/ProcedureContainer.h"
#include "Model/FreeFunctions.h"

struct DentalVisit
{
	DentalVisit() {}

	long long rowid{ 0 };
	long long patient_rowid{ 0 };
	
	Date date = Date::currentDate().to8601();

	//chronological number of the visit among the patient's visits (calculated, never entered by the user);
	//for a visit which is not saved yet: the number of the patient's saved visits
	int number{ 0 };
	std::string dentist_rowid;

	ToothContainer teeth;
	ProcedureContainer procedures;

	bool isNew() const { return rowid == 0; }

	std::string getNumber() const {
		return FreeFn::leadZeroes(number, 6);
	}

	~DentalVisit() {  }
};