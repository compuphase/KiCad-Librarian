/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  The RPN expression parser.
 *
 *  Copyright (C) 2013-2018 CompuPhase
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License. You may obtain a copy
 *  of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 *
 *  $Id: rpn.h 5907 2018-12-14 22:05:40Z thiadmer $
 */
#ifndef __rpn_h
#define __rpn_h

#include <cassert>
#include <cstring>
#include <vector>

#define MAX_STACK 16  /* max. nesting level of the stack */
#define MAX_WORD  16  /* max. length of a number, variable or function */

class RPNvalue {
public:
  RPNvalue();
  RPNvalue(double value);
  RPNvalue(const char* text);
  RPNvalue(const RPNvalue& value);
  ~RPNvalue();

  RPNvalue& operator=(const RPNvalue& value);

  bool Alpha() const { return m_text != 0; }
  double Double() const { return m_value; }   /* returns 0.0 for alpha type */
  const char *Text() const { return m_text ? m_text : "#ERROR"; }

  operator double() { return m_value; }       /* same as Double() */
  operator const char *() const { return m_text ? m_text : "#ERROR"; }

  void Set(double value);
  void Set(const char* text);
  void Set(const RPNvalue& value);

private:
  double m_value;
  char *m_text;
};

class RPNvariable {
public:
  RPNvariable(const char *name, double value) : m_value(value) {
    assert(strlen(name) <= MAX_WORD);
    strcpy(m_name, name);
  }
  RPNvariable(const char *name, const char *text) : m_value(text) {
    assert(strlen(name) <= MAX_WORD);
    strcpy(m_name, name);
  }
  RPNvariable(const char *name, const RPNvalue &value) : m_value(value) {
    assert(strlen(name) <= MAX_WORD);
    strcpy(m_name, name);
  }

  const char *Name() const { return m_name; }
  RPNvalue Value() const { return m_value; }

  void Set(double value) { m_value.Set(value); }
  void Set(const char *text) { m_value.Set(text); }
  void Set(const RPNvalue &value) { m_value.Set(value); }

private:
  char m_name[MAX_WORD + 1];
  RPNvalue m_value;
};

enum RPN_ERROR {
  RPN_OK = 0,   /* no expression error detected, single result left on the stack */
  RPN_EMPTY,    /* empty stack, not necessarily an error (when the result of the
                   expression is stored in a variable, the stack is left empty) */
  RPN_NOTSINGLE,/* more than a single item left on the stack, must be because
                   of an expression error */
  RPN_UNDERFLOW,/* stack underflow, must be because of an expression error */
  RPN_OVERFLOW, /* stack overflow (expression too complex) */
  RPN_INV_VAR,  /* reference to an unknown variable */
  RPN_INV_FUNC, /* calling an unknown function */
  RPN_INV_OPER, /* calling an unknown operator */
  RPN_TEXT_OPER,/* attempt to perform an arithmetic operation on a text string */
  RPN_INV_STRING,/* run-away string */
  RPN_DIV_ZERO, /* division by zero */
  RPN_SYNTAX,   /* syntax error */
};

/** Usage:
 *  1) create an instance of RPNexpression, passing in the expression itself
 *     (or call Set() later)
 *  2) add/set all variables
 *  3) call Parse(), returns a success/failure flag
 *  4) if Parse() returned RPN_OK, call Value() to get it
 */
class RPNexpression {
public:
  explicit RPNexpression(const char *expression = 0);
  ~RPNexpression();

  void Set(const char *expression);
  RPN_ERROR Parse();
  const RPNvalue& Value() const { return stack[0]; }

  bool ExistVariable(const char *name) { return varindex(name) >= 0; }
  void SetVariable(const RPNvariable &v);

private:
  void push(const RPNvalue &v) {
    if (m_top < MAX_STACK) stack[m_top++] = v; else m_error = RPN_OVERFLOW;
   }
  const RPNvalue &pop() {
    if (m_top > 0) return stack[--m_top];
    if (m_error == RPN_OK) m_error = RPN_UNDERFLOW;
    stack[0] = RPNvalue(0.0);
    return stack[0];
  }
  int varindex(const char *name);

  char *m_expr;
  RPNvalue stack[MAX_STACK];
  int m_top;          /* stack top */
  RPN_ERROR m_error;  /* returns parsing error, if any */

  std::vector<RPNvariable> variables;
};

#endif /* __rpn_h */

