// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE config_option

#include "caf/config_option.hpp"

#include "caf/config_value.hpp"
#include "caf/expected.hpp"
#include "caf/make_config_option.hpp"

#include "core-test.hpp"

#include <sstream>

using namespace caf;
using namespace std::literals;

using std::string;

namespace {

struct state;

struct baseline {
  std::vector<std::string> cli;
  std::string conf;
  settings res;
  std::function<bool(const state&)> predicate;
};

struct request_pair {
  my_request first;
  my_request second;
};

template <class Inspector>
bool inspect(Inspector& f, request_pair& x) {
  return f.object(x).fields(f.field("first", x.first),
                            f.field("second", x.second));
}

struct state {
  s1 my_app_s1;
  std::vector<int32_t> my_app_vector;
  level my_app_severity = level::trace;
  my_request my_app_request;
  request_pair my_app_request_pair;
  config_option_set options;

  state() {
    config_option_adder{options, "?my.app"}
      .add(my_app_s1, "s1", "")
      .add(my_app_vector, "vector,v", "")
      .add(my_app_severity, "severity,s", "")
      .add(my_app_request, "request,r", "")
      .add(my_app_request_pair, "request-pair,R", "");
    config_option_adder{options, "sys"}
      .add<std::string>("query,q", "")
      .add<int8_t>("threads,tTd", "");
  }

  void run(baseline& x, size_t index) {
    settings res;
    std::istringstream src{x.conf};
    if (auto parsed = actor_system_config::parse_config(src, options)) {
      res = std::move(*parsed);
    } else {
      CAF_ERROR("failed to parse baseline at index " << index << ": "
                                                     << parsed.error());
      return;
    }
    auto [code, pos] = options.parse(res, x.cli);
    if (pos != x.cli.end()) {
      CAF_ERROR("failed to parse all arguments for baseline at index "
                << index << ", stopped at: " << *pos << " (" << code << ')');
      return;
    }
    if (code != pec::success) {
      CAF_ERROR("CLI arguments for baseline at index "
                << index << " failed to parse: " << code);
      return;
    }
    if (!x.predicate(*this)) {
      CAF_ERROR("predicate for baseline at index " << index << "failed! ");
      return;
    }
    MESSAGE("all checks for baseline at index " << index << " passed");
  }
};

struct fixture {
  std::vector<baseline> baselines;

  template <class Predicate>
  void add_test(std::vector<std::string> cli, std::string conf, settings res,
                Predicate f) {
    baselines.emplace_back(baseline{
      std::move(cli),
      std::move(conf),
      std::move(res),
      f,
    });
  }

  template <class Predicate>
  void add_test(std::vector<std::string> cli, std::string conf, std::string res,
                Predicate f) {
    config_value cv_res{res};
    if (auto parsed = get_as<settings>(cv_res))
      add_test(std::move(cli), std::move(conf), std::move(*parsed),
               std::move(f));
    else
      CAF_FAIL("failed to parse result settings: " << parsed.error()
                                                   << "\nINPUT:\n"
                                                   << res << '\n');
  }

  template <class Res>
  void add_test(std::vector<std::string> cli, std::string conf, Res&& res) {
    return add_test(std::move(cli), std::move(conf), std::forward<Res>(res),
                    [](const state&) { return true; });
  }

  fixture() {
    using ivec = std::vector<int32_t>;
    add_test({"-s", "error"}, "", R"_(my { app { severity = "error" } })_",
             [](auto& st) {
               return CHECK_EQ(st.my_app_severity, level::error);
             });
    add_test({"-v", "1, 2, 3"}, "", R"_(my { app { vector = [1, 2, 3] } })_",
             [](auto& st) {
               return CHECK_EQ(st.my_app_vector, ivec({1, 2, 3}));
             });
    add_test({"-v", "[1, 2, 3]"}, "", R"_(my { app { vector = [1, 2, 3] } })_");
    add_test({"-v[1, 2, 3]"}, "", R"_(my { app { vector = [1, 2, 3] } })_");
    add_test({"-v1, 2, 3,"}, "", R"_(my { app { vector = [1, 2, 3] } })_");
    add_test({"-r", R"_({"a":1,"b":2})_"}, "",
             R"_(my { app { request { a = 1, b = 2 } } })_");
    add_test({"-r", R"_(a=1,b=2)_"}, "",
             R"_(my { app { request { a = 1, b = 2 } } })_");
    add_test({R"_(--my.app.request={a=1,b=2})_"}, "",
             R"_(my { app { request { a = 1, b = 2 } } })_");
    add_test({R"_(--my.app.request=a=1,b=2,)_"}, "",
             R"_(my { app { request { a = 1, b = 2 } } })_");
    add_test({"-R",
              R"_({"first": {"a": 1, "b": 2}, "second": {"a": 3, "b": 4}})_"},
             "",
             R"_(my { app { request-pair {  first { a = 1, b = 2 },
                                    second { a = 3, b = 4 } } } })_");
    add_test({}, "sys{threads=2}", R"_(sys { threads = 2 })_");
    add_test({"-t", "1"}, "sys{threads=2}", R"_(sys { threads = 1 })_");
    add_test({"-T", "1"}, "sys{threads=2}", R"_(sys { threads = 1 })_");
    add_test({"-d", "1"}, "sys{threads=2}", R"_(sys { threads = 1 })_");
    add_test({"--sys.threads=1"}, "sys{threads=2}", R"_(sys { threads = 1 })_");
    add_test({"--sys.query=foo"}, "", R"_(sys { query = "foo" })_");
    add_test({"-q", "\"a\" in b"}, "", R"_(sys { query = "\"a\" in b" })_");
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("options on the CLI override config files that override defaults") {
  for (size_t index = 0; index < baselines.size(); ++index) {
    state st;
    st.run(baselines[index], index);
  }
}

constexpr std::string_view category = "category";
constexpr std::string_view name = "name";
constexpr std::string_view explanation = "explanation";

template <class T>
constexpr int64_t overflow() {
  return static_cast<int64_t>(std::numeric_limits<T>::max()) + 1;
}

template <class T>
constexpr int64_t underflow() {
  return static_cast<int64_t>(std::numeric_limits<T>::min()) - 1;
}

template <class T>
std::optional<T> read(std::string_view arg) {
  auto result = T{};
  auto co = make_config_option<T>(result, category, name, explanation);
  config_value val{arg};
  if (auto err = co.sync(val); !err)
    return {std::move(result)};
  else
    return std::nullopt;
}

// Unsigned integers.
template <class T>
void check_integer_options(std::true_type) {
  using std::to_string;
  // Run tests for positive integers.
  T xzero = 0;
  T xmax = std::numeric_limits<T>::max();
  CHECK_EQ(read<T>(to_string(xzero)), xzero);
  CHECK_EQ(read<T>(to_string(xmax)), xmax);
  CHECK_EQ(read<T>(to_string(overflow<T>())), std::nullopt);
}

// Signed integers.
template <class T>
void check_integer_options(std::false_type) {
  using std::to_string;
  // Run tests for positive integers.
  std::true_type tk;
  check_integer_options<T>(tk);
  // Run tests for negative integers.
  auto xmin = std::numeric_limits<T>::min();
  CHECK_EQ(read<T>(to_string(xmin)), xmin);
  CHECK_EQ(read<T>(to_string(underflow<T>())), std::nullopt);
}

// only works with an integral types and double
template <class T>
void check_integer_options() {
  std::integral_constant<bool, std::is_unsigned_v<T>> tk;
  check_integer_options<T>(tk);
}

void compare(const config_option& lhs, const config_option& rhs) {
  CHECK_EQ(lhs.category(), rhs.category());
  CHECK_EQ(lhs.long_name(), rhs.long_name());
  CHECK_EQ(lhs.short_names(), rhs.short_names());
  CHECK_EQ(lhs.description(), rhs.description());
  CHECK_EQ(lhs.full_name(), rhs.full_name());
}

CAF_TEST(copy constructor) {
  auto one = make_config_option<int>("cat1"sv, "one"sv, "option 1"sv);
  auto two = one;
  compare(one, two);
}

CAF_TEST(copy assignment) {
  auto one = make_config_option<int>("cat1"sv, "one"sv, "option 1"sv);
  auto two = make_config_option<int>("cat2"sv, "two"sv, "option 2"sv);
  two = one;
  compare(one, two);
}

CAF_TEST(type_bool) {
  CHECK_EQ(read<bool>("true"), true);
  CHECK_EQ(read<bool>("false"), false);
  CHECK_EQ(read<bool>("0"), std::nullopt);
  CHECK_EQ(read<bool>("1"), std::nullopt);
}

CAF_TEST(type int8_t) {
  check_integer_options<int8_t>();
}

CAF_TEST(type uint8_t) {
  check_integer_options<uint8_t>();
}

CAF_TEST(type int16_t) {
  check_integer_options<int16_t>();
}

CAF_TEST(type uint16_t) {
  check_integer_options<uint16_t>();
}

CAF_TEST(type int32_t) {
  check_integer_options<int32_t>();
}

CAF_TEST(type uint32_t) {
  check_integer_options<uint32_t>();
}

CAF_TEST(type uint64_t) {
  CHECK_EQ(unbox(read<uint64_t>("0")), 0u);
  CHECK_EQ(read<uint64_t>("-1"), std::nullopt);
}

CAF_TEST(type int64_t) {
  CHECK_EQ(unbox(read<int64_t>("-1")), -1);
  CHECK_EQ(unbox(read<int64_t>("0")), 0);
  CHECK_EQ(unbox(read<int64_t>("1")), 1);
}

CAF_TEST(type float) {
  CHECK_EQ(unbox(read<float>("-1.0")), -1.0f);
  CHECK_EQ(unbox(read<float>("-0.1")), -0.1f);
  CHECK_EQ(read<float>("0"), 0.f);
  CHECK_EQ(read<float>("\"0.1\""), std::nullopt);
}

CAF_TEST(type double) {
  CHECK_EQ(unbox(read<double>("-1.0")), -1.0);
  CHECK_EQ(unbox(read<double>("-0.1")), -0.1);
  CHECK_EQ(read<double>("0"), 0.);
  CHECK_EQ(read<double>("\"0.1\""), std::nullopt);
}

CAF_TEST(type string) {
  CHECK_EQ(unbox(read<string>("foo")), "foo");
  CHECK_EQ(unbox(read<string>(R"_("foo")_")), R"_("foo")_");
}

CAF_TEST(type timespan) {
  timespan dur{500};
  CHECK_EQ(read<timespan>("500ns"), dur);
}

CAF_TEST(lists) {
  using int_list = std::vector<int>;
  CHECK_EQ(read<int_list>("[]"), int_list({}));
  CHECK_EQ(read<int_list>("1, 2, 3"), int_list({1, 2, 3}));
  CHECK_EQ(read<int_list>("[1, 2, 3]"), int_list({1, 2, 3}));
}

CAF_TEST(flat CLI parsing) {
  auto x = make_config_option<std::string>("?foo", "bar,b", "test option");
  CHECK_EQ(x.category(), "foo");
  CHECK_EQ(x.long_name(), "bar");
  CHECK_EQ(x.short_names(), "b");
  CHECK_EQ(x.full_name(), "foo.bar");
  CHECK_EQ(x.has_flat_cli_name(), true);
}

CAF_TEST(flat CLI parsing with nested categories) {
  auto x = make_config_option<std::string>("?foo.goo", "bar,b", "test option");
  CHECK_EQ(x.category(), "foo.goo");
  CHECK_EQ(x.long_name(), "bar");
  CHECK_EQ(x.short_names(), "b");
  CHECK_EQ(x.full_name(), "foo.goo.bar");
  CHECK_EQ(x.has_flat_cli_name(), true);
}

CAF_TEST(find by long opt) {
  auto needle = make_config_option<std::string>("?foo"sv, "bar,b"sv,
                                                "test option"sv);
  auto check = [&](std::vector<string> args, bool found_opt, bool has_opt) {
    auto res = needle.find_by_long_name(std::begin(args), std::end(args));
    CHECK_EQ(res.begin != std::end(args), found_opt);
    if (has_opt)
      CHECK_EQ(res.value, "val2");
    else
      CHECK(res.value.empty());
  };
  // Well formed, find val2.
  check({"--foo=val1", "--bar=val2", "--baz=val3"}, true, true);
  check({"--foo=val1", "--bar", "val2", "--baz=val3"}, true, true);
  // Dashes missing, no match.
  check({"--foo=val1", "bar=val2", "--baz=val3"}, false, false);
  // Equal missing.
  check({"--fooval1", "--barval2", "--bazval3"}, false, false);
  // Option value missing.
  check({"--foo=val1", "--bar=", "--baz=val3"}, true, false);
  // Option not included.
  check({"--foo=val1", "--b4r=val2", "--baz=val3"}, false, false);
  // No options to look through.
  check({}, false, false);
  // flag option for booleans
  needle = make_config_option<bool>("?foo"sv, "bar,b"sv, "test option"sv);
  check({"--foo=val1", "--bar", "--baz=val3"}, true, false);
}

END_FIXTURE_SCOPE()
