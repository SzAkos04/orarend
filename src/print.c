#include "print.h"

#include <stdlib.h>

size_t utf8_width(const char *s) {
    size_t w = 0;
    for (; *s != '\0'; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) {
            w++;
        }
    }
    return w;
}

static const char *cell_text(const char *s) { return s != NULL ? s : "NULL"; }

static void put_row(FILE *out, size_t ncols, const size_t *w,
                    const char *const *row) {
    for (size_t c = 0; c < ncols; c++) {
        const char *s = cell_text(row[c]);
        fputs(s, out);
        if (c + 1 < ncols) {
            fprintf(out, "%*s", (int)(w[c] - utf8_width(s) + 2), "");
        }
    }
    fputc('\n', out);
}

static void put_rule(FILE *out, size_t ncols, const size_t *w) {
    for (size_t c = 0; c < ncols; c++) {
        for (size_t i = 0; i < w[c]; i++) {
            fputc('-', out);
        }
        if (c + 1 < ncols) {
            fputs("  ", out);
        }
    }
    fputc('\n', out);
}

int print_table(FILE *out, size_t ncols, const char *const *headers,
                const char *const *cells, size_t nrows) {
    size_t *w = calloc(ncols + 1, sizeof *w);
    if (w == NULL) {
        fprintf(stderr, "out of memory\n");
        return -1;
    }

    for (size_t c = 0; c < ncols; c++) {
        w[c] = utf8_width(cell_text(headers[c]));
    }
    for (size_t r = 0; r < nrows; r++) {
        for (size_t c = 0; c < ncols; c++) {
            size_t x = utf8_width(cell_text(cells[r * ncols + c]));
            if (x > w[c]) {
                w[c] = x;
            }
        }
    }

    put_row(out, ncols, w, headers);
    put_rule(out, ncols, w);
    for (size_t r = 0; r < nrows; r++) {
        put_row(out, ncols, w, cells + r * ncols);
    }
    fprintf(out, "(%zu %s)\n", nrows, nrows == 1 ? "row" : "rows");

    free(w);
    return 0;
}

int print_result(MYSQL_RES *res) {
    size_t ncols = mysql_num_fields(res);
    size_t nrows = (size_t)mysql_num_rows(res);
    MYSQL_FIELD *fields = mysql_fetch_fields(res);
    const char **headers = calloc(ncols + 1, sizeof *headers);
    const char **cells = calloc(nrows * ncols + 1, sizeof *cells);
    int rc = -1;

    if (headers == NULL || cells == NULL) {
        fprintf(stderr, "out of memory\n");
        goto done;
    }
    for (size_t c = 0; c < ncols; c++) {
        headers[c] = fields[c].name;
    }
    for (size_t r = 0; r < nrows; r++) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == NULL) {
            fprintf(stderr, "row fetch failed\n");
            goto done;
        }
        for (size_t c = 0; c < ncols; c++) {
            cells[r * ncols + c] = row[c];
        }
    }
    rc = print_table(stdout, ncols, headers, cells, nrows);

done:
    free(headers);
    free(cells);
    return rc;
}
