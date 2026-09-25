#pragma once

#include <vector>
#include "TabPresenter.h"
#include "BrowserPresenter.h"

class QDento;

class MainPresenter
{
	QDento* view{nullptr};

	static MainPresenter s_singleton;

	MainPresenter(){}

	void firstTimeLogic(); //returns false if there are no practices

public:

	static MainPresenter& get() { return s_singleton; }

	void setView(QDento* view);
	void newAmbPressed();
	void newPerioPressed();
	void openCalendar();
	void openRecall();
	void showBrowser();
	void settingsPressed();
	void newInvoicePressed();
    void notificationPressed();
	bool save();
	void logOut();
	void userSettingsPressed();
	//returns false when user selects cancel, otherwise returns true
	bool closeAllTabs();

	bool m_logged_in = false;
};

