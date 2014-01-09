/**
 * Copyright (C) 2005-2014 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 */

#include "../src/config.h"

#include "3rdparty/catch/catch.hpp"

#include "globals.h"

#include "../src/page.h"
#include "../src/device.h"
#include "../src/env.h"
#include "../src/txn.h"
#include "../src/config.h"
#include "../src/page_manager.h"

namespace hamsterdb {

struct PageManagerFixture {
  ham_db_t *m_db;
  ham_env_t *m_env;
  bool m_inmemory;
  Device *m_device;

  PageManagerFixture(bool inmemorydb = false, ham_u32_t cachesize = 0)
      : m_db(0), m_inmemory(inmemorydb), m_device(0) {
    ham_u32_t flags = 0;

    if (m_inmemory)
      flags |= HAM_IN_MEMORY;

    ham_parameter_t params[2] = {{0, 0}, {0, 0}};
    if (cachesize) {
      params[0].name = HAM_PARAM_CACHE_SIZE;
      params[0].value = cachesize;
    }

    REQUIRE(0 ==
        ham_env_create(&m_env, Globals::opath(".test"), flags,
                0644, &params[0]));
    REQUIRE(0 ==
        ham_env_create_db(m_env, &m_db, 1, 0, 0));
  }

  ~PageManagerFixture() {
    REQUIRE(0 == ham_env_close(m_env, HAM_AUTO_CLEANUP));
  }

  void newDeleteTest() {
    PageManager *pm = ((LocalEnvironment *)m_env)->get_page_manager();

    if (m_inmemory)
      REQUIRE(!pm->test_get_freelist());
  }

  void fetchPageTest() {
    PageManager *pm = ((LocalEnvironment *)m_env)->get_page_manager();
    Page *page;

    page = 0;
    REQUIRE((page = pm->fetch_page(0, 16 * 1024ull, false)));
    REQUIRE(page->get_address() == 16 * 1024ull);

    page = 0;
    REQUIRE((page = pm->fetch_page(0, 16 * 1024ull, true)));
    REQUIRE(page->get_address() == 16 * 1024ull);
    REQUIRE(page);
  }

  void allocPageTest() {
    PageManager *pm = ((LocalEnvironment *)m_env)->get_page_manager();
    Page *page;

    page = 0;
    REQUIRE((page = pm->alloc_page(0, Page::kTypeFreelist,
                PageManager::kClearWithZero)));
    if (m_inmemory == false)
      REQUIRE(page->get_address() == 2 * 16 * 1024ull);
    REQUIRE(page != 0);
    REQUIRE(!page->get_db());
  }

  void fetchInvalidPageTest() {
    PageManager *pm = ((LocalEnvironment *)m_env)->get_page_manager();
    REQUIRE_CATCH(pm->fetch_page(0, 1024 * 1024 * 200, false), HAM_IO_ERROR);
  }

  void setCacheSizeEnvCreate() {
    REQUIRE(0 == ham_env_close(m_env, HAM_AUTO_CLEANUP));

    ham_db_t *db = 0;
    ham_parameter_t param[] = {
      { HAM_PARAM_CACHE_SIZE, 100 * 1024 },
      { HAM_PARAM_PAGE_SIZE,  1024 },
      { 0, 0 }
    };

    REQUIRE(0 ==
        ham_env_create(&m_env, Globals::opath(".test"),  
            0, 0644, &param[0]));
    REQUIRE(0 ==
        ham_env_create_db(m_env, &db, 13, 0, 0));

    LocalEnvironment *lenv = (LocalEnvironment *)m_env;

    REQUIRE(102400ull == lenv->get_page_manager()->m_cache_size);
  }

  void setCacheSizeEnvOpen(ham_u64_t size) {
    REQUIRE(0 == ham_env_close(m_env, HAM_AUTO_CLEANUP));

    ham_parameter_t param[] = {
      { HAM_PARAM_CACHE_SIZE, size },
      { 0, 0 }
    };

    REQUIRE(0 ==
        ham_env_open(&m_env, Globals::opath(".test"),  
            0, &param[0]));

    LocalEnvironment *lenv = (LocalEnvironment *)m_env;

    REQUIRE(size == lenv->get_page_manager()->m_cache_size);
  }

  void cachePutGet() {
    LocalEnvironment *lenv = (LocalEnvironment *)m_env;

    PPageData pers;
    memset(&pers, 0, sizeof(pers));

    Page *page = new Page(lenv);
    page->set_address(0x123ull);
    page->set_data(&pers);
    page->set_flags(Page::kNpersNoHeader);

    lenv->get_page_manager()->store_page(page);
    REQUIRE(page == lenv->get_page_manager()->fetch_page(0x123ull));
    lenv->get_page_manager()->test_remove_page(page);

    page->set_data(0);
    delete page;
  }

  void cachePutGetRemove() {
    LocalEnvironment *lenv = (LocalEnvironment *)m_env;

    PPageData pers;
    memset(&pers, 0, sizeof(pers));

    Page *page = new Page(lenv);
    page->set_address(0x123ull);
    page->set_data(&pers);
    page->set_flags(Page::kNpersNoHeader);

    lenv->get_page_manager()->store_page(page);
    REQUIRE(page == lenv->get_page_manager()->fetch_page(0x123ull));
    REQUIRE(page == lenv->get_page_manager()->m_totallist);
    lenv->get_page_manager()->test_remove_page(page);
    REQUIRE((Page *)0 == lenv->get_page_manager()->fetch_page(0x123ull));

    REQUIRE(lenv->get_page_manager()->m_page_map.size() == 1); // header page

    page->set_data(0);
    delete page;
  }

  void cacheManyPuts() {
    LocalEnvironment *lenv = (LocalEnvironment *)m_env;
    Page *page[20];
    PPageData pers[20];

    for (int i = 0; i < 20; i++) {
      page[i] = new Page(lenv);
      memset(&pers[i], 0, sizeof(pers[i]));
      page[i]->set_flags(Page::kNpersNoHeader);
      page[i]->set_address(i + 1);
      page[i]->set_data(&pers[i]);
      lenv->get_page_manager()->store_page(page[i]);
    }
    for (int i = 0; i < 20; i++)
      REQUIRE(page[i] == lenv->get_page_manager()->fetch_page(i + 1));
    for (int i = 0; i < 20; i++)
      lenv->get_page_manager()->test_remove_page(page[i]);
    for (int i = 0; i < 20; i++) {
      REQUIRE((Page *)0 == lenv->get_page_manager()->fetch_page(i + 1));
      page[i]->set_data(0);
      delete page[i];
    }
  }

  void cacheNegativeGets() {
    LocalEnvironment *lenv = (LocalEnvironment *)m_env;

    for (int i = 0; i < 20; i++)
      REQUIRE((Page *)0 == lenv->get_page_manager()->fetch_page(i + 1));
  }

  void cacheFullTest() {
    LocalEnvironment *lenv = (LocalEnvironment *)m_env;

    PPageData pers;
    memset(&pers, 0, sizeof(pers));
    std::vector<Page *> v;

    for (unsigned int i = 0; i < 15; i++) {
      Page *p = new Page(lenv);
      p->set_flags(Page::kNpersNoHeader | Page::kNpersMalloc);
      p->set_address(i + 1);
      p->set_data(&pers);
      v.push_back(p);
      lenv->get_page_manager()->store_page(p);
      REQUIRE(false == lenv->get_page_manager()->cache_is_full());
    }

    for (unsigned int i = 0; i < 5; i++) {
      Page *p = new Page(lenv);
      p->set_flags(Page::kNpersNoHeader | Page::kNpersMalloc);
      p->set_address(i + 15 + 1);
      p->set_data(&pers);
      v.push_back(p);
      lenv->get_page_manager()->store_page(p);
      REQUIRE(true == lenv->get_page_manager()->cache_is_full());
    }

    for (unsigned int i = 0; i < 5; i++) {
      REQUIRE(true == lenv->get_page_manager()->cache_is_full());
      Page *p = v.back();
      v.pop_back();
      lenv->get_page_manager()->test_remove_page(p);
      p->set_data(0);
      delete p;
    }

    for (unsigned int i = 0; i < 15; i++) {
      Page *p = v.back();
      v.pop_back();
      lenv->get_page_manager()->test_remove_page(p);
      REQUIRE(false == lenv->get_page_manager()->cache_is_full());
      p->set_data(0);
      delete p;
    }

    REQUIRE(false == lenv->get_page_manager()->cache_is_full());
  }
};

TEST_CASE("PageManager/newDelete", "")
{
  PageManagerFixture f;
  f.newDeleteTest();
}

TEST_CASE("PageManager/fetchPage", "")
{
  PageManagerFixture f;
  f.fetchPageTest();
}

TEST_CASE("PageManager/allocPage", "")
{
  PageManagerFixture f;
  f.allocPageTest();
}

TEST_CASE("PageManager/fetchInvalidPage", "")
{
  PageManagerFixture f;
  f.fetchInvalidPageTest();
}

TEST_CASE("PageManager/setCacheSizeEnvCreate", "")
{
  PageManagerFixture f;
  f.setCacheSizeEnvCreate();
}

TEST_CASE("PageManager/setCacheSizeEnvOpen", "")
{
  PageManagerFixture f;
  f.setCacheSizeEnvOpen(100 * 1024);
}

TEST_CASE("PageManager/setBigCacheSizeEnvOpen", "")
{
  PageManagerFixture f;
  f.setCacheSizeEnvOpen(1024ull * 1024ull * 1024ull * 16ull);
}

TEST_CASE("PageManager/cachePutGet", "")
{
  PageManagerFixture f;
  f.cachePutGet();
}

TEST_CASE("PageManager/cachePutGetRemove", "")
{
  PageManagerFixture f;
  f.cachePutGetRemove();
}

TEST_CASE("PageManager/cacheManyPuts", "")
{
  PageManagerFixture f;
  f.cacheManyPuts();
}

TEST_CASE("PageManager/cacheNegativeGets", "")
{
  PageManagerFixture f;
  f.cacheNegativeGets();
}

TEST_CASE("PageManager/cacheFullTest", "")
{
  PageManagerFixture f(false, 16 * HAM_DEFAULT_PAGESIZE);
  f.cacheFullTest();
}

TEST_CASE("PageManager-inmem/newDelete", "")
{
  PageManagerFixture f(true);
  f.newDeleteTest();
}

TEST_CASE("PageManager-inmem/allocPage", "")
{
  PageManagerFixture f(true);
  f.allocPageTest();
}

} // namespace hamsterdb
