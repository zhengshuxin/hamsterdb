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

#define LOOP 5000
#define NUM_THREADS 20

#define error(foo, st)  report_error(__FILE__, __LINE__, foo, st)

static void
report_error(const char *file, int line, const char *foo, ham_status_t st) {
  printf("%s:%d %s() returned error %d: %s\n", file, line, foo,
                  st, ham_strerror(st));
  _exit(-1);
}

static void *
run(void *arg) {
  int i;
  int tid = *(int *)arg;
  ham_status_t st;      /* status variable */
  ham_env_t *env;       /* hamsterdb Environment object */
  ham_db_t *db;         /* hamsterdb Database object */
  ham_key_t key = {0};     /* the structure for a key */
  ham_record_t record = {0};   /* the structure for a record */
  ham_cursor_t *cursor;

  int start = tid * LOOP;
  int stop = start + LOOP;

  printf("starting thread #%d (%d - %d)\n", tid, start, stop);

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
  st = ham_cursor_create(&cursor, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);

  /* now we can insert, delete or lookup values in the database */
  for (i = start; i < stop; i++) {
    key.data = &i;
    key.size = sizeof(i);

    record.size = key.size;
    record.data = key.data;

    st = ham_cursor_insert(cursor, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_insert", st);
  }

  /* re-open the cursor */
  st = ham_cursor_close(cursor);
  if (st)
    error("ham_cursor_close", st);
  st = ham_cursor_create(&cursor, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);

  /* now lookup all values */
  for (i = start; i < stop; i++) {
    key.data = &i;
    key.size = sizeof(i);

    st = ham_cursor_find(cursor, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_find", st);

    /* check if the value is ok */
    if (*(int *)record.data != i) {
      printf("ham_cursor_find() ok, but returned bad value\n");
      return (0);
    }
  }

  /* re-open the cursor */
  st = ham_cursor_close(cursor);
  if (st)
    error("ham_cursor_close", st);
  st = ham_cursor_create(&cursor, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);

  /* erase everything */
  for (i = start; i < stop; i++) {
    key.data = &i;
    key.size = sizeof(i);

    st = ham_cursor_find(cursor, &key, &record, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_find", st);
    st = ham_cursor_erase(cursor, 0);
    if (st != HAM_SUCCESS)
      error("ham_cursor_erase", st);
  }

  /* re-open the cursor */
  st = ham_cursor_close(cursor);
  if (st)
    error("ham_cursor_close", st);
  st = ham_cursor_create(&cursor, db, 0, 0);
  if (st)
    error("ham_cursor_create", st);

  /* and make sure that the database is empty */
  for (i = start; i < stop; i++) {
    key.data = &i;
    key.size = sizeof(i);

    st = ham_cursor_find(cursor, &key, &record, 0);
    if (st != HAM_KEY_NOT_FOUND)
      error("ham_cursor_find", st);
  }

  /* close the cursor */
  st = ham_cursor_close(cursor);
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

  printf("stopping thread #%d\n", tid);
  return (0);
}

static void *
run5(void *arg) {
  run(arg);
  run(arg);
  run(arg);
  run(arg);
  run(arg);
}

int
main(int argc, char **argv) {
  pthread_t threads[NUM_THREADS];
  int args[NUM_THREADS];

  for (int i = 0; i < NUM_THREADS; i++) {
    args[i] = i;
    pthread_create(&threads[i], 0, run5, &args[i]);
  }

  for (int i = 0; i < NUM_THREADS; i++)
    pthread_join(threads[i], 0);
  return (0);
}

