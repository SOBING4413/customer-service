#ifndef UI_LOGIN_H
#define UI_LOGIN_H

#include <windows.h>
#include "app_data.h"

/* Shows the login window modally. Returns TRUE and fills *outUser on success,
   returns FALSE if the user closed the window without logging in. */
BOOL ShowLoginWindow(HINSTANCE hInst, AppUser *outUser);

#endif
