#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "db.h"

#define DAY_COLS                                                               \
    "ELT(o.nap,'Mon','Tue','Wed','Thu','Fri') AS nap, o.sorszam AS ora, "      \
    "tg.nev AS tantargy, cs.nev AS csoport"
#define LESSON_JOINS                                                           \
    " FROM ora o JOIN tanar t ON t.id=o.tanar_id"                              \
    " JOIN tantargy tg ON tg.id=o.tantargy_id"                                 \
    " JOIN csoport cs ON cs.id=o.csoport_id"                                   \
    " JOIN terem te ON te.id=o.terem_id"
#define ORDER_BY " ORDER BY o.nap, o.sorszam"

static const char SQL_TANAR[] =
    "SELECT " DAY_COLS ", te.nev AS terem" LESSON_JOINS
    " WHERE t.nev LIKE CONCAT('%', ?, '%')" ORDER_BY;

static const char SQL_OSZTALY[] =
    "SELECT " DAY_COLS ", t.nev AS tanar, te.nev AS terem" LESSON_JOINS
    " JOIN osztaly_csoport oc ON oc.csoport_id=o.csoport_id"
    " JOIN osztaly os ON os.id=oc.osztaly_id"
    " WHERE os.evfolyam=? AND os.betujel=?" ORDER_BY;

static const char SQL_TEREM[] =
    "SELECT " DAY_COLS ", t.nev AS tanar" LESSON_JOINS
    " WHERE te.nev LIKE ?" ORDER_BY;

static const char SQL_SCHEMA[] =
    "SELECT table_name, column_name, column_type FROM "
    "information_schema.columns"
    " WHERE table_schema = DATABASE() ORDER BY table_name, ordinal_position";

static void usage(FILE *out) {
    fputs("Usage: orarend <command> [args]\n"
          "  tanar <name>             lessons of a teacher\n"
          "  osztaly <year> <letter>  timetable of a class (e.g. 12 A)\n"
          "  terem <number|name>      lessons in a room\n"
          "  schema                   tables and columns\n"
          "  sql \"<query>\"            run any SQL query\n"
          "  shell                    interactive mariadb client\n"
          "  help                     this text\n",
          out);
}

static int terem_pattern(const char *arg, char *out, size_t size) {
    int digits = arg[0] != '\0';
    for (const char *p = arg; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            digits = 0;
        }
    }
    int n = digits ? snprintf(out, size, "%s. terem%%", arg)
                   : snprintf(out, size, "%%%s%%", arg);
    return (n < 0 || (size_t)n >= size) ? -1 : 0;
}

static int run_shell(void) {
    execlp("mariadb", "mariadb", "-u", "szakos", "orarend", (char *)NULL);
    perror("exec mariadb");
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    const char *cmd = argv[1];
    char **arg = argv + 2;
    int n = argc - 2;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
        strcmp(cmd, "-h") == 0) {
        usage(stdout);
        return 0;
    }
    if (strcmp(cmd, "shell") == 0 && n == 0) {
        return run_shell();
    }

    const char *sql = NULL;
    const char *params[2];
    unsigned nparams = 0;
    int raw = 0;
    char pattern[256];

    if (strcmp(cmd, "tanar") == 0 && n == 1) {
        sql = SQL_TANAR;
        params[nparams++] = arg[0];
    } else if (strcmp(cmd, "osztaly") == 0 && n == 2) {
        sql = SQL_OSZTALY;
        params[nparams++] = arg[0];
        params[nparams++] = arg[1];
    } else if (strcmp(cmd, "terem") == 0 && n == 1) {
        if (terem_pattern(arg[0], pattern, sizeof pattern) != 0) {
            fprintf(stderr, "room name too long\n");
            return 2;
        }
        sql = SQL_TEREM;
        params[nparams++] = pattern;
    } else if (strcmp(cmd, "schema") == 0 && n == 0) {
        sql = SQL_SCHEMA;
    } else if (strcmp(cmd, "sql") == 0 && n == 1) {
        sql = arg[0];
        raw = 1;
    } else {
        usage(stderr);
        return 2;
    }

    MYSQL *conn = db_connect();
    if (conn == NULL) {
        return 1;
    }
    int rc = raw ? db_run_sql(conn, sql)
                 : db_run_prepared(conn, sql, params, nparams);
    mysql_close(conn);
    return rc == 0 ? 0 : 1;
}
