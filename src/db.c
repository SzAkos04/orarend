#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "print.h"

#define MAX_PARAMS 4

MYSQL *db_connect(void) {
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init failed\n");
        return NULL;
    }
    if (mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4") != 0) {
        fprintf(stderr, "cannot set charset\n");
        mysql_close(conn);
        return NULL;
    }
    if (mysql_real_connect(conn, NULL, "szakos", NULL, "orarend", 0, NULL, 0) ==
        NULL) {
        fprintf(stderr, "connect failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }
    return conn;
}

int db_run_sql(MYSQL *conn, const char *sql) {
    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "query failed: %s\n", mysql_error(conn));
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        if (mysql_field_count(conn) == 0) {
            printf("OK, %llu rows affected\n",
                   (unsigned long long)mysql_affected_rows(conn));
            return 0;
        }
        fprintf(stderr, "store_result failed: %s\n", mysql_error(conn));
        return -1;
    }

    int rc = print_result(res);
    mysql_free_result(res);
    return rc;
}

static int stmt_print(MYSQL_STMT *stmt) {
    MYSQL_RES *meta = mysql_stmt_result_metadata(stmt);
    if (meta == NULL) {
        fprintf(stderr, "statement returned no result set\n");
        return -1;
    }

    size_t ncols = mysql_num_fields(meta);
    size_t nrows = (size_t)mysql_stmt_num_rows(stmt);
    MYSQL_FIELD *fields = mysql_fetch_fields(meta);

    MYSQL_BIND *bind = calloc(ncols + 1, sizeof *bind);
    unsigned long *len = calloc(ncols + 1, sizeof *len);
    my_bool *is_null = calloc(ncols + 1, sizeof *is_null);
    char **buf = calloc(ncols + 1, sizeof *buf);
    const char **headers = calloc(ncols + 1, sizeof *headers);
    const char **cells = calloc(nrows * ncols + 1, sizeof *cells);
    size_t nfilled = 0;
    int rc = -1;

    if (!bind || !len || !is_null || !buf || !headers || !cells) {
        fprintf(stderr, "out of memory\n");
        goto done;
    }
    for (size_t c = 0; c < ncols; c++) {
        size_t size = fields[c].max_length + 1;
        buf[c] = malloc(size);
        if (buf[c] == NULL) {
            fprintf(stderr, "out of memory\n");
            goto done;
        }
        headers[c] = fields[c].name;
        bind[c].buffer_type = MYSQL_TYPE_STRING;
        bind[c].buffer = buf[c];
        bind[c].buffer_length = size;
        bind[c].length = &len[c];
        bind[c].is_null = &is_null[c];
    }
    if (mysql_stmt_bind_result(stmt, bind) != 0) {
        fprintf(stderr, "bind_result failed: %s\n", mysql_stmt_error(stmt));
        goto done;
    }

    while (nfilled < nrows) {
        int fr = mysql_stmt_fetch(stmt);
        if (fr == MYSQL_NO_DATA) {
            break;
        }
        if (fr != 0 && fr != MYSQL_DATA_TRUNCATED) {
            fprintf(stderr, "fetch failed: %s\n", mysql_stmt_error(stmt));
            goto done;
        }
        for (size_t c = 0; c < ncols; c++) {
            size_t i = nfilled * ncols + c;
            if (is_null[c]) {
                cells[i] = NULL;
            } else if (len[c] >= bind[c].buffer_length) {
                fprintf(stderr, "value truncated in column %s\n", headers[c]);
                goto done;
            } else {
                cells[i] = strndup(buf[c], len[c]);
                if (cells[i] == NULL) {
                    fprintf(stderr, "out of memory\n");
                    goto done;
                }
            }
        }
        nfilled++;
    }
    rc = print_table(stdout, ncols, headers, cells, nfilled);

done:
    if (cells != NULL) {
        for (size_t i = 0; i < nfilled * ncols; i++) {
            free((char *)cells[i]);
        }
    }
    if (buf != NULL) {
        for (size_t c = 0; c < ncols; c++) {
            free(buf[c]);
        }
    }
    free(cells);
    free(headers);
    free(buf);
    free(is_null);
    free(len);
    free(bind);
    mysql_free_result(meta);
    return rc;
}

int db_run_prepared(MYSQL *conn, const char *sql, const char *const *params,
                    unsigned nparams) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (stmt == NULL) {
        fprintf(stderr, "stmt_init failed: %s\n", mysql_error(conn));
        return -1;
    }

    MYSQL_BIND bind[MAX_PARAMS];
    unsigned long len[MAX_PARAMS];
    my_bool update_max = 1;
    int rc = -1;
    memset(bind, 0, sizeof bind);

    if (nparams > MAX_PARAMS) {
        fprintf(stderr, "too many parameters\n");
        goto done;
    }
    if (mysql_stmt_prepare(stmt, sql, strlen(sql)) != 0) {
        fprintf(stderr, "prepare failed: %s\n", mysql_stmt_error(stmt));
        goto done;
    }
    if (mysql_stmt_param_count(stmt) != nparams) {
        fprintf(stderr, "parameter count mismatch\n");
        goto done;
    }
    for (unsigned i = 0; i < nparams; i++) {
        len[i] = strlen(params[i]);
        bind[i].buffer_type = MYSQL_TYPE_STRING;
        bind[i].buffer = (void *)params[i];
        bind[i].buffer_length = len[i];
        bind[i].length = &len[i];
    }
    if (nparams > 0 && mysql_stmt_bind_param(stmt, bind) != 0) {
        fprintf(stderr, "bind_param failed: %s\n", mysql_stmt_error(stmt));
        goto done;
    }
    if (mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &update_max) !=
        0) {
        fprintf(stderr, "attr_set failed: %s\n", mysql_stmt_error(stmt));
        goto done;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        fprintf(stderr, "execute failed: %s\n", mysql_stmt_error(stmt));
        goto done;
    }
    if (mysql_stmt_store_result(stmt) != 0) {
        fprintf(stderr, "store_result failed: %s\n", mysql_stmt_error(stmt));
        goto done;
    }
    rc = stmt_print(stmt);

done:
    mysql_stmt_close(stmt);
    return rc;
}
