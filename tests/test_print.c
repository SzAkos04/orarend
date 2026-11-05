#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "print.h"

static void test_width(void) {
    assert(utf8_width("") == 0);
    assert(utf8_width("abc") == 3);
    assert(utf8_width("Ábel") == 4);
    assert(utf8_width("ő") == 1);
    assert(utf8_width("Fonyóné Németh") == 14);
}

static void test_table(void) {
    const char *headers[] = {"nap", "ora"};
    const char *cells[] = {
        "Mon", "1", "Éva", NULL, "Ő", "x",
    };
    char *buf = NULL;
    size_t size = 0;
    FILE *f = open_memstream(&buf, &size);
    assert(f != NULL);

    assert(print_table(f, 2, headers, cells, 3) == 0);
    fclose(f);

    const char *want = "nap  ora\n"
                       "---  ----\n"
                       "Mon  1\n"
                       "Éva  NULL\n"
                       "Ő    x\n"
                       "(3 rows)\n";
    assert(strcmp(buf, want) == 0);
    free(buf);
}

int main(void) {
    test_width();
    test_table();
    puts("print tests ok");
    return 0;
}
