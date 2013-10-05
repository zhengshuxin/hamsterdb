/**
 * Copyright (C) 2005-2013 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 */

#ifndef GENERATOR_H__
#define GENERATOR_H__

#include "database.h"
#include "graph.h"

class Configuration;

//
// base class for generating test data
//
class Generator
{
  public:
    enum {
      kCommandInsert = 0,
      kCommandFind,
      kCommandErase,
      kCommandCommitTransaction,
      kCommandTablescan,
      kCommandCreate,
      kCommandOpen,
      kCommandClose,
      kCommandBeginTransaction,
      kCommandAbortTransaction,
      kCommandFlush,
      kCommandNop,
      kCommandFullcheck = 999999 // avoid conflicts with ham_status_t
    };

    // constructor
    Generator(int id, Configuration *conf, Database *db)
      : m_id(id), m_config(conf), m_db(db), m_last_status(0), m_graph(0) {
      memset(&m_record, 0, sizeof(m_record));
      memset(&m_opspersec, 0, sizeof(m_opspersec));

      // only create graph output for the first hamsterdb thread!
      if (conf->metrics >= Configuration::kMetricsPng
           && !strcmp(db->get_name(), "hamsterdb")
           && id == 0) {
        m_graph = new Graph("hamsterdb");
      }
    }

    // destructor
    virtual ~Generator() {
      if (m_graph) {
        delete m_graph;
        m_graph = 0;
      }
    }

    // Returns the database
    Database *get_db() {
      return (m_db);
    }
    
    // Returns the status of the last database operation
    ham_status_t get_status() const {
      return (m_last_status);
    }

    // executes the next generated statement; returns true if more statements
    // will follow, false when the test is done
    virtual bool execute() = 0;

    // opens the Environment; used for 'reopen'
    virtual void open() = 0;

    // closes the Environment; used for 'reopen'
    virtual void close() = 0;

    // returns true if the test was successful
    virtual bool was_successful() const = 0;

    // returns the collected metrics/statistics
    virtual void get_metrics(Metrics *metrics) = 0;

    // Returns the previously retrieved record
    const ham_record_t *get_record() const {
      return (&m_record);
    }

  protected:
    // unique ID - used to create the database
    int m_id;

    // the configuration settings
    Configuration *m_config;

    // the Database
    Database *m_db;

    // the status of the previous database operation
    ham_status_t m_last_status;

    // A record to store data from the last call to find() and
    // cursor_find(); used by the caller to compare results from
    // different databases
    ham_record_t m_record;

    // the performance graphs
    Graph *m_graph;

    // accumulating operations-per-seconds for the graphs
    uint32_t m_opspersec[4];
};

#endif /* GENERATOR_H__ */

