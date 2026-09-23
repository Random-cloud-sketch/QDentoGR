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