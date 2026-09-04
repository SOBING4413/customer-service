#ifndef APP_DATA_H
#define APP_DATA_H

#include <windows.h>

#define FIELD_LEN 128

typedef struct {
    int  id;
    char wilayah_customer[FIELD_LEN];
    char wilayah_cabang[FIELD_LEN];
    char sales[FIELD_LEN];
    char tanggal[16];           /* YYYY-MM-DD */
    char customer_number[FIELD_LEN];
    char catatan[256];
} Customer;

typedef struct {
    Customer *items;
    int       count;
    int       capacity;
    int       next_id;
} CustomerStore;

typedef struct {
    char username[FIELD_LEN];
    char password[FIELD_LEN];
    char nama[FIELD_LEN];
    char role[FIELD_LEN];
} AppUser;

/* --- data folder resolution --- */
void  data_init_paths(void);
const char *data_get_customers_path(void);
const char *data_get_users_path(void);

/* --- customer store --- */
void  store_init(CustomerStore *s);
void  store_free(CustomerStore *s);
BOOL  store_load(CustomerStore *s);
BOOL  store_save(CustomerStore *s);
int   store_add(CustomerStore *s, Customer *c);            /* returns new id, assigns id */
BOOL  store_update(CustomerStore *s, int id, Customer *c);
BOOL  store_delete(CustomerStore *s, int id);
Customer *store_find(CustomerStore *s, int id);

/* filter: returns array of pointers (caller must free the array, not items) matching search text
   across all text fields (case-insensitive substring). pass NULL/"" for no filter. */
Customer **store_filter(CustomerStore *s, const char *query, int *out_count);

/* --- users --- */
BOOL users_load(AppUser **out_users, int *out_count);
BOOL users_verify(const char *username, const char *password, AppUser *out_user);

/* --- stats --- */
typedef struct {
    char label[FIELD_LEN];
    int  count;
} LabelCount;

int store_count_by_year(CustomerStore *s, const char *year);
int store_total(CustomerStore *s);
/* fills out_years (ascending) with every distinct year found in the data, up to max_years;
   returns how many were written. Lets any UI stay correct as new years appear in the data
   instead of hardcoding specific years. */
int store_distinct_years(CustomerStore *s, int *out_years, int max_years);
/* counts how many distinct calendar months (1-12) have at least one record in the given year;
   this is the right denominator for a "per month" average, whether the year is a past year
   with a short history, a year in progress, or (eventually) a full 12-month year. */
int store_active_months_count(CustomerStore *s, const char *year);
/* returns malloc'd array of 12 LabelCount for months of given year (Jan..Dec), caller frees */
LabelCount *store_monthly_for_year(CustomerStore *s, const char *year);
/* top-N grouping by field selector: 0=wilayah_cabang, 1=sales, 2=wilayah_customer */
LabelCount *store_top_group(CustomerStore *s, int field, int top_n, int *out_count);

#endif
