/*****************************************************************************
*
* ALPS Project: Algorithms and Libraries for Physics Simulations
*
* ALPS Libraries
*
* Copyright (C) 1994-2003 by Matthias Troyer <troyer@itp.phys.ethz.ch>,
*                            Synge Todo <wistaria@comp-phys.org>
*
* This software is part of the ALPS libraries, published under the ALPS
* Library License; you can use, redistribute it and/or modify it under
* the terms of the license, either version 1 or (at your option) any later
* version.
* 
* You should have received a copy of the ALPS Library License along with
* the ALPS Libraries; see the file LICENSE.txt. If not, the license is also
* available from http://alps.comp-phys.org/.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
* SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
* FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
* DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

/* $Id$ */

#ifndef ALPS_OSIRIS_DUMP_H
#define ALPS_OSIRIS_DUMP_H

#include <alps/config.h>
#ifdef ALPS_HAVE_STDARG_H
# include <stdarg.h>
#endif
#include <boost/smart_ptr.hpp>
#include <complex>
#include <iostream>
#include <map>
//#include <rpc/types.h>
#include <typeinfo>
#include <vector>
#include <string>

namespace alps {

class ODump
{
public:
  ODump(uint32_t v = 0);
  virtual ~ODump() {}

  uint32_t version() const { return version_; } 
  
# define ALPS_DUMP_DO_TYPE(T) \
  virtual void write_simple(T x);
  ALPS_DUMP_DO_TYPE(bool)
  ALPS_DUMP_DO_TYPE(char)
  ALPS_DUMP_DO_TYPE(signed char)
  ALPS_DUMP_DO_TYPE(unsigned char)
  ALPS_DUMP_DO_TYPE(short)
  ALPS_DUMP_DO_TYPE(unsigned short)
  virtual void write_simple(int x) = 0;
  ALPS_DUMP_DO_TYPE(unsigned int)
  ALPS_DUMP_DO_TYPE(long)
  ALPS_DUMP_DO_TYPE(unsigned long)
# ifdef BOOST_HAS_LONG_LONG
  ALPS_DUMP_DO_TYPE(long long)
  ALPS_DUMP_DO_TYPE(unsigned long long)
# endif
  ALPS_DUMP_DO_TYPE(float)
  virtual void write_simple(double x) = 0;
  ALPS_DUMP_DO_TYPE(long double)
# undef ALPS_DUMP_DO_TYPE

  template <class T>
  void write_complex(const std::complex<T>& x)
  {
    write_simple(std::real(x));
    write_simple(std::imag(x));
  }

  template<class T>
  ODump& operator<<(const std::complex<T>& x) { write_complex(x); return *this; }


# define ALPS_DUMP_DO_TYPE(T) \
  virtual void write_array(std::size_t n, const T * p); \
  ODump& operator<<( T x) { write_simple(x); return *this; }
  ALPS_DUMP_DO_TYPE(bool)
  ALPS_DUMP_DO_TYPE(char)
  ALPS_DUMP_DO_TYPE(signed char)
  ALPS_DUMP_DO_TYPE(unsigned char)
  ALPS_DUMP_DO_TYPE(short)
  ALPS_DUMP_DO_TYPE(unsigned short)
  ALPS_DUMP_DO_TYPE(int)
  ALPS_DUMP_DO_TYPE(unsigned int)
  ALPS_DUMP_DO_TYPE(long)
  ALPS_DUMP_DO_TYPE(unsigned long)
# ifdef BOOST_HAS_LONG_LONG
  ALPS_DUMP_DO_TYPE(long long)
  ALPS_DUMP_DO_TYPE(unsigned long long)
# endif
  ALPS_DUMP_DO_TYPE(float)
  ALPS_DUMP_DO_TYPE(double)
  ALPS_DUMP_DO_TYPE(long double)
# undef ALPS_DUMP_DO_TYPE

  template <class T>
  void write_array(std::size_t n, const std::complex<T>* p)
  { write_array(2 * n, reinterpret_cast<const T*>(p)); }

  virtual void write_string(std::size_t n, const char* s);
  virtual void write_string(const std::string&);

  /** register an object to prepare serializing a pointer to it.
      after writing an object it has to be registered with the dump to
      allow writing a pointer to the object. */

  void registerObjectAddress(void* p);

  /** serialize a pointer.
      after registering an object's address a pointer to it can be serialized.
      This is done by writing an integer number associated with the object
      when its address is registered. */
      
  void writePointer(void* p);
  
private: 
  uint32_t version_;
  uint32_t highestNumber_;
  std::map<void *, uint32_t> numberOfPointer_;
};


class IDump
{
public:
  IDump(uint32_t v=0);
  virtual ~IDump() {}

  uint32_t version() const { return version_;} 
  void set_version(uint32_t v) { version_=v;} 
  
# define ALPS_DUMP_DO_TYPE(T) \
  virtual void read_simple(T& x);

  ALPS_DUMP_DO_TYPE(bool)
  ALPS_DUMP_DO_TYPE(char)
  ALPS_DUMP_DO_TYPE(signed char)
  ALPS_DUMP_DO_TYPE(unsigned char)
  ALPS_DUMP_DO_TYPE(short)
  ALPS_DUMP_DO_TYPE(unsigned short)
  virtual void read_simple(int& x) = 0;
  ALPS_DUMP_DO_TYPE(unsigned int)
  ALPS_DUMP_DO_TYPE(long)
  ALPS_DUMP_DO_TYPE(unsigned long)
# ifdef BOOST_HAS_LONG_LONG
  ALPS_DUMP_DO_TYPE(long long)
  ALPS_DUMP_DO_TYPE(unsigned long long)
# endif
  ALPS_DUMP_DO_TYPE(float)
  virtual void read_simple(double& x) = 0;
  ALPS_DUMP_DO_TYPE(long double)
# undef ALPS_DUMP_DO_TYPE

  template <class T>
  void read_complex(std::complex<T>& x) { x = std::complex<T>(get<T>(), get<T>());}
  
  template<class T>
  IDump& operator>>(std::complex<T>& x) { read_complex(x); return *this; }

# define ALPS_DUMP_DO_TYPE(T) \
  virtual void read_array(std::size_t n, T * p); \
  IDump& operator>>(T& x) { read_simple(x); return *this; } \
  operator T () { return get<T>(); }
  ALPS_DUMP_DO_TYPE(bool)
  ALPS_DUMP_DO_TYPE(char)
  ALPS_DUMP_DO_TYPE(signed char)
  ALPS_DUMP_DO_TYPE(unsigned char)
  ALPS_DUMP_DO_TYPE(short)
  ALPS_DUMP_DO_TYPE(unsigned short)
  ALPS_DUMP_DO_TYPE(int)
  ALPS_DUMP_DO_TYPE(unsigned int)
  ALPS_DUMP_DO_TYPE(long)
  ALPS_DUMP_DO_TYPE(unsigned long)
# ifdef BOOST_HAS_LONG_LONG
  ALPS_DUMP_DO_TYPE(long long)
  ALPS_DUMP_DO_TYPE(unsigned long long)
# endif
  ALPS_DUMP_DO_TYPE(float)
  ALPS_DUMP_DO_TYPE(double)
  ALPS_DUMP_DO_TYPE(long double)
# undef ALPS_DUMP_DO_TYPE

  template <class T>
  void read_array(std::size_t n, std::complex<T>* p)
  { read_array(2 * n, reinterpret_cast<float*>(p)); }

  virtual void read_string(std::size_t n, char* s);
  virtual void read_string(std::string&);

  template <class T>
  operator std::complex<T> ()
  {
    std::complex<T> x;
    read_simple(x);
    return x;
  }

  template <class T>
  inline T get()
  {
    T x; read_simple(x);
    return x;
  }

  // read the next boolean value from the dump and return its value.
  bool test() { return get<bool>(); }

  /** register an object to prepare deserializing a pointer to it.
      after reading an object it has to be registered with the dump to
      allow reading a pointer to the object. */
  void registerObjectAddress(void* p);

  /** deserialize a pointer.
      After registering an object's address a pointer to it can be deserialized.
      This is done by reading an integer number associated with the object
      when its address is registered. */
  void* readPointer();
  
  /** deserialize a pointer to T.
      After registering an object's address a pointer to it can be deserialized.
      This is done by reading an integer number associated with the object
      when its address is registered. This function just calls readPointer()
      and type casts the pointer. It is a more convenient way of reading pointer.*/
  template <class T>
  T* readAPointer() {return static_cast<T*>(readPointer());}
 
private:
  uint32_t version_;
  std::vector<void*> pointerVector_;
};

} // end namespace

#include "dumparchive.h"

#endif // OSIRIS_DUMP_H
