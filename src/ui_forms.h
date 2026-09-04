#ifndef UI_FORMS_H
#define UI_FORMS_H

#include <windows.h>
#include "app_data.h"

/* Shows the add/edit customer form. If existing != NULL, pre-fills for editing
   and updates that record; otherwise creates a new record. Returns TRUE if the
   record was saved. */
BOOL ShowCustomerFormDialog(HINSTANCE hInst, HWND owner, CustomerStore *store, Customer *existing);

/* Shows a read-only detail view of a customer. */
void ShowCustomerDetailDialog(HINSTANCE hInst, HWND owner, Customer *c);

/* Simple confirm dialog. Returns TRUE if user confirmed (Yes). */
BOOL ShowConfirmDialog(HWND owner, const char *title, const char *message);

#endif
