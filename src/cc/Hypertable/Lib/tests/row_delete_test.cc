/*
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
 * License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <Common/Compat.h>

#include <Common/md5.h>
#include <Common/Usage.h>

#include <Hypertable/Lib/Client.h>

#include <cstdlib>
#include <iostream>

using namespace std;
using namespace Hypertable;

namespace {

  const char *schema =
  "<Schema>"
  "  <AccessGroup name=\"default\">"
  "    <ColumnFamily>"
  "      <Name>data</Name>"
  "    </ColumnFamily>"
  "  </AccessGroup>"
  "</Schema>";

  const char *usage[] = {
    "usage: row_delete_test [<seed>]",
    "",
    "Validates the insertion and retrieval of large items.",
    0
  };


}


int main(int argc, char **argv) {
  unsigned long seed = 1234;

  if (argc > 2 ||
      (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-?"))))
    Usage::dump_and_exit(usage);

  if (argc == 2)
    seed = atoi(argv[1]);

  srandom(seed);

  try {
    Client *hypertable = new Client(argv[0], "./hypertable.cfg");
    NamespacePtr ns = hypertable->open_namespace("/");
    TablePtr table_ptr;
    KeySpec key;
    Cell cell;
    const char *value1 = "Hello, World! (1)";
    const char *value2 = "Hello, World! (2)";
    const char *expected[] = { "foo : DELETE ROW", "foo data: Hello, World! (2)" };

    /**
     * Validate large object returned by CREATE_SCANNER
     */
    ns->drop_table("RowDeleteTest", true);
    ns->create_table("RowDeleteTest", schema);

    table_ptr = ns->open_table("RowDeleteTest");

    TableMutatorPtr mutator_ptr(table_ptr->create_mutator());

    key.row = "foo";
    key.row_len = strlen("foo");
    key.column_family = "data";
    key.column_qualifier = 0;
    key.column_qualifier_len = 0;
    mutator_ptr->set(key, value1, strlen(value1));
    mutator_ptr->flush();
    key.clear();

    key.row = "foo";
    key.row_len = strlen("foo");
    key.column_family = 0;
    key.column_qualifier = 0;
    key.column_qualifier_len = 0;
    key.flag = FLAG_DELETE_ROW;
    mutator_ptr->set_delete(key);
    mutator_ptr->flush();
    key.clear();

    key.row = "foo";
    key.row_len = strlen("foo");
    key.column_family = "data";
    key.column_qualifier = 0;
    key.column_qualifier_len = 0;
    mutator_ptr->set(key, value2, strlen(value2));
    mutator_ptr->flush();
    key.clear();

    mutator_ptr.reset();

    ScanSpec scan_spec;

    scan_spec.return_deletes = true;
    TableScannerPtr scanner_ptr(table_ptr->create_scanner(scan_spec));

    std::vector<String>  values;
    String result;
    while (scanner_ptr->next(cell)) {
      result = String(cell.row_key);
      if (cell.column_family) {
	result += String(" ") + cell.column_family;
	if (cell.column_qualifier)
	  result += String(":") + cell.column_qualifier;
      }
      if (cell.flag == FLAG_DELETE_ROW)
	result += String(" DELETE ROW");
      else if (cell.flag == FLAG_DELETE_COLUMN_FAMILY)
	result += String(" DELETE COLUMN FAMILY");
      else if (cell.flag == FLAG_DELETE_CELL)
	result += String(" DELETE CELL");
      else if (cell.flag == FLAG_DELETE_CELL_VERSION)
	result += String(" DELETE CELL VERSION");
      else
	result += String(" ") + String((const char *)cell.value, cell.value_len);
      values.push_back(result);
    }

    for (size_t i=0; i<values.size(); i++) {
      if (values[i] != String(expected[i])) {
	std::cout << "Expected:\n";
	std::cout << expected[0] << "\n" << expected[1] << "\n\n";
	std::cout << "Got:\n";
	for (size_t j=0; j<values.size(); j++)
	  std::cout << values[i] << "\n";
	std::cout << endl;
	quick_exit(EXIT_FAILURE);
      }
    }

    scanner_ptr.reset();
    table_ptr.reset();
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    quick_exit(EXIT_FAILURE);
  }

  quick_exit(EXIT_SUCCESS);
}
