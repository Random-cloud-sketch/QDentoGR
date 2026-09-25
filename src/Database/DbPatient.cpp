#include "DbPatient.h"
#include "Database.h"
#include "Database/DbNotes.h"
#include "Model/UpperCase.h"

long long DbPatient::insert(Patient& patient)
{
    //every patient gets a permanent identifier when created
    if (patient.id.empty()) patient.id = Patient::newId();

    Db db(
        "INSERT INTO patient "
        "(id, birth, sex, fname, lname, "
        "address, phone, color, referring_doctor) "
        "VALUES (?,?,?,?,?,?,?,?,?)"
    );

    //names, address and referring doctor are always saved in capital letters
    db.bind(1, patient.id);
    db.bind(2, patient.birth.to8601());
    db.bind(3, patient.sex);
    db.bind(4, UpperCase::convert(patient.firstName));
    db.bind(5, UpperCase::convert(patient.lastName));
    db.bind(6, UpperCase::convert(patient.address));
    db.bind(7, patient.phone);
    db.bind(8, patient.colorNameRgb);
    db.bind(9, UpperCase::convert(patient.referringDoctor));

    if (db.execute()) return db.lastInsertedRowID();

    return 0;

}

bool DbPatient::update(const Patient& patient)
{
    Db db(

        //the identifier is never changed
        "UPDATE patient SET "
        "birth=?,"
        "sex=?,"
        "fname=?,"
        "lname=?,"
        "address=?,"
        "phone=?,"
        "color=?,"
        "referring_doctor=? "
        "WHERE rowid=?"
    );

    db.bind(1, patient.birth.to8601());
    db.bind(2, patient.sex);
    db.bind(3, UpperCase::convert(patient.firstName));
    db.bind(4, UpperCase::convert(patient.lastName));
    db.bind(5, UpperCase::convert(patient.address));
    db.bind(6, patient.phone);
    db.bind(7, patient.colorNameRgb);
    db.bind(8, UpperCase::convert(patient.referringDoctor));

    db.bind(9, patient.rowid);

    return db.execute();

}

Patient DbPatient::get(const std::string& patientID)
{
    std::string query = "SELECT rowid, birth, sex, fname, lname, address, phone, color, referring_doctor FROM patient WHERE id=?";

    Db db(query);

    db.bind(1, patientID);

    Patient patient;

    while (db.hasRows())
    {
        patient.id = patientID;
        patient.rowid = db.asRowId(0),
        patient.birth = Date(db.asString(1));
        patient.sex = Patient::Sex(db.asInt(2));
        patient.firstName = db.asString(3);
        patient.lastName = db.asString(4);
        patient.address = db.asString(5);
        patient.phone = db.asString(6);
        patient.colorNameRgb = db.asString(7);
        patient.referringDoctor = db.asString(8);
    }

    patient.teethNotes = getToothNotes(patient.rowid);
    patient.patientNotes = DbNotes::getNote(patient.rowid, -1);

    return patient;
}

Patient DbPatient::get(long long rowid)
{
    Db db("SELECT rowid, id, birth, sex, fname, lname, address, phone, color, referring_doctor "
        "FROM patient WHERE rowid = " + std::to_string(rowid)
    );

    Patient patient;

    while (db.hasRows())
    {
        patient.rowid = db.asRowId(0),
        patient.id = db.asString(1);
        patient.birth = Date(db.asString(2));
        patient.sex = Patient::Sex(db.asInt(3));
        patient.firstName = db.asString(4);
        patient.lastName = db.asString(5);
        patient.address = db.asString(6);
        patient.phone = db.asString(7);
        patient.colorNameRgb = db.asString(8);
        patient.referringDoctor = db.asString(9);
    }

    patient.teethNotes = getToothNotes(patient.rowid);
    patient.patientNotes = DbNotes::getNote(patient.rowid, -1);

    return patient;
}

long long DbPatient::getPatientRowid(const std::string& firstName, const std::string& birth)
{
    Db db;

    db.newStatement("SELECT rowid FROM patient WHERE fname=? AND birth=?");

    db.bind(1, firstName);
    db.bind(2, birth);

    while(db.hasRows()){
        return db.asRowId(0);
    }

    return 0;
}

long long DbPatient::getPatientRowid(const std::string& id)
{
    Db db;

    db.newStatement("SELECT rowid FROM patient WHERE id = ?"
    );

    db.bind(1, id);

    while (db.hasRows()) {
        return db.asRowId(0);
    }

    return 0;
}

std::vector<DbPatient::PatientRecord> DbPatient::getPatientList()
{
    std::vector<PatientRecord> result;

    Db db("SELECT fname, lname, phone, birth, color, rowid FROM patient");

    while (db.hasRows())
    {
        PatientRecord r;
        r.fname = db.asString(0);
        r.summary = r.fname + " " + db.asString(1);
        r.name = r.summary;
        
        auto phone = db.asString(2);
        r.phone = phone;

        if (phone.size()) {
            r.summary += " ";
            r.summary += phone;
        }

        r.birth = db.asString(3);     

        r.color = db.asString(4);

        r.rowid = db.asRowId(5);

        result.push_back(r);
    }

    return result;
}

TeethNotes DbPatient::getToothNotes(long long patientRowId)
{
    Db db("SELECT tooth, text FROM note WHERE patient_rowid=?");

    db.bind(1, patientRowId);

    TeethNotes notes{};

    while (db.hasRows())
    {
        auto index = db.asInt(0);
        //teeth notes range from 0 to 31
        if (index < 0 || index >=notes.size()) continue;

        notes[index] = db.asString(1);
    }

    return notes;
}