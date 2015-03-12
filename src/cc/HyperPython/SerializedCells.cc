/**
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

#include "../ThriftBroker/SerializedCellsReader.h"
#include "../ThriftBroker/SerializedCellsWriter.h"

#include <boost/python.hpp>
#include <string>

using namespace Hypertable;
using namespace boost::python;

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 3
static PyObject *PyBuffer_FromMemory(void *ptr, Py_ssize_t size)
{
  // New style buffer interface - PyMemoryView_FromMemory available since 3.3
  return PyMemoryView_FromMemory((char *)ptr, size, PyBUF_READ);
}
#endif // Python version test

typedef bool (SerializedCellsWriter::*addfn)(const char *row,
                const char *column_family, const char *column_qualifier,
                int64_t timestamp, const char *value, int32_t value_length,
                int cell_flag);
typedef const char *(SerializedCellsWriter::*getfn)();
typedef int32_t (SerializedCellsWriter::*getlenfn)();

static addfn afn = &Hypertable::SerializedCellsWriter::add;
static getlenfn lenfn = &Hypertable::SerializedCellsWriter::get_buffer_length;

static PyObject *convert(const SerializedCellsWriter &scw) {
  boost::python::object obj(handle<>(PyBuffer_FromMemory(
                      (void *)scw.get_buffer(), scw.get_buffer_length())));
  return boost::python::incref(obj.ptr());
}

// static SerializedCellsReader *initbuf(PyObject *buf, uint32_t len) {
//     PyObject *mview = PyMemoryView_FromObject(buf);
//     Py_buffer *pybuf = PyMemoryView_GET_BUFFER(mview);
//     return new SerializedCellsReader(pybuf->buf, len);
// }

class SerializedCellsReaderPy : public SerializedCellsReader
{
public:
  SerializedCellsReaderPy(PyObject *buf, uint32_t len) 
    : SerializedCellsReader((char *)PyMemoryView_GET_BUFFER(
                              PyMemoryView_FromObject(buf))->buf, len)

  {}

  const std::string value() {
    return std::string(value_str(), value_len());
  }
};
    
BOOST_PYTHON_MODULE(libHyperPython)
{

  class_<Cell>("Cell")
    .def("sanity_check", &Cell::sanity_check)
    .def_readwrite("row", &Cell::row_key)
    .def_readwrite("column_family", &Cell::column_family)
    .def_readwrite("column_qualifier", &Cell::column_qualifier)
    .def_readwrite("timestamp", &Cell::timestamp)
    .def_readwrite("revision", &Cell::revision)
    .def_readwrite("value", &Cell::value)
    .def_readwrite("flag", &Cell::flag)
    .def(self_ns::str(self_ns::self))
    ;

  class_<SerializedCellsReaderPy>("SerializedCellsReader",
          init<PyObject *, uint32_t>())
    .def("has_next", &SerializedCellsReaderPy::next)
    .def("get_cell", &SerializedCellsReaderPy::get_cell,
          return_value_policy<return_by_value>())
    .def("row", &SerializedCellsReaderPy::row,
          return_value_policy<return_by_value>())
    .def("column_family", &SerializedCellsReaderPy::column_family,
          return_value_policy<return_by_value>())
    .def("column_qualifier", &SerializedCellsReaderPy::column_qualifier,
          return_value_policy<return_by_value>())
    .def("value", &SerializedCellsReaderPy::value,
          return_value_policy<return_by_value>())
    .def("value_len", &SerializedCellsReaderPy::value_len)
    .def("value_str", &SerializedCellsReaderPy::value_str,
          return_value_policy<return_by_value>())
    .def("timestamp", &SerializedCellsReaderPy::timestamp)
    .def("cell_flag", &SerializedCellsReaderPy::cell_flag)
    .def("flush", &SerializedCellsReaderPy::flush)
    .def("eos", &SerializedCellsReaderPy::eos)
  ;

  class_<SerializedCellsWriter, boost::noncopyable>("SerializedCellsWriter",
          init<int32_t, bool>())
    .def("add", afn)
    .def("finalize", &SerializedCellsWriter::finalize)
    .def("empty", &SerializedCellsWriter::empty)
    .def("clear", &SerializedCellsWriter::clear)
    .def("__len__", lenfn)
    .def("get", &convert)
  ;
}
