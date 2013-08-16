/**
 * Copyright (C) 2005-2013 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 *
 *
 * A simple example which connects to a hamsterdb server (see server1.c),
 * creates a database, inserts some values, looks them up and erases them.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* for exit() */
#include <pthread.h>
#include <ham/hamsterdb.h>

#define LOOP 1000
#define NUM_THREADS 2

#define error(foo, st)  report_error(__FILE__, __LINE__, foo, st)

static void
report_error(const char *file, int line, const char *foo, ham_status_t st) {
  printf("%s:%d %s() returned error %d: %s\n", file, line, foo,
                  st, ham_strerror(st));
  _exit(-1);
}

void 
run() {
  int i;
  ham_status_t st;      /* status variable */
  ham_env_t *env;       /* hamsterdb Environment object */
  ham_db_t *db;         /* hamsterdb Database object */
  ham_key_t key = {0};     /* the structure for a key */
  ham_record_t record = {0};   /* the structure for a record */
  ham_cursor_t *cursor1, *cursor2;

  /*
   * Connect to the server which should listen at 8080. The server is
   * implemented in server1.c.
   */
  st = ham_env_create(&env, "ham://localhost:8080/env1.db", 0, 0, 0);
  if (st != HAM_SUCCESS)
    error("ham_env_create", st);

  /* now open a Database in this Environment */
  st = ham_env_open_db(env, &db, 13, 0, 0);
  if (st != HAM_SUCCESS)
    error("ham_env_open_db", st);

  /* create a cursor */
  st = ham_cursor_create(&cursor1, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);
  st = ham_cursor_create(&cursor2, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);

  /* now we can insert, delete or lookup values in the database */
  for (i = 0; i < LOOP; i++) {
    key.data = &i;
    key.size = sizeof(i);

    record.size = key.size;
    record.data = key.data;

    printf("inserting %d\n", i);
    st = ham_cursor_insert(cursor1, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_insert", st);
  }
  for (i = LOOP; i < LOOP * 2; i++) {
    key.data = &i;
    key.size = sizeof(i);

    record.size = key.size;
    record.data = key.data;

    printf("inserting %d\n", i);
    st = ham_cursor_insert(cursor2, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_insert", st);
  }

#if 0
  /* re-open the cursor */
  st = ham_cursor_close(cursor);
  if (st)
    error("ham_cursor_close", st);
  st = ham_cursor_create(&cursor, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);
#endif

  /* now lookup all values */
  for (i = 0; i < LOOP; i++) {
    key.data = &i;
    key.size = sizeof(i);

    printf("finding %d\n", i);
    st = ham_cursor_find(cursor1, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_find", st);

    /* check if the value is ok */
    if (*(int *)record.data != i) {
      printf("ham_cursor_find() ok, but returned bad value\n");
      return;
    }
  }
  for (i = LOOP; i < LOOP * 2; i++) {
    key.data = &i;
    key.size = sizeof(i);

    printf("finding %d\n", i);
    st = ham_cursor_find(cursor2, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_find", st);

    /* check if the value is ok */
    if (*(int *)record.data != i) {
      printf("ham_cursor_find() ok, but returned bad value\n");
      return;
    }
  }

#if 0
  /* re-open the cursor */
  st = ham_cursor_close(cursor);
  if (st)
    error("ham_cursor_close", st);
  st = ham_cursor_create(&cursor, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);
#endif

  /* erase everything */
  for (i = 0; i < LOOP; i++) {
    int k1 = i, k2 = i + LOOP;
    key.size = sizeof(int);

    key.data = &k2;
    printf("2: finding %d\n", k2);
    st = ham_cursor_find(cursor2, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_find", st);

    key.data = &k1;
    printf("1: finding %d\n", k1);
    st = ham_cursor_find(cursor1, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_find", st);

    if (k1 == 768)
      printf("hit in line %d\n", __LINE__);
    printf("1: erasing %d\n", k1);
    st = ham_cursor_erase(cursor1, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_erase1", st);
    printf("2: erasing %d\n", k2);
    st = ham_cursor_erase(cursor2, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_erase2", st);
  }

#if 0
  /* re-open the cursor */
  st = ham_cursor_close(cursor);
  if (st)
    error("ham_cursor_close", st);
  st = ham_cursor_create(&cursor, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);
#endif

  /* and make sure that the database is empty */
  for (i = 0; i < LOOP; i++) {
    key.data = &i;
    key.size = sizeof(i);

    st = ham_cursor_find(cursor1, &key, &record, 0);
    if (st != HAM_KEY_NOT_FOUND)
      error("ham_cursor_find", st);
  }
  for (i = LOOP; i < LOOP * 2; i++) {
    key.data = &i;
    key.size = sizeof(i);

    st = ham_cursor_find(cursor2, &key, &record, 0);
    if (st != HAM_KEY_NOT_FOUND)
      error("ham_cursor_find", st);
  }

  /* close the cursor */
  st = ham_cursor_close(cursor1);
  if (st)
    error("ham_cursor_close", st);
  st = ham_cursor_close(cursor2);
  if (st)
    error("ham_cursor_close", st);

  /* close the database handle */
  st = ham_db_close(db, 0);
  if (st != HAM_SUCCESS)
    error("ham_db_close", st);

  /* close the environment handle */
  st = ham_env_close(env, 0);
  if (st != HAM_SUCCESS)
    error("ham_env_close", st);
}

int
main(int argc, char **argv) {
  run();
  return (0);
}

