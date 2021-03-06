/*
 *  Copyright (C) 2009 Steve Harris
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  $Id: update.c $
 */

#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <pcre.h>

#include "update.h"
#include "import.h"
#include "common/4store.h"
#include "common/hash.h"
#include "common/error.h"

enum update_op {
    OP_LOAD, OP_CLEAR
};

typedef struct _update_operation {
    enum update_op op;
    char *arg1;
    char *arg2;
    struct _update_operation *next;
} update_operation;

static int inited = 0;
static pcre *re_ws = NULL;
static pcre *re_load = NULL;
static pcre *re_clear = NULL;

static void re_error(int rc)
{
    if (rc != PCRE_ERROR_NOMATCH) {
        fs_error(LOG_ERR, "PCRE error %d\n", rc);
    }
}

static update_operation *add_op(update_operation *head, enum update_op op, char *arg1, char *arg2)
{
    update_operation *tail = head;
    while (tail && tail->next) {
        tail = tail->next;
    }

    update_operation *newop = calloc(1, sizeof(update_operation));
    newop->op = op;
    newop->arg1 = g_strdup(arg1);
    if (arg2) {
        newop->arg2 = g_strdup(arg2);
    }
    if (tail) {
        tail->next = newop;
    
        return head;
    }

    return newop;
}

static void free_ops(update_operation *head)
{
    update_operation *ptr = head;
    while (ptr) {
        g_free(ptr->arg1);
        update_operation *next = ptr->next;
        free(ptr);
        ptr = next;
    }
}

int fs_update(fsp_link *l, char *update, char **message, int unsafe)
{
    if (!inited) {
        const char *errstr = NULL;
        int erroffset = 0;

        if ((re_ws = pcre_compile("^\\s+", PCRE_UTF8, &errstr, &erroffset,
                                  NULL)) == NULL) {
            fs_error(LOG_ERR, "pcre compile failed: %s", errstr);
        }
        if ((re_load = pcre_compile("^\\s*LOAD\\s*<(.*?)>(?:\\s+INTO\\s<(.*?)>)?",
             PCRE_CASELESS | PCRE_UTF8, &errstr, &erroffset, NULL)) == NULL) {
            fs_error(LOG_ERR, "pcre compile failed: %s", errstr);
        }
        if ((re_clear = pcre_compile("^\\s*(?:CLEAR|DROP)\\s*GRAPH\\s*<(.*?)>",
             PCRE_CASELESS | PCRE_UTF8, &errstr, &erroffset, NULL)) == NULL) {
            fs_error(LOG_ERR, "pcre compile failed: %s", errstr);
        }
    }

    char tmpa[4096];
    char tmpb[4096];
    int rc;
    int ovector[30];
    int length = strlen(update);
    char *scan = update;
    int match;
    update_operation *ops = NULL;
    GString *mstr = g_string_new("");

    do {
        match = 0;

        rc = pcre_exec(re_ws, NULL, scan, length, 0, 0, ovector, 30);
        if (rc > 0) {
            match = 1;
            scan += ovector[1];
            length -= ovector[1];
        }

        rc = pcre_exec(re_load, NULL, scan, length, 0, 0, ovector, 30);
        if (rc > 0) {
            match = 1;
            pcre_copy_substring(scan, ovector, rc, 1, tmpa, sizeof(tmpa));
            if (rc == 2) {
                ops = add_op(ops, OP_LOAD, tmpa, NULL);
            } else {
                pcre_copy_substring(scan, ovector, rc, 2, tmpb, sizeof(tmpb));
                ops = add_op(ops, OP_LOAD, tmpa, tmpb);
            }
            scan += ovector[1];
            length -= ovector[1];
        } else {
            re_error(rc);
        }

        rc = pcre_exec(re_clear, NULL, scan, length, 0, 0, ovector, 30);
        if (rc > 0) {
            match = 1;
            pcre_copy_substring(scan, ovector, rc, 1, tmpa, sizeof(tmpa));
            ops = add_op(ops, OP_CLEAR, tmpa, NULL);
            scan += ovector[1];
            length -= ovector[1];
        } else {
            re_error(rc);
        }
    } while (match);

    if (length > 0) {
        free_ops(ops);
        g_string_free(mstr, TRUE);
        *message = g_strdup_printf("Syntax error in SPARUL command near "
                                   "“%.40s”\n", scan);

        return 1;
    }

    int errors = 0;

    for (update_operation *ptr = ops; ptr; ptr = ptr->next) {
        switch (ptr->op) {
        case OP_LOAD:;
            FILE *errout = NULL;
            errout = tmpfile();
            int count = 0;
            if (fsp_start_import_all(l)) {
                errors++;
                g_string_append(mstr, "aborting import\n");
                fclose(errout);
                free_ops(ops);
                *message = mstr->str;
                g_string_free(mstr, FALSE);

                break;
            }

            char *model = ptr->arg2 ? ptr->arg2 : ptr->arg1;
            fs_import(l, model, ptr->arg1, "auto", 0, 0, 0, errout, &count);
            fs_import_commit(l, 0, 0, 0, errout, &count);
            fsp_stop_import_all(l);
            rewind(errout);
            char tmp[1024];
            if (fgets(tmp, 1024, errout)) {
                errors++;
                g_string_append(mstr, tmp);
            } else {
                if (ptr->arg2) {
                    g_string_append_printf(mstr, "Imported <%s> into <%s>\n",
                                           ptr->arg1, ptr->arg2);
                } else {
                    g_string_append_printf(mstr, "Imported <%s>\n", ptr->arg1);
                }
            }
            fclose(errout);
            break;
        case OP_CLEAR:;
            fs_rid_vector *mvec = fs_rid_vector_new(0);
            fs_rid muri = fs_hash_uri(ptr->arg1);
            fs_rid_vector_append(mvec, muri);

            if (fsp_delete_model_all(l, mvec)) {
                errors++;
                g_string_append_printf(mstr, "error while trying to delete %s\n", ptr->arg1);
            } else {
                g_string_append_printf(mstr, "Deleted <%s>\n", ptr->arg1);
            }
            fs_rid_vector_free(mvec);
            break;
        }
    }

    free_ops(ops);
    *message = mstr->str;
    g_string_free(mstr, FALSE);

    return errors;
}

/* vi:set expandtab sts=4 sw=4: */
