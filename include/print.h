#ifndef PRINT_H
#define PRINT_H

#include <mysql.h>
#include <stddef.h>
#include <stdio.h>

size_t utf8_width(const char *s);

/* 0 on success, -1 on out-of-memory. */
int print_table(FILE *out, size_t ncols, const char *const *headers,
                const char *const *cells, size_t nrows);

/* 0 on success, -1 on error. */
int print_result(MYSQL_RES *res);

#endif // !PRINT_H
