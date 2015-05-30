/* -*- C++ -*-
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hypertable. If not, see <http://www.gnu.org/licenses/>
 */

#include "Common/Compat.h"
#include "Common/Init.h"

#include <unistd.h>

#include "Hypertable/Lib/Config.h"
#include "Hypertable/Lib/Client.h"
#include "Hypertable/Lib/HqlInterpreter.h"

using namespace Hypertable;
using namespace Config;
using namespace std;

namespace {

void check_results(Table *table) {
  ScanSpec ss;
  TableScannerPtr scanner = table->create_scanner(ss);
  CellsBuilder cb;

  copy(scanner, cb);

  HT_ASSERT(cb.get().size() == 1);
  HT_ASSERT(cb.get().front().value_len == 5);
  HT_ASSERT(memcmp(cb.get().front().value, "value", 5) == 0);
}

void default_test(Table *table)  {
  TableMutatorPtr mutator = table->create_mutator(0, 0, 500);
  mutator->set(KeySpec("rowkey", "col", "cq"), "value");
  sleep(2);
  check_results(table);
}

void no_log_sync_test(Table *table) {
  TableMutatorPtr mutator = table->create_mutator(0, TableMutator::FLAG_NO_LOG_SYNC, 500);
  mutator->set_delete(KeySpec("rowkey", "col", AUTO_ASSIGN, FLAG_DELETE_COLUMN_FAMILY));
  mutator->set(KeySpec("rowkey", "col", "cq"), "value");
  sleep(2);
  check_results(table);
}

void cells_builder_test(Table *table)  {
  TableMutatorPtr mutator = table->create_mutator();
  mutator->set(KeySpec("1", "col", "cq"), "value1");
  mutator->set(KeySpec("2", "col", ""), "value2");
  mutator->set(KeySpec("3", "col2", "tag"), "value2");
  mutator->set(KeySpec("4", "col2", "cq"), "value3");
  mutator->flush();

  ScanSpec ss;
  TableScannerPtr scanner = table->create_scanner(ss);
  CellsBuilder cb;
  copy(scanner, cb);

  // check
  {
    HT_ASSERT(cb.get().size() == 4);
    const Cell& cell = cb.get().front();
    HT_ASSERT(strcmp(cell.column_family, "col") == 0);
    HT_ASSERT(cell.value_len == 6);
    HT_ASSERT(memcmp(cell.value, "value1", 6) == 0);
  }

  // clear, scan and check again
  cb.clear();
  scanner = table->create_scanner(ss);
  copy(scanner, cb);

  {
    HT_ASSERT(cb.get().size() == 4);
    const Cell& cell = cb.get().front();
    HT_ASSERT(strcmp(cell.column_family, "col") == 0);
    HT_ASSERT(cell.value_len == 6);
    HT_ASSERT(memcmp(cell.value, "value1", 6) == 0);
  }

  for (Cells::iterator it = cb.get().begin(); it != cb.get().end(); ++it)
    mutator->set_delete(KeySpec(
      it->row_key,
      it->column_family,
      it->column_qualifier,
      AUTO_ASSIGN,
      FLAG_DELETE_CELL));
  mutator->flush();

  // clear, scan and check again
  cb.clear();
  scanner = table->create_scanner(ss);
  copy(scanner, cb);

 HT_ASSERT(cb.get().size() == 0);
}

} // local namesapce


int main(int argc, char *argv[]) {
  try {
    init_with_policy<DefaultClientPolicy>(argc, argv);

    ClientPtr client = new Hypertable::Client();
    NamespacePtr ns = client->open_namespace("/");
    HqlInterpreterPtr hql = client->create_hql_interpreter();

    hql->execute("use '/'");
    hql->execute("drop table if exists periodic_flush_test");
    hql->execute("create table periodic_flush_test(col, col2)");

    TablePtr table = ns->open_table("periodic_flush_test");

    cells_builder_test(table.get());
    default_test(table.get());
    no_log_sync_test(table.get());
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    _exit(1);
  }
  _exit(0);
}
