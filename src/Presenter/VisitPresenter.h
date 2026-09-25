#pragma once

#include <vector>
#include <memory>
#include "Database/DbDentalVisit.h"

#include "Presenter/TabInstance.h"
#include "SurfacePanelPresenter.h"
#include "CheckModel.h"


#include "Presenter/PatientInfoPresenter.h"

typedef std::vector<int> SelectedTeethIdx;

class VisitView;
class TabView;

class VisitPresenter : public TabInstance
{
    SurfacePanelPresenter surf_presenter;
    PatientInfoPresenter patient_info;

    SelectedTeethIdx m_selectedIndexes;

    CheckModel m_checkModel;
    CheckModel m_dsnCheckModel;

    VisitView* view;

    void prepareDerivedForSwitch() override {
        patient_info.setCurrent(false);
    }

    bool isValid();
    void refreshProcedureView();
    void statusChanged();

public:

    DentalVisit m_visit;

    VisitPresenter(TabView* tabView, std::shared_ptr<Patient> patient, long long rowId = 0);

    long long rowID() const override;
    bool save() override;
    bool isNew() override;
    TabName getTabName() override;
    void setDataToView() override;

    void setAmbDate(const Date& date);

    //recalculates the number of the visit from the database (other visits may have been saved or deleted)
    void refreshVisitNumber();

    void setToothStatus(Dental::StatusType t, int code, bool supernumeral = false);
    void setOther(int code);

    void setSelectedTeeth(const std::vector<int>& SelectedIndexes);
    
    void historyRequested();

    void openDetails(int toothIdx);
    void openDetails();

    void addProcedure();
    void editProcedure(int index);
    void deleteProcedure(int index);
    void moveProcedure(int from, int to);

    void showAppliedStatus();

    void createInvoice();
    void createPerioMeasurment();

    ~VisitPresenter();
};

