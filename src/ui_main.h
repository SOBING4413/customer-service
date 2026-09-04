#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <windows.h>
#include "app_data.h"

/* Creates and runs the main application shell (sidebar + dashboard + customer
   management). Blocks until the window is closed or the user logs out.
   Returns TRUE if the user chose "logout" (caller may show login again),
   FALSE if the app should exit entirely. */
BOOL ShowMainShell(HINSTANCE hInst, AppUser *user, CustomerStore *store);

#endif
