// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE flow.empty

#include "caf/flow/observable_builder.hpp"

#include "core-test.hpp"

#include "caf/flow/scoped_coordinator.hpp"

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  flow::scoped_coordinator_ptr ctx = flow::make_scoped_coordinator();
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("an empty observable terminates normally") {
  GIVEN("an empty<int32>") {
    WHEN("an observer subscribes") {
      THEN("the observer receives on_complete") {
        auto uut = ctx->make_observable().empty<int32_t>();
        auto snk = flow::make_passive_observer<int32_t>();
        uut.subscribe(snk->as_observer());
        ctx->run();
        if (CHECK(snk->sub)) {
          snk->sub.request(42);
          ctx->run();
          CHECK_EQ(snk->state, flow::observer_state::completed);
          CHECK(snk->buf.empty());
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()
