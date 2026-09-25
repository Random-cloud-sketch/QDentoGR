#pragma once

#include "Model/Patient.h"
#include <queue>

class Db;

namespace DbPatient
{
    struct PatientRecord {
        std::string summary;
        std::string name;   //first and last name
        std::string phone;
        std::string fname;
        std::string birth;
        std::string color;
        long long rowid;
    };

    //assigns a new identifier (UUID) to the patient if it has none
    long long insert(Patient& patient);
    //updates everything except the identifier, which never changes
    bool update(const Patient& patient);
    //by identifier (UUID)
    Patient get(const std::string& patientID);
    Patient get(long long rowid);

    long long getPatientRowid(const std::string& firstName, const std::string& birth);
    long long getPatientRowid(const std::string& id);
    std::vector<PatientRecord> getPatientList();

    TeethNotes getToothNotes(long long patientRowId);
};

