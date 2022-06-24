/*
 * nghttp3
 *
 * Copyright (c) 2020 nghttp3 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "nghttp3_http_test.h"

#include <assert.h>

#include <CUnit/CUnit.h>

#include "nghttp3_http.h"
#include "nghttp3_macro.h"
#include "nghttp3_test_helper.h"

void test_nghttp3_http_parse_priority(void) {
  int rv;

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(0 == rv);
    CU_ASSERT((uint32_t)-1 == pri.urgency);
    CU_ASSERT(-1 == pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=7,i";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(0 == rv);
    CU_ASSERT((uint32_t)7 == pri.urgency);
    CU_ASSERT(1 == pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=0,i=?0";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(0 == rv);
    CU_ASSERT((uint32_t)0 == pri.urgency);
    CU_ASSERT(0 == pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=3, i";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(0 == rv);
    CU_ASSERT((uint32_t)3 == pri.urgency);
    CU_ASSERT(1 == pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=0, i, i=?0, u=6";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(0 == rv);
    CU_ASSERT((uint32_t)6 == pri.urgency);
    CU_ASSERT(0 == pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=0,";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=0, ";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "i=?1";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(0 == rv);
    CU_ASSERT((uint32_t)-1 == pri.urgency);
    CU_ASSERT(1 == pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "i=?2";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "i=?";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "i=";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=-1";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = "u=8";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] =
        "i=?0, u=1, a=(x y z), u=2; i=?0;foo=\",,,\", i=?1;i=?0; u=6";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    CU_ASSERT(0 == rv);
    CU_ASSERT((uint32_t)2 == pri.urgency);
    CU_ASSERT(1 == pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, -1};
    const uint8_t v[] = {'u', '='};

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v));

    CU_ASSERT(NGHTTP3_ERR_INVALID_ARGUMENT == rv);
  }
}

void test_nghttp3_sf_parse_item(void) {
  {
    nghttp3_sf_value val;
    const uint8_t s[] = "?1";
    val.type = 0xff;

    CU_ASSERT(2 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BOOLEAN == val.type);
    CU_ASSERT(1 == val.b);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "?1 ";
    val.type = 0xff;

    CU_ASSERT(2 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BOOLEAN == val.type);
    CU_ASSERT(1 == val.b);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "?1;foo=bar";
    val.type = 0xff;

    CU_ASSERT(10 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BOOLEAN == val.type);
    CU_ASSERT(1 == val.b);
  }

  {
    const uint8_t s[] = {'?', '1', ';', 'f', 'o', 'o', '='};

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s)));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "?0";
    val.type = 0xff;

    CU_ASSERT(2 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BOOLEAN == val.type);
    CU_ASSERT(0 == val.b);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "?0 ";
    val.type = 0xff;

    CU_ASSERT(2 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BOOLEAN == val.type);
    CU_ASSERT(0 == val.b);
  }

  {
    const uint8_t s[] = "?2";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "?";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "?1";

    CU_ASSERT(2 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = ":cHJldGVuZCB0aGlzIGlzIGJpbmFyeSBjb250ZW50Lg==:";
    val.type = 0xff;

    CU_ASSERT(46 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BYTESEQ == val.type);
    CU_ASSERT(44 == val.s.len);
    CU_ASSERT(0 == memcmp("cHJldGVuZCB0aGlzIGlzIGJpbmFyeSBjb250ZW50Lg==",
                          val.s.base, val.s.len));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = ":cHJldGVuZCB0aGlzIGlzIGJpbmFyeSBjb250ZW50Lg==: ";
    val.type = 0xff;

    CU_ASSERT(46 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BYTESEQ == val.type);
    CU_ASSERT(44 == val.s.len);
    CU_ASSERT(0 == memcmp("cHJldGVuZCB0aGlzIGlzIGJpbmFyeSBjb250ZW50Lg==",
                          val.s.base, val.s.len));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "::";
    val.type = 0xff;

    CU_ASSERT(2 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BYTESEQ == val.type);
    CU_ASSERT(0 == val.s.len);
  }

  {
    const uint8_t s[] = ":cHJldGVuZCB0aGlzIGlzIGJpbmFyeSBjb250ZW50Lg==";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = ":";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = ":@:";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = ":foo:";

    CU_ASSERT(5 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] =
        ":abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+/=:";
    val.type = 0xff;

    CU_ASSERT(67 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_BYTESEQ == val.type);
    CU_ASSERT(65 == val.s.len);
    CU_ASSERT(
        0 ==
        memcmp(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+/=",
            val.s.base, val.s.len));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "foo123/456";
    val.type = 0xff;

    CU_ASSERT(10 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_TOKEN == val.type);
    CU_ASSERT(10 == val.s.len);
    CU_ASSERT(0 == memcmp(s, val.s.base, val.s.len));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "foo123/456 ";
    val.type = 0xff;

    CU_ASSERT(10 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_TOKEN == val.type);
    CU_ASSERT(10 == val.s.len);
    CU_ASSERT(0 == memcmp(s, val.s.base, val.s.len));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "*";
    val.type = 0xff;

    CU_ASSERT(1 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_TOKEN == val.type);
    CU_ASSERT(1 == val.s.len);
    CU_ASSERT(0 == memcmp(s, val.s.base, val.s.len));
  }

  {
    const uint8_t s[] = "*";

    CU_ASSERT(1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "\"hello world\"";
    val.type = 0xff;

    CU_ASSERT(13 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_STRING == val.type);
    CU_ASSERT(11 == val.s.len);
    CU_ASSERT(0 == memcmp("hello world", val.s.base, val.s.len));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "\"hello world\" ";
    val.type = 0xff;

    CU_ASSERT(13 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_STRING == val.type);
    CU_ASSERT(11 == val.s.len);
    CU_ASSERT(0 == memcmp("hello world", val.s.base, val.s.len));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "\"foo\\\"\\\\\"";
    val.type = 0xff;

    CU_ASSERT(9 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_STRING == val.type);
    CU_ASSERT(7 == val.s.len);
    CU_ASSERT(0 == memcmp("foo\\\"\\\\", val.s.base, val.s.len));
  }

  {
    const uint8_t s[] = "\"foo\\x\"";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "\"foo";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "\"\x7f\"";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "\"\x1f\"";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "\"foo\"";

    CU_ASSERT(5 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "4.5";
    val.type = NGHTTP3_SF_VALUE_TYPE_DECIMAL;

    CU_ASSERT(3 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_DECIMAL == val.type);
    CU_ASSERT(fabs(4.5 - val.d) < 1e-9);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "4.5 ";
    val.type = NGHTTP3_SF_VALUE_TYPE_DECIMAL;

    CU_ASSERT(3 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_DECIMAL == val.type);
    CU_ASSERT(fabs(4.5 - val.d) < 1e-9);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "-4.5";
    val.type = NGHTTP3_SF_VALUE_TYPE_DECIMAL;

    CU_ASSERT(4 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_DECIMAL == val.type);
    CU_ASSERT(fabs(-4.5 - val.d) < 1e-9);
  }

  {
    const uint8_t s[] = "4.5";

    CU_ASSERT(3 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "123456789012.123";
    val.type = NGHTTP3_SF_VALUE_TYPE_DECIMAL;

    CU_ASSERT(16 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_DECIMAL == val.type);
    CU_ASSERT(fabs(123456789012.123 - val.d) < 1e-9);
  }

  {
    const uint8_t s[] = "1123456789012.123";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "123456789012.1234";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "1.";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "123456789012345";
    val.type = NGHTTP3_SF_VALUE_TYPE_DECIMAL;

    CU_ASSERT(15 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_INTEGER == val.type);
    CU_ASSERT(123456789012345 == val.i);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "1 ";
    val.type = NGHTTP3_SF_VALUE_TYPE_DECIMAL;

    CU_ASSERT(1 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_INTEGER == val.type);
    CU_ASSERT(1 == val.i);
  }

  {
    const uint8_t s[] = "1";

    CU_ASSERT(1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "1234567890123456";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "\"foo\";a;  b=\"bar\";c=1.3;d=9;e=baz;f=:aaa:";
    val.type = 0xff;

    CU_ASSERT(41 == nghttp3_sf_parse_item(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_STRING == val.type);
    CU_ASSERT(0 == memcmp("foo", val.s.base, val.s.len));
  }

  {
    const uint8_t s[] = "\"foo\";a;  b=\"bar";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "foo;";

    CU_ASSERT(-1 == nghttp3_sf_parse_item(NULL, s, s + sizeof(s) - 1));
  }
}

void test_nghttp3_sf_parse_inner_list(void) {
  {
    nghttp3_sf_value val;
    const uint8_t s[] = "()";
    val.type = 0xff;

    CU_ASSERT(2 == nghttp3_sf_parse_inner_list(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_INNER_LIST == val.type);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "(     )";
    val.type = 0xff;

    CU_ASSERT(7 == nghttp3_sf_parse_inner_list(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_INNER_LIST == val.type);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "(a)";
    val.type = 0xff;

    CU_ASSERT(3 == nghttp3_sf_parse_inner_list(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_INNER_LIST == val.type);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "(a b)";
    val.type = 0xff;

    CU_ASSERT(5 == nghttp3_sf_parse_inner_list(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_INNER_LIST == val.type);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "(  a b   )";
    val.type = 0xff;

    CU_ASSERT(10 == nghttp3_sf_parse_inner_list(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_INNER_LIST == val.type);
  }

  {
    nghttp3_sf_value val;
    const uint8_t s[] = "( a;foo=bar)";
    val.type = 0xff;

    CU_ASSERT(12 == nghttp3_sf_parse_inner_list(&val, s, s + sizeof(s) - 1));
    CU_ASSERT(NGHTTP3_SF_VALUE_TYPE_INNER_LIST == val.type);
  }

  {
    const uint8_t s[] = "(";

    CU_ASSERT(-1 == nghttp3_sf_parse_inner_list(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "(a";

    CU_ASSERT(-1 == nghttp3_sf_parse_inner_list(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "(a   ";

    CU_ASSERT(-1 == nghttp3_sf_parse_inner_list(NULL, s, s + sizeof(s) - 1));
  }

  {
    const uint8_t s[] = "(a;b";

    CU_ASSERT(-1 == nghttp3_sf_parse_inner_list(NULL, s, s + sizeof(s) - 1));
  }
}

#define check_header_value(S)                                                  \
  nghttp3_check_header_value((const uint8_t *)S, sizeof(S) - 1)

void test_nghttp3_check_header_value(void) {
  uint8_t goodval[] = {'a', 'b', 0x80u, 'c', 0xffu, 'd'};
  uint8_t badval1[] = {'a', 0x1fu, 'b'};
  uint8_t badval2[] = {'a', 0x7fu, 'b'};

  CU_ASSERT(check_header_value("!|}~"));
  CU_ASSERT(!check_header_value(" !|}~"));
  CU_ASSERT(!check_header_value("!|}~ "));
  CU_ASSERT(!check_header_value("\t!|}~"));
  CU_ASSERT(!check_header_value("!|}~\t"));
  CU_ASSERT(check_header_value(goodval));
  CU_ASSERT(!check_header_value(badval1));
  CU_ASSERT(!check_header_value(badval2));
  CU_ASSERT(check_header_value(""));
  CU_ASSERT(!check_header_value(" "));
  CU_ASSERT(!check_header_value("\t"));
}
