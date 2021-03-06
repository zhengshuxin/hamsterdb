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


#ifndef DATASOURCE_STRING_H__
#define DATASOURCE_STRING_H__

#include <string>
#include <fstream>
#include <boost/limits.hpp>
#include <boost/random.hpp>
#include <boost/random/uniform_01.hpp>

// The file with the (sorted) words
#ifdef WIN32
#  define DICT "words"
#  undef min   // clashes with std::min
#  undef max   // clashes with std::max
#else
#  define DICT "/usr/share/dict/words"
#endif

//
// abstract base class for a data source - generates test data
//
class StringRandomDatasource : public Datasource
{
  public:
    StringRandomDatasource(int size, bool fixed_size, unsigned int seed = 0)
      : m_size(size), m_fixed_size(fixed_size) {
      if (seed)
        m_rng.seed(seed);
      std::ifstream infile(DICT);
      std::string line;
      while (std::getline(infile, line)) {
        m_data.push_back(line);
      }
      if (m_data.size() == 0) {
        printf("Sorry, %s seems to be empty or does not exist\n", DICT);
        exit(-1);
      }
    }

    // returns the next piece of data
    virtual void get_next(std::vector<uint8_t> &vec) {
      vec.clear();
      int pos = m_rng() % m_data.size();
      size_t i;
      for (i = 0; i < std::min(m_size, m_data[pos].size()); i++)
        vec.push_back(m_data[pos][i]);

      while (m_fixed_size && vec.size() < m_size) {
        vec.push_back(' ');
        pos = m_rng() % m_data.size();
        for (i = 0; vec.size() < m_size && i < m_data[pos].size(); i++)
          vec.push_back(m_data[pos][i]);
      }
    }

  private:
    boost::mt19937 m_rng;
    std::vector<std::string> m_data;
    size_t m_size;
    bool m_fixed_size;
};

class StringAscendingDatasource : public Datasource
{
  public:
    StringAscendingDatasource(int size, bool fixed_size)
      : m_size(size), m_next(0), m_fixed_size(fixed_size) {
      std::ifstream infile(DICT);
      std::string line;
      while (std::getline(infile, line)) {
        m_data.push_back(line);
      }
      if (m_data.size() == 0) {
        printf("Sorry, %s seems to be empty or does not exist\n", DICT);
        exit(-1);
      }
    }

    // returns the next piece of data; overflows are ignored
    virtual void get_next(std::vector<uint8_t> &vec) {
      vec.clear();
      size_t i;
      for (i = 0; i < std::min(m_data[m_next].size(), m_size); i++)
        vec.push_back(m_data[m_next][i]);
      if (m_fixed_size) {
        for (; i < m_size; i++)
          vec.push_back(' ');
      }
      if (++m_next == m_data.size())
        m_next = 0;
    }

  private:
    size_t m_size;
    size_t m_next;
    std::vector<std::string> m_data;
    bool m_fixed_size;
};

class StringDescendingDatasource : public Datasource
{
  public:
    StringDescendingDatasource(int size, bool fixed_size)
      : m_size(size), m_fixed_size(fixed_size) {
      std::ifstream infile(DICT);
      std::string line;
      while (std::getline(infile, line)) {
        m_data.push_back(line);
      }
      if (m_data.size() == 0) {
        printf("Sorry, %s seems to be empty or does not exist\n", DICT);
        exit(-1);
      }
      m_next = m_data.size() - 1;
    }

    // returns the next piece of data; overflows are ignored
    virtual void get_next(std::vector<uint8_t> &vec) {
      vec.clear();
      size_t i;
      for (i = 0; i < std::min(m_data[m_next].size(), m_size); i++)
        vec.push_back(m_data[m_next][i]);
      if (m_fixed_size) {
        for (; i < m_size; i++)
          vec.push_back(' ');
      }
      if (m_next == 0)
        m_next = m_data.size() - 1;
      else
        m_next--;
    }

  private:
    size_t m_size;
    size_t m_next;
    std::vector<std::string> m_data;
    bool m_fixed_size;
};

// Zipfian distribution is based on
// http://www.cse.usf.edu/~christen/tools/toolpage.html
class StringZipfianDatasource : public Datasource
{
  // vorberechnen eines datenstroms, der groß genug ist um daraus die
  // ganzen werte abzuleiten (N * size)
  // dann eine NumericZipfianDatasource erzeugen und in diesem binary
  // array entsprechend die daten rauskopieren
  public:
    StringZipfianDatasource(uint64_t n, size_t size, bool fixed_size,
            long seed = 0, double alpha = 0.8)
      : m_size(size), m_fixed_size(fixed_size), m_zipf(n, seed, alpha) {
      if (seed)
        m_rng.seed(seed);
      std::ifstream infile(DICT);
      std::string line;
      while (std::getline(infile, line)) {
        m_data.push_back(line);
      }
      if (m_data.size() == 0) {
        printf("Sorry, %s seems to be empty or does not exist\n", DICT);
        exit(-1);
      }
    }

    // returns the next piece of data
    virtual void get_next(std::vector<uint8_t> &vec) {
      vec.clear();
      size_t i;
      int pos = m_zipf.get_next_number() % m_data.size(); 
      for (i = 0; i < std::min(m_size, m_data[pos].size()); i++)
        vec.push_back(m_data[pos][i]);

      if (m_fixed_size) {
        for (; i < m_size; i++)
          vec.push_back(' ');
      }
    }

  private:
    boost::mt19937 m_rng;
    size_t m_size;
    bool m_fixed_size;
    NumericZipfianDatasource<int> m_zipf;
    std::vector<std::string> m_data;
};

#endif /* DATASOURCE_STRING_H__ */

