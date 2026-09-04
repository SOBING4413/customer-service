#include "app_data.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <shlwapi.h>
#include <shlobj.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

static char g_exeDir[MAX_PATH];
static char g_dataDir[MAX_PATH];
static char g_customersPath[MAX_PATH];
static char g_usersPath[MAX_PATH];

static void ensure_dir(const char *path) {
    CreateDirectoryA(path, NULL);
}

void data_init_paths(void) {
    GetModuleFileNameA(NULL, g_exeDir, MAX_PATH);
    PathRemoveFileSpecA(g_exeDir);

    /* data folder lives next to the executable: <exe>\data */
    sprintf(g_dataDir, "%s\\data", g_exeDir);
    ensure_dir(g_dataDir);

    sprintf(g_customersPath, "%s\\customers.json", g_dataDir);
    sprintf(g_usersPath, "%s\\users.json", g_dataDir);
}

const char *data_get_customers_path(void) { return g_customersPath; }
const char *data_get_users_path(void)     { return g_usersPath; }

/* ---------- generic file read/write helpers ---------- */

static char *read_whole_file(const char *path, long *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

static BOOL write_whole_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return FALSE;
    fputs(text, f);
    fclose(f);
    return TRUE;
}

/* ---------- customer store ---------- */

void store_init(CustomerStore *s) {
    s->items = NULL;
    s->count = 0;
    s->capacity = 0;
    s->next_id = 1;
}

void store_free(CustomerStore *s) {
    free(s->items);
    s->items = NULL;
    s->count = 0;
    s->capacity = 0;
}

static void store_ensure_capacity(CustomerStore *s, int needed) {
    if (needed <= s->capacity) return;
    int newcap = s->capacity == 0 ? 32 : s->capacity * 2;
    while (newcap < needed) newcap *= 2;
    s->items = (Customer *)realloc(s->items, newcap * sizeof(Customer));
    s->capacity = newcap;
}

static void copy_field(char *dst, size_t dstsz, cJSON *obj, const char *key) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (v && cJSON_IsString(v) && v->valuestring) {
        strncpy(dst, v->valuestring, dstsz - 1);
        dst[dstsz - 1] = '\0';
    } else {
        dst[0] = '\0';
    }
}

BOOL store_load(CustomerStore *s) {
    store_free(s);
    store_init(s);

    long len = 0;
    char *text = read_whole_file(g_customersPath, &len);
    if (!text) {
        return TRUE; /* no file yet -> empty store is fine */
    }

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) return FALSE;

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "customers");
    int max_id = 0;
    if (arr && cJSON_IsArray(arr)) {
        int n = cJSON_GetArraySize(arr);
        store_ensure_capacity(s, n);
        cJSON *el;
        cJSON_ArrayForEach(el, arr) {
            Customer c;
            memset(&c, 0, sizeof(c));
            cJSON *idv = cJSON_GetObjectItemCaseSensitive(el, "id");
            c.id = (idv && cJSON_IsNumber(idv)) ? idv->valueint : 0;
            copy_field(c.wilayah_customer, sizeof(c.wilayah_customer), el, "wilayah_customer");
            copy_field(c.wilayah_cabang, sizeof(c.wilayah_cabang), el, "wilayah_cabang");
            copy_field(c.sales, sizeof(c.sales), el, "sales");
            copy_field(c.tanggal, sizeof(c.tanggal), el, "tanggal");
            copy_field(c.customer_number, sizeof(c.customer_number), el, "customer_number");
            copy_field(c.catatan, sizeof(c.catatan), el, "catatan");
            s->items[s->count++] = c;
            if (c.id > max_id) max_id = c.id;
        }
    }

    cJSON *nid = cJSON_GetObjectItemCaseSensitive(root, "next_id");
    if (nid && cJSON_IsNumber(nid)) {
        s->next_id = nid->valueint;
    } else {
        s->next_id = max_id + 1;
    }
    if (s->next_id <= max_id) s->next_id = max_id + 1;

    cJSON_Delete(root);
    return TRUE;
}

BOOL store_save(CustomerStore *s) {
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < s->count; i++) {
        Customer *c = &s->items[i];
        cJSON *el = cJSON_CreateObject();
        cJSON_AddNumberToObject(el, "id", c->id);
        cJSON_AddStringToObject(el, "wilayah_customer", c->wilayah_customer);
        cJSON_AddStringToObject(el, "wilayah_cabang", c->wilayah_cabang);
        cJSON_AddStringToObject(el, "sales", c->sales);
        cJSON_AddStringToObject(el, "tanggal", c->tanggal);
        cJSON_AddStringToObject(el, "customer_number", c->customer_number);
        cJSON_AddStringToObject(el, "catatan", c->catatan);
        cJSON_AddItemToArray(arr, el);
    }
    cJSON_AddItemToObject(root, "customers", arr);
    cJSON_AddNumberToObject(root, "next_id", s->next_id);

    char *text = cJSON_Print(root);
    BOOL ok = FALSE;
    if (text) {
        /* write to temp then replace, to reduce risk of corruption */
        char tmpPath[MAX_PATH];
        sprintf(tmpPath, "%s.tmp", g_customersPath);
        if (write_whole_file(tmpPath, text)) {
            DeleteFileA(g_customersPath);
            ok = MoveFileA(tmpPath, g_customersPath);
        }
        free(text);
    }
    cJSON_Delete(root);
    return ok;
}

int store_add(CustomerStore *s, Customer *c) {
    store_ensure_capacity(s, s->count + 1);
    c->id = s->next_id++;
    s->items[s->count++] = *c;
    store_save(s);
    return c->id;
}

BOOL store_update(CustomerStore *s, int id, Customer *c) {
    for (int i = 0; i < s->count; i++) {
        if (s->items[i].id == id) {
            int keepId = s->items[i].id;
            s->items[i] = *c;
            s->items[i].id = keepId;
            store_save(s);
            return TRUE;
        }
    }
    return FALSE;
}

BOOL store_delete(CustomerStore *s, int id) {
    for (int i = 0; i < s->count; i++) {
        if (s->items[i].id == id) {
            memmove(&s->items[i], &s->items[i + 1], (s->count - i - 1) * sizeof(Customer));
            s->count--;
            store_save(s);
            return TRUE;
        }
    }
    return FALSE;
}

Customer *store_find(CustomerStore *s, int id) {
    for (int i = 0; i < s->count; i++) {
        if (s->items[i].id == id) return &s->items[i];
    }
    return NULL;
}

static BOOL str_contains_ci(const char *hay, const char *needle) {
    if (!needle || !*needle) return TRUE;
    if (!hay) return FALSE;
    int hlen = (int)strlen(hay), nlen = (int)strlen(needle);
    if (nlen > hlen) return FALSE;
    for (int i = 0; i <= hlen - nlen; i++) {
        int j = 0;
        for (; j < nlen; j++) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
        }
        if (j == nlen) return TRUE;
    }
    return FALSE;
}

Customer **store_filter(CustomerStore *s, const char *query, int *out_count) {
    Customer **result = (Customer **)malloc(sizeof(Customer *) * (s->count > 0 ? s->count : 1));
    int n = 0;
    for (int i = 0; i < s->count; i++) {
        Customer *c = &s->items[i];
        BOOL match = (!query || !*query) ||
            str_contains_ci(c->wilayah_customer, query) ||
            str_contains_ci(c->wilayah_cabang, query) ||
            str_contains_ci(c->sales, query) ||
            str_contains_ci(c->tanggal, query) ||
            str_contains_ci(c->customer_number, query) ||
            str_contains_ci(c->catatan, query);
        if (match) result[n++] = c;
    }
    *out_count = n;
    return result;
}

BOOL users_load(AppUser **out_users, int *out_count) {
    long len = 0;
    char *text = read_whole_file(g_usersPath, &len);
    if (!text) { *out_users = NULL; *out_count = 0; return FALSE; }

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) { *out_users = NULL; *out_count = 0; return FALSE; }

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "users");
    int n = (arr && cJSON_IsArray(arr)) ? cJSON_GetArraySize(arr) : 0;
    AppUser *users = (AppUser *)malloc(sizeof(AppUser) * (n > 0 ? n : 1));
    int idx = 0;
    cJSON *el;
    if (arr) {
        cJSON_ArrayForEach(el, arr) {
            memset(&users[idx], 0, sizeof(AppUser));
            copy_field(users[idx].username, sizeof(users[idx].username), el, "username");
            copy_field(users[idx].password, sizeof(users[idx].password), el, "password");
            copy_field(users[idx].nama, sizeof(users[idx].nama), el, "nama");
            copy_field(users[idx].role, sizeof(users[idx].role), el, "role");
            idx++;
        }
    }
    cJSON_Delete(root);
    *out_users = users;
    *out_count = idx;
    return TRUE;
}

BOOL users_verify(const char *username, const char *password, AppUser *out_user) {
    AppUser *users = NULL;
    int count = 0;
    if (!users_load(&users, &count)) return FALSE;
    BOOL ok = FALSE;
    for (int i = 0; i < count; i++) {
        if (_stricmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            if (out_user) *out_user = users[i];
            ok = TRUE;
            break;
        }
    }
    free(users);
    return ok;
}

/* ---------- stats ---------- */

int store_total(CustomerStore *s) { return s->count; }

int store_count_by_year(CustomerStore *s, const char *year) {
    int n = 0;
    for (int i = 0; i < s->count; i++) {
        if (strncmp(s->items[i].tanggal, year, 4) == 0) n++;
    }
    return n;
}

LabelCount *store_monthly_for_year(CustomerStore *s, const char *year) {
    static const char *names[12] = {"Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des"};
    LabelCount *arr = (LabelCount *)malloc(sizeof(LabelCount) * 12);
    for (int i = 0; i < 12; i++) {
        strcpy(arr[i].label, names[i]);
        arr[i].count = 0;
    }
    for (int i = 0; i < s->count; i++) {
        const char *t = s->items[i].tanggal;
        if (strncmp(t, year, 4) == 0 && strlen(t) >= 7) {
            int month = (t[5] - '0') * 10 + (t[6] - '0');
            if (month >= 1 && month <= 12) arr[month - 1].count++;
        }
    }
    return arr;
}

int store_distinct_years(CustomerStore *s, int *out_years, int max_years) {
    int years[128];
    int n = 0;
    for (int i = 0; i < s->count; i++) {
        const char *t = s->items[i].tanggal;
        if (strlen(t) < 4) continue;
        BOOL digitsOk = TRUE;
        for (int k = 0; k < 4; k++) if (!isdigit((unsigned char)t[k])) { digitsOk = FALSE; break; }
        if (!digitsOk) continue;
        int y = (t[0]-'0')*1000 + (t[1]-'0')*100 + (t[2]-'0')*10 + (t[3]-'0');
        BOOL found = FALSE;
        for (int j = 0; j < n; j++) if (years[j] == y) { found = TRUE; break; }
        if (!found && n < 128) years[n++] = y;
    }
    /* insertion sort ascending (n is always small) */
    for (int i = 1; i < n; i++) {
        int key = years[i], j = i - 1;
        while (j >= 0 && years[j] > key) { years[j + 1] = years[j]; j--; }
        years[j + 1] = key;
    }
    int outN = (n < max_years) ? n : max_years;
    for (int i = 0; i < outN; i++) out_years[i] = years[i];
    return outN;
}

int store_active_months_count(CustomerStore *s, const char *year) {
    BOOL seen[12] = {0};
    int cnt = 0;
    for (int i = 0; i < s->count; i++) {
        const char *t = s->items[i].tanggal;
        if (strncmp(t, year, 4) == 0 && strlen(t) >= 7) {
            int month = (t[5] - '0') * 10 + (t[6] - '0');
            if (month >= 1 && month <= 12 && !seen[month - 1]) { seen[month - 1] = TRUE; cnt++; }
        }
    }
    return cnt;
}

static int find_label(LabelCount *arr, int n, const char *label) {
    for (int i = 0; i < n; i++) if (strcmp(arr[i].label, label) == 0) return i;
    return -1;
}

LabelCount *store_top_group(CustomerStore *s, int field, int top_n, int *out_count) {
    LabelCount *arr = (LabelCount *)malloc(sizeof(LabelCount) * (s->count > 0 ? s->count : 1));
    int n = 0;
    for (int i = 0; i < s->count; i++) {
        const char *val = field == 0 ? s->items[i].wilayah_cabang :
                           field == 1 ? s->items[i].sales :
                                        s->items[i].wilayah_customer;
        if (!val || !*val) continue;
        int idx = find_label(arr, n, val);
        if (idx < 0) {
            strncpy(arr[n].label, val, sizeof(arr[n].label) - 1);
            arr[n].label[sizeof(arr[n].label) - 1] = '\0';
            arr[n].count = 1;
            n++;
        } else {
            arr[idx].count++;
        }
    }
    /* simple sort desc by count */
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (arr[j].count > arr[i].count) {
                LabelCount tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
            }
        }
    }
    if (top_n > 0 && n > top_n) n = top_n;
    *out_count = n;
    return arr;
}
