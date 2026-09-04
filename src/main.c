#include <windows.h>
#include "app_data.h"
#include "theme.h"
#include "ui_login.h"
#include "ui_main.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    data_init_paths();
    theme_init();

    CustomerStore store;
    store_init(&store);
    store_load(&store);

    BOOL running = TRUE;
    while (running) {
        AppUser user;
        ZeroMemory(&user, sizeof(user));
        BOOL loggedIn = ShowLoginWindow(hInstance, &user);
        if (!loggedIn) {
            running = FALSE;
            break;
        }

        BOOL wantsLogout = ShowMainShell(hInstance, &user, &store);
        if (!wantsLogout) {
            running = FALSE;
        }
        /* if wantsLogout, loop back and show the login window again */
    }

    store_free(&store);
    theme_cleanup();
    return 0;
}
