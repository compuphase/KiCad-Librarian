/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  The RPN expression parser.
 *
 *  Copyright (C) 2013-2015 CompuPhase
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
 *  $Id: rpn.cpp 5387 2015-10-22 19:31:30Z thiadmer $
 */
#include "rpn.h"
#include <cctype>
#include <cmath>
#include <cstdlib>

#if !defined EPSILON
  #define EPSILON       0.000001
#endif
#if !defined M_PI
  #define M_PI          3.14159265358979323846
#endif
#if !defined sizearray
  #define sizearray(a)  (sizeof(a) / sizeof((a)[0]))
#endif


RPNvalue::RPNvalue()
{
  m_value = 0;
  m_text = 0;
}

RPNvalue::RPNvalue(double value)
{
  m_value = value;
  m_text = 0;
}

RPNvalue::RPNvalue(const char* text)
{
  assert(text != 0);
  m_value = 0;
  m_text = new char[strlen(text) + 1];
  if (m_text)
    strcpy(m_text, text);
}

RPNvalue::RPNvalue(const RPNvalue& value)
{
  if (value.Alpha()) {
    m_value = 0;
    m_text = new char[strlen(value.Text()) + 1];
    if (m_text)
      strcpy(m_text, value.Text());
  } else {
    m_value = value.Double();
    m_text = 0;
  }
}

RPNvalue::~RPNvalue()
{
  if (m_text)
    delete[] m_text;
}

RPNvalue& RPNvalue::operator=(const RPNvalue& value)
{
  if (&value == this)
    return *this; /* check self-assignment */
  if (m_text && m_text == value.Text())
    return *this; /* check assignment with same contents */

  if (m_text)
    delete[] m_text;
  if (value.Alpha()) {
    m_value = 0;
    m_text = new char[strlen(value.Text()) + 1];
    if (m_text)
      strcpy(m_text, value.Text());
  } else {
    m_value = value.Double();
    m_text = 0;
  }
  return *this;
}

void RPNvalue::Set(double value)
{
  if (m_text) {
    delete[] m_text;
    m_text = 0;
  }
  m_value = value;
}

void RPNvalue::Set(const char* text)
{
  if (m_text)
    delete[] m_text;
  m_text = new char[strlen(text) + 1];
  if (m_text)
    strcpy(m_text, text);
  m_value = 0;
}

void RPNvalue::Set(const RPNvalue& value)
{
  if (m_text)
    delete[] m_text;
  if (value.Alpha()) {
    m_value = 0;
    m_text = new char[strlen(value.Text()) + 1];
    if (m_text)
      strcpy(m_text, value.Text());
  } else {
    m_value = value.Double();
    m_text = 0;
  }
}



RPNexpression::RPNexpression(const char* expression)
{
  if (expression) {
    m_expr = new char[strlen(expression) + 1];
    assert(m_expr);
    strcpy(m_expr, expression);
  } else {
    m_expr = 0;
  }
  m_top = 0;
  variables.clear();
  m_error = RPN_OK;
}

RPNexpression::~RPNexpression()
{
  if (m_expr)
    delete[] m_expr;
  variables.clear();
}

void RPNexpression::Set(const char *expression)
{
  if (m_expr)
    delete[] m_expr;
  assert(expression);
  m_expr = new char[strlen(expression) + 1];
  assert(m_expr);
  strcpy(m_expr, expression);
}

void RPNexpression::SetVariable(const RPNvariable &v)
{
  int idx = varindex(v.Name());
  if (idx >= 0)
    variables[idx].Set(v.Value()); /* update existing */
  else
    variables.push_back(v);
}

int RPNexpression::varindex(const char *name)
{
  for (unsigned i = 0; i < variables.size(); i++)
    if (strcmp(variables[i].Name(), name) == 0)
      return (int)i;
  return -1;
}

RPN_ERROR RPNexpression::Parse()
{
  char *start = m_expr;
  assert(start);
  m_error = RPN_OK;
  m_top = 0;
  int stackmark = -1;
  while (*start != '\0') {
    /* collect a word */
    while (*start != '\0' && *start <= ' ')
      start++;  /* skip white space */
    if (*start=='\0')
      break;
    char word[MAX_WORD + 1];
    char *tail = start;
    int idx = 0;
    while (*tail != '\0' && *tail > ' ') {
      if (idx <= MAX_WORD)
        word[idx++] = *tail;
      tail++;
    }
    word[idx] = '\0';
    /* check what it is */
    if (isdigit(word[0]) || (word[0] == '-' && isdigit(word[1]))) {
      /* number (must start with a digit: .5 is incorrect, use 0.5 instead */
      push(RPNvalue(strtod(word, 0)));
    } else if (word[0] == '"') {
      /* string, collect from the source string */
      assert(*start == '"');
      start++;
      for (tail = start; *tail != '\0' && *tail != '"'; tail++)
        /* nothing */;
      if (*tail != '"') {
        m_error = RPN_INV_STRING;
      } else {
        *tail = '\0';   /* temporary terminate the string here */
        push(RPNvalue(start));
        *tail++ = '"';  /* restore and skip */
      }
    } else if (word[0] == '@') {
      /* variable assignment */
      RPNvalue p1 = pop();
      if (word[1] == '?') {
        /* optional assignment; only assign if it does not yet exist (simply
           drop the value if the variable already exists */
        int i = varindex(word + 2);
        if (i < 0) {
          RPNvariable v(word + 2, p1);
          SetVariable(v);
        }
      } else {
        RPNvariable v(word + 1, p1);
        SetVariable(v);
      }
    } else if (isalpha(word[0])) {
      if (isupper(word[0])) {
        /* variable reference */
        int i = varindex(word);
        if (i >= 0) {
          assert(i < (int)variables.size());
          push(variables[i].Value());
        } else if (m_error == RPN_OK) {
          m_error = RPN_INV_VAR;
        }
      } else {
        /* function */
        if (strcmp(word, "abs") == 0) {
          RPNvalue p1 = pop();
          push(fabs(p1));
        } else if (strcmp(word, "atan") == 0) {
          RPNvalue p2 = pop();  /* dx */
          RPNvalue p1 = pop();  /* dy */
          push(atan2(p1,p2) * 180.0 / M_PI);  /* returns angles in the range -180 .. +180 */
        } else if (strcmp(word, "ceil") == 0) {
          RPNvalue p1 = pop();
          push(ceil(p1 - EPSILON));
        } else if (strcmp(word, "chr") == 0) {
          RPNvalue p1 = pop();
		  char str[2] = "?";
		  str[0] = (char)(int)p1.Double();
		  push(RPNvalue(str));
        } else if (strcmp(word, "cos") == 0) {
          RPNvalue p1 = pop();
          push(cos(p1 * M_PI / 180.0));
        } else if (strcmp(word, "even") == 0) {
          RPNvalue p1 = pop();
          long v = (long)floor(p1 + 0.5);
          push((v & 1) == 0);
        } else if (strcmp(word, "floor") == 0) {
          RPNvalue p1 = pop();
          push(floor(p1 + EPSILON));
        } else if (strcmp(word, "gacol") == 0) {
          static const char letters[] = "ABCDEFGHJKLMNPRTUVWY";	/*  no I, O, Q, S, X, Z */
          RPNvalue p1 = pop();
		  char str[2] = "?";
		  int idx = (int)p1.Double();
		  if (idx >= 0 && idx < sizearray(letters))
			str[0] = letters[idx];
		  push(RPNvalue(str));
        } else if (strcmp(word, "max") == 0) {
          RPNvalue p2 = pop();
          RPNvalue p1 = pop();
          push((p1.Double() > p2.Double()) ? p1 : p2);
        } else if (strcmp(word, "mil") == 0) {
          RPNvalue p1 = pop();
          push(floor(p1 / 0.0254 + 0.5));
        } else if (strcmp(word, "min") == 0) {
          RPNvalue p2 = pop();
          RPNvalue p1 = pop();
          push((p1.Double() < p2.Double()) ? p1 : p2);
        } else if (strcmp(word, "odd") == 0) {
          RPNvalue p1 = pop();
          long v = (long)floor(p1 + 0.5);
          push((v & 1) != 0);
        } else if (strcmp(word, "round") == 0) {
          RPNvalue p1 = pop();
          push(floor(p1 + 0.5));
        } else if (strcmp(word, "sin") == 0) {
          RPNvalue p1 = pop();
          push(sin(p1 * M_PI / 180.0));
        } else if (strcmp(word, "snap") == 0) {
          RPNvalue p1 = pop();
          p1 = 50 * (int)(p1 / 50 + 0.5); /* snap to 50 mil grid */
          push(p1);
        } else if (strcmp(word, "sqrt") == 0) {
          RPNvalue p1 = pop();
          push(sqrt(p1));
        } else if (strcmp(word, "tan") == 0) {
          RPNvalue p1 = pop();
          push(tan(p1 * M_PI / 180.0));
        } else if (m_error == RPN_OK) {
          m_error = RPN_INV_FUNC;
        }
      }
    } else {
      /* operator */
      size_t len = strlen(word);
      if ((len > 2 || (len > 1 && word[0] != '<' && word[0] != '>' && word[0] != ')')) && m_error == RPN_OK)
        m_error = RPN_INV_OPER; /* with a few exceptions, only single-character operators are supported */
      RPNvalue p1, p2;
      switch (word[0]) {
      case '+':
        p2 = pop();
        p1 = pop();
        push(p1.Double() + p2.Double());
        break;
      case '-':
        p2 = pop();
        p1 = pop();
        push(p1.Double() - p2.Double());
        break;
      case '*':
        p2 = pop();
        p1 = pop();
        push(p1.Double() * p2.Double());
        break;
      case '/':
        p2 = pop();
        p1 = pop();
        if (p2.Double() == 0.0) {
          m_error = RPN_DIV_ZERO;
          push(RPNvalue(0.0));
        } else {
          push(p1.Double() / p2.Double());
        }
        break;
      case '~':
        p1 = pop();
        push(-p1.Double());
        break;
      case '=':
        p2 = pop();
        p1 = pop();
        push(p1.Double() >= p2.Double() - EPSILON && p1.Double() <= p2.Double() + EPSILON);
        break;
      case '<':
        p2 = pop();
        p1 = pop();
        if (word[1] == '=')
          p2.Set(p2.Double() + EPSILON); /* for p1 <= p2 use p1 < p2 + EPSILON */
        if (word[1] == '>') {
          /* <> is "different than" */
          push(p1.Double() < p2.Double() - EPSILON || p1.Double() > p2.Double() + EPSILON);
        } else {
          push(p1.Double() < p2.Double());
        }
        break;
      case '>':
        p2 = pop();
        p1 = pop();
        if (word[1] == '=')
          p2.Set(p2.Double() - EPSILON); /* for p1 >= p2 use p1 > p2 - EPSILON */
        push(p1.Double() > p2.Double());
        break;
      case '&':
        p2 = pop();
        p1 = pop();
        push(p1.Double() != 0 && p2.Double() != 0);
        break;
      case '|':
        p2 = pop();
        p1 = pop();
        push(p1.Double() != 0 || p2.Double() != 0);
        break;
      case '?':
        p1 = pop();   /* the condition (3rd expression) */
        if (p1.Double() != 0) {
          pop();      /* remove the 2nd expression, keep the 1st expression */
        } else {
          p1 = pop(); /* remove (but save) the 2nd expression */
          pop();      /* remove the 1st expression */
          push(p1);   /* restore the 2nd expression */
        }
        break;
	  case '(':
	    stackmark = m_top;
		break;
	  case ')':
		if (stackmark < 0) {
		  m_error = RPN_SYNTAX;
		} else {
		  if (word[1] == '=' && stackmark > 0) {
		    p1 = stack[stackmark - 1];
			int i;
			for (i = stackmark; i < m_top; i++) {
			  p2 = stack[i];
			  if (p1.Double() >= p2.Double() - EPSILON && p1.Double() <= p2.Double() + EPSILON)
			    break;
			}
			p1 = (i < m_top);	/* set to 1 if any of the values in the list match, 0 otherwise */
			m_top = stackmark - 1;	/* pop off the list and the parameter */
			push(p1);
		  } else {
			m_error = RPN_INV_OPER;
			m_top = stackmark;	/* pop off the list */
		  }
		}
		stackmark = -1;
	    break;
      default:
        if (m_error == RPN_OK)
          m_error = RPN_INV_OPER;
      }
    }
    start = tail;
  }

  if (m_error == RPN_OK) {
    if (m_top == 0)
      m_error = RPN_EMPTY;
    else if (m_top > 1)
      m_error = RPN_NOTSINGLE;
  }
  return m_error;
}

