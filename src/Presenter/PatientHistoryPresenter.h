#pragma once

#include "View/Widgets/PatientHistoryDialog.h"
#include "Model/Patient.h"
#include "Model/Dental/Snapshot.h"
#include "Model/TableRows.h"
#include "Database/DbDentalVisit.h"
#include <variant>
#include <vector>


class PatientHistoryPresenter {
	
	Patient& patient;

	std::vector<Procedure> local_history;

	std::vector<Snapshot> local_snapshots;

	std::vector<RowInstance> documents;

	std::vector<DbDentalVisit::VisitRecord> visits;

	PatientHistoryDialog view;

public:
	PatientHistoryPresenter(Patient& patient); //updates the history of the patient directly

	void openDocuments(const std::vector<int>& selectedDocIdx);

	//opens the visit (index in the visit history) in its dental visit tab
	void openVisit(int index);

	void toothHistoryRequested(int toothIdx);

	void openDialog(bool visitHistory = false);
};