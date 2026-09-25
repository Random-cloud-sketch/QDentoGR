#pragma once

//OAuth client of QDento for Google Calendar.
//Copy this file to GoogleClientConfig.h (in the same folder; that file is not committed to git)
//and fill in the client created in Google Cloud Console:
//  APIs & Services > Credentials > Create credentials > OAuth client ID > Application type: Desktop app
//(Google Calendar API enabled, OAuth consent screen with the scope .../auth/calendar).
//For a desktop application the "client secret" is not confidential, but it is still kept out of git.

#define GOOGLE_CLIENT_ID "xxxxxxxxxxxx-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.apps.googleusercontent.com"
#define GOOGLE_CLIENT_SECRET "GOCSPX-xxxxxxxxxxxxxxxxxxxxxxxxxxxx"
