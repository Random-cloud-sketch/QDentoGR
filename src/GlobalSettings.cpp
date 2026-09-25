#include "GlobalSettings.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <json.h>
#include <QtGlobal>
#include <QTextStream>
#include <QFileInfo>

#include "Model/User.h"
#include "Model/Date.h"
#include "Model/FreeFunctions.h"
#include "Model/Time.h"
#include "View/ModalDialogBuilder.h"

void rewriteCfg(const Json::Value& settings)
{
    QFile cfg(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("config.json"));
    cfg.open(QIODevice::ReadWrite);
    cfg.resize(0);
    cfg.write(Json::StyledWriter().write(settings).c_str());
}

Json::Value getSettingsAsJson()
{
    Json::Value settings;

    QFile file(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("config.json"));

    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    };

    QString text;

    while (!file.atEnd()) {
        text += file.readLine();
    }

    if (!Json::Reader().parse(text.toStdString(), settings)) return {};

    return settings;
}

std::string GlobalSettings::getDbBackupFilepath()
{
    auto dataFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
 
    if (!dataFolder.cd("backup")) dataFolder.mkpath("backup");

    dataFolder.cd("backup");

    auto time = Time::currentTime();

    return dataFolder.path().toStdString() + "/" +
        "backup" +
        Date::currentDate().to8601() + "T" +
        FreeFn::leadZeroes(time.hour, 2) + "-" +
        FreeFn::leadZeroes(time.minutes, 2) + "-" +
        FreeFn::leadZeroes(time.sec, 2) +
        ".db"
        ;
}

void GlobalSettings::createCfgIfNotExists()
{
    auto dataFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    //creating the user data folder
    if (!dataFolder.exists()) dataFolder.mkpath(".");

    auto settings = getSettingsAsJson();

    if (!settings.isMember("is_ADA")) {
        settings["is_ADA"] = false;
    }

    if (!settings.isMember("translation_path")) {
        settings["translation_path"] = "";
    }

    if (!settings.isMember("language")) {
        settings["language"] = "el";
    }

    if (!settings.isMember("db_path"))
    {
        settings["db_path"] = dataFolder.filePath("database.db").toUtf8().toStdString();
    }

    rewriteCfg(settings);
}

std::string GlobalSettings::getDbPath()
{
    QDir dir(QString::fromUtf8(getSettingsAsJson()["db_path"].asString().c_str()));

    return dir.path().toStdString();
}

std::string GlobalSettings::setDbPath()
{
    auto str = QFileDialog::getOpenFileName(
        nullptr, 
        QObject::tr("Pick database location"),
        getDbPath().c_str(), "sqlite3 file (*.db)"
    );

    if (str.isEmpty()) return std::string();

    Json::Value settings = getSettingsAsJson();

    settings["db_path"] = str.toUtf8().toStdString();

    rewriteCfg(settings);

    return getDbPath();
}

std::string GlobalSettings::getTranslationPath()
{
    auto path = getSettingsAsJson()["translation_path"].asString();

    if (path.empty()) {
        return path;
    }

    QDir dir(QString::fromUtf8(path.c_str()));

    return dir.path().toStdString();
}

void GlobalSettings::removeTranslationPath()
{
    auto settings = getSettingsAsJson();

    settings["translation_path"] = "";

    rewriteCfg(settings);
}

std::string GlobalSettings::setTranslationPath()
{
    auto str = QFileDialog::getOpenFileName(
        nullptr,
        QObject::tr("Choose translation file"),
        getTranslationPath().c_str(), "(*.qm)"
    );

    if (str.size()) {

        Json::Value settings = getSettingsAsJson();

        settings["translation_path"] = str.toUtf8().toStdString();

        rewriteCfg(settings);
    }

    return getTranslationPath();
}

std::string GlobalSettings::getLanguage()
{
    auto language = getSettingsAsJson()["language"].asString();

    return language == "en" ? "en" : "el";
}

void GlobalSettings::setLanguage(const std::string& language)
{
    auto settings = getSettingsAsJson();

    settings["language"] = language;

    rewriteCfg(settings);
}

static bool s_greekUi{ false };

bool GlobalSettings::isGreekUi()
{
    return s_greekUi;
}

void GlobalSettings::setGreekUi(bool greek)
{
    s_greekUi = greek;
}

GlobalSettings::CalendarAxis GlobalSettings::getCalendarAxis()
{
    auto settings = getSettingsAsJson();

    CalendarAxis axis;

    if (settings.isMember("calendar_start_hour")) axis.startHour = settings["calendar_start_hour"].asInt();
    if (settings.isMember("calendar_end_hour")) axis.endHour = settings["calendar_end_hour"].asInt();
    if (settings.isMember("calendar_slot_minutes")) axis.slotMinutes = settings["calendar_slot_minutes"].asInt();
    if (settings.isMember("calendar_full_day")) axis.fullDay = settings["calendar_full_day"].asBool();

    //invalid values from the file fall back to the defaults
    if (axis.startHour < 0 || axis.startHour > 23) axis.startHour = CalendarAxis{}.startHour;
    if (axis.endHour <= axis.startHour || axis.endHour > 24) axis.endHour = std::max(axis.startHour + 1, CalendarAxis{}.endHour);
    if (axis.slotMinutes != 15 && axis.slotMinutes != 30 && axis.slotMinutes != 60) axis.slotMinutes = 15;

    return axis;
}

void GlobalSettings::setCalendarAxis(const CalendarAxis& axis)
{
    auto settings = getSettingsAsJson();

    settings["calendar_start_hour"] = axis.startHour;
    settings["calendar_end_hour"] = axis.endHour;
    settings["calendar_slot_minutes"] = axis.slotMinutes;
    settings["calendar_full_day"] = axis.fullDay;

    rewriteCfg(settings);
}

bool GlobalSettings::isFieldRequired(const std::string& field)
{
    auto settings = getSettingsAsJson();

    if (settings.isMember("required_fields") &&
        settings["required_fields"].isObject() &&
        settings["required_fields"].isMember(field))
    {
        return settings["required_fields"][field].asBool();
    }

    return field == RequiredField::FirstName ||
           field == RequiredField::LastName ||
           field == RequiredField::DateOfBirth;
}

void GlobalSettings::setFieldRequired(const std::string& field, bool required)
{
    auto settings = getSettingsAsJson();

    settings["required_fields"][field] = required;

    rewriteCfg(settings);
}

GlobalSettings::GoogleCalendar GlobalSettings::getGoogleCalendar()
{
    auto settings = getSettingsAsJson();

    GoogleCalendar g;

    if (!settings.isMember("google_calendar") || !settings["google_calendar"].isObject()) return g;

    auto& s = settings["google_calendar"];

    g.enabled = s.get("enabled", false).asBool();
    g.accountEmail = s.get("account_email", "").asString();
    g.calendarId = s.get("calendar_id", "").asString();
    g.calendarName = s.get("calendar_name", "").asString();
    g.autoSync = s.get("auto_sync", true).asBool();
    g.dentistRowid = s.get("dentist_rowid", 0).asInt64();
    g.testApiUrl = s.get("test_api_url", "").asString();
    g.testOAuthUrl = s.get("test_oauth_url", "").asString();

    return g;
}

void GlobalSettings::setGoogleCalendar(const GoogleCalendar& g)
{
    auto settings = getSettingsAsJson();

    auto& s = settings["google_calendar"];

    s["enabled"] = g.enabled;
    s["account_email"] = g.accountEmail;
    s["calendar_id"] = g.calendarId;
    s["calendar_name"] = g.calendarName;
    s["auto_sync"] = g.autoSync;
    s["dentist_rowid"] = static_cast<Json::Int64>(g.dentistRowid);

    //the test addresses are only kept when they were set by hand
    if (g.testApiUrl.size()) s["test_api_url"] = g.testApiUrl; else s.removeMember("test_api_url");
    if (g.testOAuthUrl.size()) s["test_oauth_url"] = g.testOAuthUrl; else s.removeMember("test_oauth_url");

    rewriteCfg(settings);
}

std::string GlobalSettings::getDataFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
}

bool GlobalSettings::isADANum()
{
    return getSettingsAsJson()["is_ADA"].asBool();
}

void GlobalSettings::setToothNum(bool ADA)
{
    auto settings = getSettingsAsJson();

    settings["is_ADA"] = ADA;

    rewriteCfg(settings);
}