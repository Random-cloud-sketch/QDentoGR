#include <QtWidgets/QApplication>
#include "View/Widgets/QDento.h"
#include <QtGlobal>
#include <QTranslator>
#include <QStyleHints>
#include "GlobalSettings.h"
#include "View/ModalDialogBuilder.h"

bool initFunction();

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //QDento's theme (Theme.h colors, stylesheets and custom painting) is light-only.
    //Otherwise Qt follows the Windows dark mode for the standard widgets only,
    //resulting in mixed dark/light windows and white-on-white text.
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);

    a.setApplicationName("QDento");
    a.setWindowIcon(QIcon(":/icons/icon_app.png"));

    GlobalSettings::createCfgIfNotExists();

    QTranslator translator;
    QTranslator qtTranslator;

    auto ts_path = GlobalSettings::getTranslationPath();

    bool customTranslation = ts_path.size() && translator.load(ts_path.c_str());

    if (customTranslation) {
        a.installTranslator(&translator);
    }
    else if (GlobalSettings::getLanguage() == "el") {

        //built-in Greek interface (default); "en" keeps the original English texts
        if (translator.load(":/translations/Translation_el_GR.qm")) {
            a.installTranslator(&translator);
            GlobalSettings::setGreekUi(true);
        }

        //standard Qt dialog buttons and context menus
        if (qtTranslator.load(":/translations/qtbase_el.qm")) {
            a.installTranslator(&qtTranslator);
        }
    }

    if (!initFunction()) {  return 0;  }

    //splash screen is destroyed at the end of QDento constructor
    QDento w;

    if (!w.m_loggedIn) return 0;

    w.show();

    return a.exec();
}

#include <QFontDatabase>

#include "Database/Database.h"
#include "Database/DbUpdater.h"
#include "Model/User.h"
#include "View/Graphics/SpriteSheets.h"
#include "View/Graphics/Zodiac.h"
#include "View/Widgets/SplashScreen.h"

bool initFunction() {

    auto args = QApplication::arguments();

#ifdef Q_OS_WIN
   // QApplication::setStyle("windowsvista"); //"windows11", "windowsvista", "Windows", "Fusion"
#endif

#ifdef Q_OS_OSX
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
#endif

    SplashScreen::createAndShow();

    Db::setFilePath(GlobalSettings::getDbPath());

    if (!Db::createIfNotExist()) {

        SplashScreen::hideAndDestroy();

        ModalDialogBuilder::showError(
            QObject::tr("Can't create database. Be sure to start the program as administrator.").toStdString()
        );

        return false;
    };

    //Intializing static data
    SplashScreen::showMessage(QObject::tr("Loading textures").toStdString());
    SpriteSheets::container().initialize();
    Zodiac::initialize();

    User::loadDentistList();

    Db::showErrorDialog(true);

    SplashScreen::showMessage(QObject::tr("Updating database").toStdString());
    DbUpdater::updateDb();

    SplashScreen::showMessage(QObject::tr("Starting QDento...").toStdString());

    return true;
}
