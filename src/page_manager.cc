/*
 * Copyright (C) 2005-2014 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 */

#include <string.h>

#include "util.h"
#include "page.h"
#include "device.h"
#include "btree_index.h"
#include "btree_node_proxy.h"

#include "page_manager.h"

namespace hamsterdb {

PageManager::PageManager(LocalEnvironment *env, ham_u64_t cache_size)
  : m_env(env), m_freelist(0), m_cache_size(cache_size), m_needs_flush(false),
    m_blobid(0), m_totallist(0), m_totallist_tail(0), m_page_count_fetched(0),
    m_page_count_flushed(0), m_page_count_index(0), m_page_count_blob(0),
    m_page_count_freelist(0), m_cache_hits(0), m_cache_misses(0)
{
}

PageManager::~PageManager()
{
  if (m_freelist) {
    delete m_freelist;
    m_freelist = 0;
  }
}

void
PageManager::load_state(ham_u64_t blobid)
{
  m_blobid = blobid;

  ByteArray buffer;
  ham_record_t rec = {0};

  // read the blob
  m_env->get_blob_manager()->read(0, blobid, &rec, 0, &buffer);

  ham_u8_t *p = (ham_u8_t *)rec.data;

  // set up the state
  ham_u32_t counter = ham_db2h32(*(ham_u32_t *)p);
  p += 4;

  for (ham_u32_t i = 0; i < counter; i++) {
    ham_u64_t id = ham_db2h64(*(ham_u64_t *)p);
    p += 8;
    bool is_free = *p ? true : false;
    p += 1;
    ham_assert(p - (ham_u8_t *)rec.data <= rec.size);

    PageState ps = PageState(0);
    ps.is_free = is_free;
    m_page_map[id] = ps;
  }
}

ham_u64_t
PageManager::store_state()
{
  // no modifications? then simply return the old blobid
  if (!m_needs_flush)
    return (m_blobid);
  m_needs_flush = false;

  ByteArray buffer(m_page_map.size() * 9 + 4);

  ham_u8_t *p = (ham_u8_t *)buffer.get_ptr();

  // store the number of elements
  *(ham_u32_t *)p = ham_h2db32(m_page_map.size());
  p += 4;

  for (PageMap::const_iterator it = m_page_map.begin();
                  it != m_page_map.end(); it++) {
    *(ham_u64_t *)p = ham_h2db64(it->first);
    p += 8;
    *p = it->second.is_free ? 1 : 0;
    p += 1;
  }

  ham_record_t rec = {0};
  rec.data = buffer.get_ptr();
  rec.size = buffer.get_size();

  if (m_blobid)
    return (m_env->get_blob_manager()->overwrite(0, m_blobid, &rec, 0));
  else
    return (m_env->get_blob_manager()->allocate(0, &rec, 0));
}

void
PageManager::get_metrics(ham_env_metrics_t *metrics) const
{
  metrics->page_count_fetched = m_page_count_fetched;
  metrics->page_count_flushed = m_page_count_flushed;
  metrics->page_count_type_index = m_page_count_index;
  metrics->page_count_type_blob = m_page_count_blob;
  metrics->page_count_type_freelist= m_page_count_freelist;
  metrics->cache_hits = m_cache_hits;
  metrics->cache_misses = m_cache_misses;

  if (m_freelist)
    m_freelist->get_metrics(metrics);
}

Page *
PageManager::fetch_page(LocalDatabase *db, ham_u64_t address,
                bool only_from_cache)
{
  /* fetch the page from our list */
  Page *page = fetch_page(address);
  if (page) {
    m_cache_hits++;

    ham_assert(page->get_data());
    /* store the page in the changeset if recovery is enabled */
    if (m_env->get_flags() & HAM_ENABLE_RECOVERY)
      m_env->get_changeset().add_page(page);
    return (page);
  }

  m_cache_misses++;

  if (only_from_cache || m_env->get_flags() & HAM_IN_MEMORY)
    return (0);

  page = new Page(m_env, db);
  try {
    page->fetch(address);
  }
  catch (Exception &ex) {
    delete page;
    throw ex;
  }

  ham_assert(page->get_data());

  /* store the page in the list */
  store_page(page);

  /* store the page in the changeset */
  if (m_env->get_flags() & HAM_ENABLE_RECOVERY)
    m_env->get_changeset().add_page(page);

  m_page_count_fetched++;

  return (page);
}

Page *
PageManager::alloc_page(LocalDatabase *db, ham_u32_t page_type, ham_u32_t flags)
{
  ham_u64_t freelist = 0;
  Page *page = 0;

  ham_assert(0 == (flags & ~(PageManager::kIgnoreFreelist
                                | PageManager::kClearWithZero)));

  /* first, we ask the freelist for a page */
  if (!(flags & PageManager::kIgnoreFreelist) && m_freelist) {
    /* check the internal list for a free page */
    for (PageMap::iterator it = m_page_map.begin();
                  it != m_page_map.end(); it++) {
      if (it->second.is_free) {
        it->second.is_free = false;
        m_needs_flush = true;
        maybe_store_state();

        page = it->second.page;
        if (page)
          goto done;
        freelist = it->first;
        break;
      }
    }

    if (freelist == 0)
      freelist = m_freelist->alloc_page();
    if (freelist > 0) {
      ham_assert(freelist % m_env->get_page_size() == 0);
      /* try to fetch the page from the cache */
      page = fetch_page(freelist);
      if (page)
        goto done;
      /* allocate a new page structure and read the page from disk */
      page = new Page(m_env, db);
      page->fetch(freelist);
      goto done;
    }
  }

  if (!page)
    page = new Page(m_env, db);

  ham_assert(freelist == 0);
  page->allocate();

done:
  /* clear the page with zeroes?  */
  if (flags & PageManager::kClearWithZero)
    memset(page->get_data(), 0, m_env->get_page_size());

  /* initialize the page; also set the 'dirty' flag to force logging */
  page->set_type(page_type);
  page->set_dirty(true);
  page->set_db(db);

  /* an allocated page is always flushed if recovery is enabled */
  if (m_env->get_flags() & HAM_ENABLE_RECOVERY)
    m_env->get_changeset().add_page(page);

  /* store the page in the cache */
  store_page(page);

  switch (page_type) {
    case Page::kTypeBindex:
    case Page::kTypeBroot: {
      memset(page->get_payload(), 0, sizeof(PBtreeNode));
      m_page_count_index++;
      break;
    }
    case Page::kTypeFreelist:
      m_page_count_freelist++;
      break;
    case Page::kTypeBlob:
      m_page_count_blob++;
      break;
    default:
      break;
  }

  return (page);
}

ham_u64_t
PageManager::alloc_blob(Database *db, ham_u32_t size, bool *pallocated)
{
  ham_u64_t address = 0;

  if (pallocated)
    *pallocated = false;

  // first check the freelist
  if (m_freelist)
    address = m_freelist->alloc_area(size);

  return (address);
}

void
PageManager::flush_all_pages(bool nodelete)
{
  for (PageMap::iterator it = m_page_map.begin();
                  it != m_page_map.end(); it++) {
    if (!it->second.page)
      continue;
    flush_page(it->second.page);
    // if the page will be deleted then uncouple all cursors
    if (!nodelete) {
      BtreeCursor::uncouple_all_cursors(it->second.page);
      delete it->second.page;
    }
  }

  if (!nodelete)
    m_page_map.clear();
}

void
PageManager::purge_cache()
{
  /* in-memory-db: don't remove the pages or they would be lost */
  if (m_env->get_flags() & HAM_IN_MEMORY)
    return;

  if (!cache_is_full())
    return;

  /* calculate a limit of pages that we will flush */
  unsigned max_pages = m_page_map.size();

  if (max_pages == 0)
    max_pages = 1;
  /* but still we set an upper limit to avoid IO spikes */
  else if (max_pages > kPurgeLimit)
    max_pages = kPurgeLimit;

  unsigned i = 0;
  
  /* start with the oldest page */
  Page *page = m_totallist_tail;
  while (page && i < max_pages) {
    /* pick the page if it's unused, (not in a changeset), NOT mapped and old
     * enough */
    if (page->get_flags() & Page::kNpersMalloc
            && !m_env->get_changeset().contains(page)
            && page->get_address() > 0) {
      flush_page(page);
      BtreeCursor::uncouple_all_cursors(page);

      remove_from_totallist(page);

      PageMap::iterator it = m_page_map.find(page->get_address());
      ham_assert(it != m_page_map.end());

      Page *prev = page->get_previous(Page::kListCache);
      delete page;
      m_page_map.erase(it);
      page = prev;
    }
    else
      page = page->get_previous(Page::kListCache);
  }
}

void
PageManager::close_database(Database *db)
{
  PageMap::iterator it = m_page_map.begin();
  while (it != m_page_map.end()) {
    Page *page = it->second.page;
    if (page && page->get_address() > 0 && page->get_db() == db) {
      flush_page(page);
      BtreeCursor::uncouple_all_cursors(page);
      delete page;
      m_page_map.erase(it++);
    }
    else {
      ++it;
    }
  }

  m_totallist = 0;
  m_totallist_tail = 0;
}

void
PageManager::reclaim_space()
{
  if (!m_freelist)
    return;

  ham_assert(!(m_env->get_flags() & HAM_DISABLE_RECLAIM_INTERNAL));

  ham_u32_t page_size = m_env->get_page_size();
  ham_u64_t filesize = m_env->get_device()->get_filesize();

  // ignore subsequent errors - we're closing the database, and if
  // reclaiming fails then this is not a tragedy
  try {
    ham_u64_t new_size = filesize;
    while (true) {
      if (!m_freelist->is_page_free(new_size - page_size))
        break;
      new_size -= page_size;
      m_freelist->truncate_page(new_size);
    }
    if (new_size == filesize)
      return;
    m_env->get_device()->truncate(new_size);
  }
  catch (Exception &) {
  }
}

void
PageManager::check_integrity()
{
  // TODO
}

void
PageManager::add_to_freelist(Page *page)
{
  PageMap::iterator it = m_page_map.find(page->get_address());
  ham_assert(it != m_page_map.end());
  ham_assert(it->second.is_free == false);
  it->second.is_free = true;

  m_needs_flush = true;
  maybe_store_state();

  Freelist *f = get_freelist();

  if (page->get_node_proxy()) {
    delete page->get_node_proxy();
    page->set_node_proxy(0);
  }

  if (f)
    f->free_page(page);
}

void
PageManager::close()
{
  // flush all dirty pages to disk
  flush_all_pages();

  // reclaim unused disk space
  // if logging is enabled: also flush the changeset to write back the
  // modified freelist pages
  //bool try_reclaim = m_env->get_flags() & HAM_DISABLE_RECLAIM_INTERNAL
                //? false
                //: true;
#ifdef WIN32
  // Win32: it's not possible to truncate the file while there's an active
  // mapping, therefore only reclaim if memory mapped I/O is disabled
  if (!(m_env->get_flags() & HAM_DISABLE_MMAP))
    try_reclaim = false;
#endif

  /* TODO implement me 
  if (try_reclaim) {
    reclaim_space();

    if (m_env->get_flags() & HAM_ENABLE_RECOVERY)
      m_env->get_changeset().flush(m_env->get_incremented_lsn());
  }
  */

  // flush again; there were pages fetched during reclaim, and they have
  // to be released now
  flush_all_pages();
}

} // namespace hamsterdb

