#pragma once

#include <unordered_set>
#include <vector>
#include "Model/Dental/DentalVisit.h"

namespace DbDentalVisit
{
    //One row of the patient's visit history
    struct VisitRecord
    {
        long long rowid{ 0 };
        int number{ 0 };            //chronological number of the visit
        Date date;
        std::string time;           //start of the patient's appointment on that day, if there is one
        std::string description;    //procedures of the visit and their notes
        bool permissionToOpen{ true };
    };

    //SQL expression: chronological number of the dental_visit row named visitAlias among the visits
    //of its patient - saved visits ordered by date, visits of the same day by rowid
    std::string numberSql(const std::string& visitAlias);

    //number of saved visits of the patient
    int count(long long patientRowId);
    //chronological number of a saved visit (0 if it does not exist)
    int number(long long visitRowId);

    //visits of the patient, the most recent first
    std::vector<VisitRecord> getPatientVisits(long long patientRowId);

    DentalVisit getNewDoc(long long patientRowId);
    DentalVisit get(long long rowid);

    long long insert(const DentalVisit& v, long long patientRowId); //returns the rowId of the new instered row (0 on error)
    void remove(long long rowid);
    bool update(const DentalVisit& v);

    //writes the current chronological number of the visit to dental_visit.num
    void storeNumber(long long visitRowId);
};

