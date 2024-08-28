/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
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
#include "qpack.h"

#include <cstring>
#include <iostream>
#include <string>

#include <getopt.h>

#include "qpack_encode.h"
#include "qpack_decode.h"

namespace nghttp3 {

Config config{};

namespace {
void print_usage() {
  std::cerr << "Usage: qpack [OPTIONS] <COMMAND> <INFILE> <OUTFILE>"
            << std::endl;
}
} // namespace

namespace {
void print_help() {
  print_usage();

  std::cerr << R"(
  <COMMAND>   "encode" or "decode"
  <INFILE>    Path to an input file
  <OUTFILE>   Path to an output file
Options:
  -h, --help  Display this help and exit.
  -m, --max-blocked=<N>
              The maximum number of streams which are permitted to be blocked.
  -s, --max-dtable-size=<N>
              The maximum size of dynamic table.
  -a, --immediate-ack
              Turn on immediate acknowledgement.
)";
}
} // namespace

int main(int argc, char **argv) {
  for (;;) {
    static int flag = 0;
    (void)flag;
    constexpr static option long_opts[] = {
      {"help", no_argument, nullptr, 'h'},
      {"max-blocked", required_argument, nullptr, 'm'},
      {"max-dtable-size", required_argument, nullptr, 's'},
      {"immediate-ack", no_argument, nullptr, 'a'},
      {nullptr, 0, nullptr, 0},
    };

    auto optidx = 0;
    auto c = getopt_long(argc, argv, "hm:s:a", long_opts, &optidx);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'h':
      // --help
      print_help();
      exit(EXIT_SUCCESS);
    case 'm': {
      // --max-blocked
      config.max_blocked = strtoul(optarg, nullptr, 10);
      break;
    }
    case 's': {
      // --max-dtable-size
      config.max_dtable_size = strtoul(optarg, nullptr, 10);
      break;
    }
    case 'a':
      // --immediate-ack
      config.immediate_ack = true;
      break;
    case '?':
      print_usage();
      exit(EXIT_FAILURE);
    case 0:
      break;
    default:
      break;
    };
  }

  if (argc - optind < 3) {
    std::cerr << "Too few arguments" << std::endl;
    print_usage();
    exit(EXIT_FAILURE);
  }

  auto command = std::string_view(argv[optind++]);
  auto infile = std::string_view(argv[optind++]);
  auto outfile = std::string_view(argv[optind++]);

  int rv;
  if (command == "encode") {
    rv = encode(outfile, infile);
  } else if (command == "decode") {
    rv = decode(outfile, infile);
  } else {
    std::cerr << "Unrecognized command: " << command << std::endl;
    print_usage();
    exit(EXIT_FAILURE);
  }

  if (rv != 0) {
    exit(EXIT_FAILURE);
  }

  return 0;
}

} // namespace nghttp3

int main(int argc, char **argv) { return nghttp3::main(argc, argv); }
