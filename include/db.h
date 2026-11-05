#ifndef DB_H
#define DB_H

#include <mysql.h>

MYSQL *db_connect(void);

/*  0 ok, 1 error. */
int db_run_sql(MYSQL *conn, const char *sql);

/* 0 ok, 1 error. */
int db_run_prepared(MYSQL *conn, const char *sql, const char *const *params,
                    unsigned nparams);

#endif // !DB_H
