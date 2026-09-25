#include "DbDentalVisit.h"
#include "Database/Database.h"
#include "Model/User.h"
#include "Model/Patient.h"
#include "Model/Dental/DentalVisit.h"
#include "Model/Date.h"
#include "Model/Parser.h"
#include "DbProcedure.h"
#include "Model/FreeFunctions.h"

long long DbDentalVisit::insert(const DentalVisit& sheet, long long patientRowId)
{

    Db db("INSERT INTO dental_visit "
        "(date, num, status, patient_rowid, dentist_rowid) "
        "VALUES (?,?,?,?,?)");

    db.bind(1, sheet.date.to8601());
    db.bind(2, 0); //replaced below by the chronological number
    db.bind(3, Parser::write(sheet.teeth));
    db.bind(4, patientRowId);
    db.bind(5, User::dentist().rowID);

    if (!db.execute()) return 0;

    auto rowID = db.lastInsertedRowID();

    DbProcedure::saveProcedures(rowID, sheet.procedures.list(), db);

    storeNumber(rowID);

    return rowID;
}

bool DbDentalVisit::update(const DentalVisit& sheet)
{
    //the number is never taken from the view
    std::string query = "UPDATE dental_visit SET "
        "date=?,"
        "status=? "
        "WHERE rowid=?";

    Db db(query);

    db.bind(1, sheet.date.to8601());
    db.bind(2, Parser::write(sheet.teeth));
    db.bind(3, sheet.rowid);

    if (!db.execute()) return false;

    DbProcedure::saveProcedures(sheet.rowid, sheet.procedures.list(), db);

    storeNumber(sheet.rowid);

    return true;
}

DentalVisit DbDentalVisit::getNewDoc(long long patientRowId)
{

    DentalVisit visit;
    visit.dentist_rowid = User::dentist().rowID;
    std::string status;

    Db db;

    std::string query = "SELECT rowid, num, status, date FROM dental_visit WHERE "
        "patient_rowid=? AND dentist_rowid=? AND "
        "date(dental_visit.date) = date('now')";

    db.newStatement(query);

    db.bind(1, patientRowId);
    db.bind(2, User::dentist().rowID);

    while(db.hasRows())
    {
        visit.patient_rowid = patientRowId;
        visit.rowid = db.asRowId(0);
        status = db.asString(2);
        visit.date = db.asString(3);
    }

    //today's visit is opened again, otherwise the new visit shows the number of saved visits
    //(it is counted only when it is saved)
    visit.number = visit.rowid ? number(visit.rowid) : count(patientRowId);

    if (!visit.rowid)
    {
        //getting the last recorded status

        db.newStatement(
            "SELECT rowid, status, date FROM dental_visit WHERE "
            "patient_rowid = ? "
            "ORDER BY date DESC LIMIT 1"
        );

        db.bind(1, patientRowId);

        long long oldId = 0;
        std::string basedOnNrn;
        Date amblistDate;

        while(db.hasRows()){

            oldId = db.asRowId(0);
            status = db.asString(1);
            amblistDate = db.asString(2);
        }

        if (!oldId) return visit; //no data is found for this patient

        Parser::parse(status, visit.teeth);

        //getting all procedures after the last recorded status and applying them

        db.newStatement(
            "SELECT dental_visit.rowid FROM procedure "
            "LEFT JOIN dental_visit ON procedure.dental_visit_rowid = dental_visit.rowid "
            "WHERE dental_visit.date >= ? AND patient_rowid = ? "
            "GROUP BY dental_visit.rowid ORDER BY dental_visit.date ASC"
        );

        db.bind(1, amblistDate.to8601());
        db.bind(2, patientRowId);

        std::vector<long long> amblistRowidProcedures;


        while (db.hasRows()) {
            amblistRowidProcedures.push_back(db.asLongLong(0));
        }

        for (auto& rowid : amblistRowidProcedures) {
            for (auto& p : DbProcedure::getProcedures(rowid, db))
            {
                p.applyProcedure(visit.teeth);

            }
        }

        return visit;
    }

    Parser::parse(status, visit.teeth);
    visit.procedures.addProcedures(DbProcedure::getProcedures(visit.rowid, db));


    return visit;
}

DentalVisit DbDentalVisit::get(long long rowId)
{

    std::string status;
    DentalVisit dental_visit;

    Db db(
        "SELECT rowid, num, status, patient_rowid, date FROM dental_visit WHERE "
        "rowid = " + std::to_string(rowId)
    );

    while (db.hasRows())
    {
        dental_visit.rowid = db.asRowId(0);
        status = db.asString(2);
        dental_visit.dentist_rowid = User::dentist().rowID;
        dental_visit.patient_rowid = db.asRowId(3);
        dental_visit.date = db.asString(4);
    }

    Parser::parse(status, dental_visit.teeth);
    dental_visit.procedures.addProcedures(DbProcedure::getProcedures(dental_visit.rowid, db));
    dental_visit.number = number(dental_visit.rowid);
    return dental_visit;

}

void DbDentalVisit::remove(long long rowid)
{
    Db::crudQuery("DELETE FROM dental_visit WHERE rowid = " + std::to_string(rowid) + ")");
}

std::string DbDentalVisit::numberSql(const std::string& a)
{
    return
        "(SELECT COUNT(*) FROM dental_visit nv WHERE nv.patient_rowid = " + a + ".patient_rowid AND "
        "(date(nv.date) < date(" + a + ".date) OR (date(nv.date) = date(" + a + ".date) AND nv.rowid <= " + a + ".rowid)))";
}

int DbDentalVisit::count(long long patientRowId)
{
    Db db("SELECT COUNT(*) FROM dental_visit WHERE patient_rowid=?");

    db.bind(1, patientRowId);

    while (db.hasRows()) {
        return db.asInt(0);
    }

    return 0;
}

int DbDentalVisit::number(long long visitRowId)
{
    Db db("SELECT " + numberSql("v") + " FROM dental_visit v WHERE v.rowid=?");

    db.bind(1, visitRowId);

    while (db.hasRows()) {
        return db.asInt(0);
    }

    return 0;
}

void DbDentalVisit::storeNumber(long long visitRowId)
{
    //the num column keeps the number the visit had when it was last saved;
    //what is shown is always calculated (older visits are never renumbered)
    auto current = number(visitRowId);

    Db db("UPDATE dental_visit SET num = ? WHERE rowid = ?");

    db.bind(1, current);
    db.bind(2, visitRowId);

    db.execute();
}

std::vector<DbDentalVisit::VisitRecord> DbDentalVisit::getPatientVisits(long long patientRowId)
{
    std::vector<VisitRecord> result;

    Db db(
        "SELECT v.rowid, " + numberSql("v") + ", v.date, "
        //the appointments are not linked to the visits: the patient's appointment on the same day
        "(SELECT MIN(strftime('%H:%M', a.start)) FROM appointment a "
        "WHERE a.patient_rowid = v.patient_rowid AND date(a.start) = date(v.date)), "
        "v.dentist_rowid = ? "
        "FROM dental_visit v WHERE v.patient_rowid = ? "
        "ORDER BY date(v.date) DESC, v.rowid DESC"
    );

    db.bind(1, User::dentist().rowID);
    db.bind(2, patientRowId);

    while (db.hasRows())
    {
        auto& r = result.emplace_back();
        r.rowid = db.asRowId(0);
        r.number = db.asInt(1);
        r.date = db.asString(2);
        r.time = db.asString(3);
        r.permissionToOpen = db.asBool(4);
    }

    for (auto& r : result)
    {
        std::string description;

        for (auto& p : DbProcedure::getProcedures(r.rowid, db))
        {
            std::string text = p.name;

            if (auto tooth = p.getToothString(); tooth.size()) text += " " + tooth;

            if (p.notes.size()) text += " (" + p.notes + ")";

            if (description.size()) description += "; ";

            description += text;
        }

        r.description = description;
    }

    return result;
}