#pragma once
#include <memory>

#include "Model/Patient.h"

#include "Model/TabType.h"

class TabInstance;
class PatientTileInfo;

struct PatientInfoPresenter
{
	std::shared_ptr<Patient> patient;
	PatientTileInfo* view;

	Date doc_date;

	bool m_isCurrent{ false };

	TabInstance* m_parent{ nullptr };

public:
	PatientInfoPresenter(PatientTileInfo* view, std::shared_ptr<Patient> p);
	void setDate(const Date& date);
	void patientTileClicked();
	void appointmentClicked();
	void notesRequested();
	void setCurrent(bool current);
    void notificationClicked();
    void openDocument(TabType type);
	void medicalHistoryRequested();
	void refreshMedicalHistory();
	void patientFilesRequested();
	void refreshPatientFiles();
	void visitHistoryRequested();
	void refreshVisitCount();
	void recallRequested();
	void refreshRecall();
	void setParent(TabInstance* p) { m_parent = p; }
};
