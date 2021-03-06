/*
 * Copyright (C) 2005-2013 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 */

#ifndef HAM_HAMSERVER_H__
#define HAM_HAMSERVER_H__

// otherwise uv.h includes <windows.h>, which includes
// <winsock.h>, which conflicts with <winsock2.h>
#define WIN32_LEAN_AND_MEAN

#include <vector>
#include <uv.h>

#include <ham/hamsterdb.h>
#include <ham/types.h>
#include <ham/hamsterdb_srv.h>

#include "../db.h"
#include "../mutex.h"
#include "../cursor.h"

struct ham_srv_t {
  bool dummy;
};

namespace hamsterdb {

class ErrorInducer;

template<typename T>
struct Handle {
  Handle(ham_u64_t _index, T *_object)
    : index(_index), object(_object) {
  }

  ham_u64_t index;
  T *object;
};

typedef std::vector< Handle<Environment> > EnvironmentVector;
typedef std::vector< Handle<Database> > DatabaseVector;
typedef std::vector< Handle<Cursor> > CursorVector;
typedef std::vector< Handle<Transaction> > TransactionVector;
typedef std::map<std::string, Environment *> EnvironmentMap;

class ServerContext {
  public:
    ServerContext()
      : thread_id(0), loop(0), m_inducer(0), m_handle_counter(1) {
      memset(&server, 0, sizeof(server));
      memset(&async, 0, sizeof(async));
    }

    // allocates a new handle
    // TODO the allocate_handle methods have lots of duplicate code;
    // try to find a generic solution!
    ham_u64_t allocate_handle(Environment *env) {
      ham_u64_t c = 0;
      for (EnvironmentVector::iterator it = m_environments.begin();
              it != m_environments.end(); it++, c++) {
        if (it->index == 0) {
          c |= m_handle_counter << 32;
          m_handle_counter++;
          it->index = c;
          it->object = env;
          return (c);
        }
      }

      c = m_environments.size() | m_handle_counter << 32;
      m_handle_counter++;
      m_environments.push_back(Handle<Environment>(c, env));
      return (c);
    }

    ham_u64_t allocate_handle(Database *db) {
      ham_u64_t c = 0;
      for (DatabaseVector::iterator it = m_databases.begin();
              it != m_databases.end(); it++, c++) {
        if (it->index == 0) {
          c |= m_handle_counter << 32;
          m_handle_counter++;
          it->index = c;
          it->object = db;
          return (c);
        }
      }

      c = m_databases.size() | m_handle_counter << 32;
      m_handle_counter++;
      m_databases.push_back(Handle<Database>(c, db));
      return (c);
    }

    ham_u64_t allocate_handle(Transaction *txn) {
      ham_u64_t c = 0;
      for (TransactionVector::iterator it = m_transactions.begin();
              it != m_transactions.end(); it++, c++) {
        if (it->index == 0) {
          c |= m_handle_counter << 32;
          m_handle_counter++;
          it->index = c;
          it->object = txn;
          return (c);
        }
      }

      c = m_transactions.size() | m_handle_counter << 32;
      m_handle_counter++;
      m_transactions.push_back(Handle<Transaction>(c, txn));
      return (c);
    }

    ham_u64_t allocate_handle(Cursor *cursor) {
      ham_u64_t c = 0;
      for (CursorVector::iterator it = m_cursors.begin();
              it != m_cursors.end(); it++, c++) {
        if (it->index == 0) {
          c |= m_handle_counter << 32;
          m_handle_counter++;
          it->index = c;
          it->object = cursor;
          return (c);
        }
      }

      c = m_cursors.size() | m_handle_counter << 32;
      m_handle_counter++;
      m_cursors.push_back(Handle<Cursor>(c, cursor));
      return (c);
    }

    void remove_env_handle(ham_u64_t handle) {
      ham_u32_t index = handle & 0xffffffff;
      ham_assert(index < m_environments.size());
      if (index >= m_environments.size())
        return;
      EnvironmentVector::iterator it = m_environments.begin() + index;
      ham_assert(it->index == handle);
      if (it->index != handle)
        return;
      it->index = 0;
      it->object = 0;
    }

    void remove_db_handle(ham_u64_t handle) {
      ham_u32_t index = handle & 0xffffffff;
      ham_assert(index < m_databases.size());
      if (index >= m_databases.size())
        return;
      DatabaseVector::iterator it = m_databases.begin() + index;
      ham_assert(it->index == handle);
      if (it->index != handle)
        return;
      it->index = 0;
      it->object = 0;
    }

    void remove_txn_handle(ham_u64_t handle) {
      ham_u32_t index = handle & 0xffffffff;
      ham_assert(index < m_transactions.size());
      if (index >= m_transactions.size())
        return;
      TransactionVector::iterator it = m_transactions.begin() + index;
      ham_assert(it->index == handle);
      if (it->index != handle)
        return;
      it->index = 0;
      it->object = 0;
    }

    void remove_cursor_handle(ham_u64_t handle) {
      ham_u32_t index = handle & 0xffffffff;
      ham_assert(index < m_cursors.size());
      if (index >= m_cursors.size())
        return;
      CursorVector::iterator it = m_cursors.begin() + index;
      ham_assert(it->index == handle);
      if (it->index != handle)
        return;
      it->index = 0;
      it->object = 0;
    }

    Environment *get_env(ham_u64_t handle) {
      ham_u32_t index = handle & 0xffffffff;
      ham_assert(index < m_environments.size());
      if (index >= m_environments.size())
        return (0);
      EnvironmentVector::iterator it = m_environments.begin() + index;
      ham_assert(it->index == handle);
      if (it->index != handle)
        return (0);
      return (it->object);
    }

    Database *get_db(ham_u64_t handle) {
      ham_u32_t index = handle & 0xffffffff;
      ham_assert(index < m_databases.size());
      if (index >= m_databases.size())
        return (0);
      DatabaseVector::iterator it = m_databases.begin() + index;
      ham_assert(it->index == handle);
      if (it->index != handle)
        return (0);
      return (it->object);
    }

    Transaction *get_txn(ham_u64_t handle) {
      ham_u32_t index = handle & 0xffffffff;
      ham_assert(index < m_transactions.size());
      if (index >= m_transactions.size())
        return (0);
      TransactionVector::iterator it = m_transactions.begin() + index;
      ham_assert(it->index == handle);
      if (it->index != handle)
        return (0);
      return (it->object);
    }

    Cursor *get_cursor(ham_u64_t handle) {
      ham_u32_t index = handle & 0xffffffff;
      ham_assert(index < m_cursors.size());
      if (index >= m_cursors.size())
        return (0);
      CursorVector::iterator it = m_cursors.begin() + index;
      ham_assert(it->index == handle);
      if (it->index != handle)
        return (0);
      return (it->object);
    }

    Handle<Database> get_db_by_name(ham_u16_t dbname) {
      for (size_t i = 0; i < m_databases.size(); i++) {
        Database *db = m_databases[i].object;
        if (db && db->get_name() == dbname)
          return (m_databases[i]);
      }
      return (Handle<Database>(0, 0));
    }

    uv_tcp_t server;
    uv_thread_t thread_id;
    uv_async_t async;
	uv_loop_t *loop;
    EnvironmentMap open_envs;
    ErrorInducer *m_inducer;

    Mutex open_queue_mutex;
    EnvironmentMap open_queue;

  private:
    EnvironmentVector m_environments;
    DatabaseVector m_databases;
    CursorVector m_cursors;
    TransactionVector m_transactions;
    ham_u64_t m_handle_counter;
};

struct ClientContext {
  ClientContext(ServerContext *_srv)
    : buffer(0), srv(_srv) {
    ham_assert(srv != 0);
  }

  ByteArray buffer;
  ServerContext *srv;
};


} // namespace hamsterdb

#endif /* HAM_HAMSERVER_H__ */
