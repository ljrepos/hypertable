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

#include <Hypertable/Lib/Client.h>
#include <Hypertable/Lib/Future.h>

#include <Common/md5.h>
#include <Common/Usage.h>

#include <cstdlib>
#include <iostream>

extern "C" {
#include <unistd.h>
}

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
    "usage: future_test",
    "",
    "Validates asynchronous (Future) API and cancellation.",
    0
  };

  void load_buffer_with_random(uint8_t *buf, size_t size) {
    uint8_t *ptr = buf;
    uint32_t uival;
    size_t n = size / 4;
    if (size % 4)
      n++;
    for (size_t i=0; i<n; i++) {
      uival = (uint32_t)random();
      memcpy(ptr, &uival, 4);
      ptr += 4;
    }
  }

}


int main(int argc, char **argv) {
  unsigned long seed = 1234;
  uint8_t *buf = new uint8_t [1048576];
  char keybuf[32];
  uint8_t sent_digest[16];
  uint8_t received_digest[16];

  if (argc > 2 ||
      (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-?"))))
    Usage::dump_and_exit(usage);

  if (argc == 2)
    seed = atoi(argv[1]);

  cout << "SEED: " << seed << endl;

  srandom(seed);

  try {
    Client *hypertable = new Client(argv[0], "./future_test.cfg");
    NamespacePtr ns = hypertable->open_namespace("/");

    TablePtr table_ptr;
    KeySpec key;
    ScanSpecBuilder ssbuilder;
    ScanSpec scan_spec;
    Cell cell;
    md5_context md5_ctx;
    ResultPtr result;
    Cells cells;

    /**
     * Load data
     */
    {
      Future ff;
      ns->drop_table("FutureTest", true);
      ns->create_table("FutureTest", schema);

      table_ptr = ns->open_table("FutureTest");

      TableMutatorAsyncPtr mutator_ptr(table_ptr->create_mutator_async(&ff, 0, Table::MUTATOR_FLAG_NO_LOG_SYNC));

      key.column_family = "data";
      key.column_qualifier = 0;
      key.column_qualifier_len = 0;

      memset(&md5_ctx, 0, sizeof(md5_ctx));
      md5_starts(&md5_ctx);

      size_t outstanding=0;
      for (size_t i=0; i<100; i++) {
        load_buffer_with_random(buf, 1000);
        sprintf(keybuf, "%05u", (unsigned)i);
        key.row = keybuf;
        key.row_len = strlen(keybuf);
        mutator_ptr->set(key, buf, 1000);
        mutator_ptr->flush();
        outstanding++;

        if (outstanding>5) {
          while (ff.get(result)) {
            if (result->is_error()) {
              int error;
              String error_msg;
              result->get_error(error, error_msg);
              Exception e(error, error_msg);
              HT_ERROR_OUT << "Encountered scan error " << e << HT_END;
              quick_exit(EXIT_FAILURE);
            }

            HT_ASSERT(result->is_update());
            HT_INFO("Flush complete");
            HT_ASSERT(outstanding > 0);
            outstanding--;
          }
          HT_ASSERT(outstanding==0);
        }

        if (i<50)
          md5_update(&md5_ctx, buf, 1000);
      }
      md5_finish(&md5_ctx, sent_digest);
    }

    {
      // Do asynchronous scan and cancel after some time
      Future ff(500);

      ssbuilder.set_row_limit(60);
      ssbuilder.add_row_interval("00000",true, "00048", false);
      ssbuilder.add_row_interval("00048",true, "00049", false);
      ssbuilder.add_row_interval("00049",true, "00050", false);
      ssbuilder.add_row_interval("00050",true, "01000", false);
      scan_spec = ssbuilder.get();

      TableScannerAsyncPtr scanner_ptr(table_ptr->create_scanner_async(&ff, scan_spec));

      memset(&md5_ctx, 0, sizeof(md5_ctx));
      md5_starts(&md5_ctx);
      int num_cells=0;
      bool finished = false;
      while (!finished) {
        if (ff.is_full())
          HT_INFO("Future queue is full");
        ff.get(result);
        if (result->is_error()) {
          int error;
          String error_msg;
          result->get_error(error, error_msg);
          Exception e(error, error_msg);
          HT_ERROR_OUT << "Encountered scan error " << e << HT_END;
          quick_exit(EXIT_FAILURE);
        }

        result->get_cells(cells);
        for (size_t ii=0; ii< cells.size(); ++ii) {
          md5_update(&md5_ctx, (unsigned char *)cells[ii].value, cells[ii].value_len);
          HT_INFOF("Got cell with key=%s", cells[ii].row_key);
          num_cells++;
          if (num_cells>=50) {
            ff.cancel();
            finished=true;
            break;
          }
        }
      }
      md5_finish(&md5_ctx, received_digest);

      if (memcmp(sent_digest, received_digest, 16)) {
        HT_ERROR("MD5 digest mismatch between sent and received");
        quick_exit(EXIT_FAILURE);
      }
      HT_INFO("cancel test finished");

      // this time let the scan run through till completion
      memset(&md5_ctx, 0, sizeof(md5_ctx));
      md5_starts(&md5_ctx);
      Future ff2;
      ssbuilder.clear();
      ssbuilder.add_row_interval("00000",true, "00010", false);
      ssbuilder.add_row_interval("00010",true, "00027", false);
      ssbuilder.add_row_interval("00027",true, "00050", false);
      scan_spec = ssbuilder.get();
      scanner_ptr.reset(table_ptr->create_scanner_async(&ff2, scan_spec));

      while (ff2.get(result)) {
        if (result->is_error()) {
          int error;
          String error_msg;
          result->get_error(error, error_msg);
          Exception e(error, error_msg);
          HT_ERROR_OUT << "Encountered scan error " << e << HT_END;
          quick_exit(EXIT_FAILURE);
        }

        result->get_cells(cells);
        for (size_t ii=0; ii< cells.size(); ++ii) {
          md5_update(&md5_ctx, (unsigned char *)cells[ii].value, cells[ii].value_len);
          HT_INFOF("Got cell with key=%s", cells[ii].row_key);
        }
      }

      md5_finish(&md5_ctx, received_digest);

      if (memcmp(sent_digest, received_digest, 16)) {
        HT_ERROR("MD5 digest mismatch between sent and received");
        quick_exit(EXIT_FAILURE);
      }
      HT_INFO("full scan test finished");

      // this time cancel individual scanners
      memset(&md5_ctx, 0, sizeof(md5_ctx));
      md5_starts(&md5_ctx);
      Future ff3(50000);
      ssbuilder.clear();
      ssbuilder.add_row_interval("00000",true, "00006", false);
      ssbuilder.add_row_interval("00006",true, "00041", false);
      ssbuilder.add_row_interval("00041",true, "00050", false);
      ScanSpec scan_spec_a = ssbuilder.get();

      ssbuilder.clear();
      ssbuilder.add_row_interval("00000",true, "00016", false);
      ssbuilder.add_row_interval("00016",true, "00031", false);
      ssbuilder.add_row_interval("00031",true, "00050", false);
      scan_spec = ssbuilder.get();

      ssbuilder.clear();
      ssbuilder.add_row_interval("00009",true, "00099", false);
      ScanSpec scan_spec_c = ssbuilder.get();

      TableScannerAsyncPtr scanner_a_ptr(table_ptr->create_scanner_async(&ff3, scan_spec_a));
      TableScannerAsyncPtr scanner_b_ptr(table_ptr->create_scanner_async(&ff3, scan_spec));
      TableScannerAsyncPtr scanner_c_ptr(table_ptr->create_scanner_async(&ff3, scan_spec_c));

      // wait until the queue is full
      while (!ff3.is_full())
        sleep(1);

      // cancel the scanners
      scanner_a_ptr->cancel();
      scanner_c_ptr->cancel();

      // get the remaining results
      while (ff3.get(result)) {
        if (result->is_error()) {
          int error;
          String error_msg;
          result->get_error(error, error_msg);
          Exception e(error, error_msg);
          HT_ERROR_OUT << "Encountered scan error " << e << HT_END;
          quick_exit(EXIT_FAILURE);
        }

        if (scanner_b_ptr.get() == result->get_scanner()) {
          result->get_cells(cells);
          for (size_t ii=0; ii< cells.size(); ++ii) {
            md5_update(&md5_ctx, (unsigned char *)cells[ii].value, cells[ii].value_len);
            HT_INFOF("Got cell with key=%s", cells[ii].row_key);
          }
        }
        else {
          HT_ERROR("Result from a cancelled scanner received");
          quick_exit(EXIT_FAILURE);
        }
      }

      md5_finish(&md5_ctx, received_digest);

      if (memcmp(sent_digest, received_digest, 16)) {
        HT_ERROR("MD5 digest mismatch between sent and received");
        quick_exit(EXIT_FAILURE);
      }
      HT_INFO("cancel scanner test finished");

      scanner_a_ptr = scanner_b_ptr = scanner_c_ptr = 0;
    }
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    quick_exit(EXIT_FAILURE);
  }

  quick_exit(EXIT_SUCCESS);
}
