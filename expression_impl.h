/***************************************************************************
* ALPS library
*
* alps/expression_impl.h   A Class to evaluate expressions
*
* $Id$
*
* Copyright (C) 2001-2003 by Matthias Troyer <troyer@comp-phys.org>,
*                            Synge Todo <wistaria@comp-phys.org>,
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
**************************************************************************/

#ifndef ALPS_EXPRESSION_IMPL_H
#define ALPS_EXPRESSION_IMPL_H

#include <alps/config.h>
#include <alps/expression.h>

#include <iostream>

namespace alps {
namespace detail {

class Block : public Expression
{
public:
  Block(std::istream&);
  Block(const Expression& e) : Expression(e) {}
  void output(std::ostream&) const;
  Evaluatable* clone() const;
  void flatten();
  boost::shared_ptr<Evaluatable> flatten_one();
  Evaluatable* partial_evaluate_replace(const Evaluator& p);
};

class Symbol : public Evaluatable {
public:
  Symbol(const std::string& n) : name_(n) {}
  double value(const Evaluator& p) const;
  bool can_evaluate(const Evaluator& p) const;
  void output(std::ostream&) const;
  Evaluatable* clone() const;
  Evaluatable* partial_evaluate_replace(const Evaluator& p);
private:
  std::string name_;
};

class Function : public Evaluatable {
public:
  Function(std::istream&, const std::string&);
  Function(const std::string& n, const Expression& e) : name_(n), arg_(e) {}
  double value(const Evaluator& p) const;
  bool can_evaluate(const Evaluator& p) const;
  void output(std::ostream&) const;
  Evaluatable* clone() const;
  boost::shared_ptr<Evaluatable> flatten_one();
  Evaluatable* partial_evaluate_replace(const Evaluator& p);
private:
 std::string name_;
 Expression arg_;
};

class Number : public Evaluatable {
public:
  Number(double x) : val_(x) {}
  double value(const Evaluator& p) const;
  bool can_evaluate(const Evaluator& p) const;
  void output(std::ostream&) const;
  Evaluatable* clone() const;
private:
 double val_;
};

} // end namespace detail
} // end namespace alps

#endif // ALPS_EXPRESSION_IMPL_H
