#include <print>

#include <reflect_sml/sml.hpp>

#ifndef BUG_FIXED
#    define BUG_FIXED 0
#endif

enum class state {
    idle,
    connecting,
    connected,
    disconnected, // Always declared. Bug = no transition references it.
};

struct connect {};

struct established {};

struct disconnect {};

struct fsm {
    using state_type = state;
    static constexpr auto initial_state = state::idle;

    // clang-format off
    [[= reflect_sml::transition{state::idle, state::connecting}]]
    void on(connect const&) {}

    [[= reflect_sml::transition{state::connecting, state::connected}]]
    void on(established const&) {}

#if BUG_FIXED
    // The "fix": this is the transition the dev forgot to add. Once it's
    // back, every enum value is referenced and the assertion passes.
    [[= reflect_sml::transition{state::connected, state::disconnected}]]
    void on(disconnect const&) {}
#endif
    // clang-format on
};

static_assert(
    reflect_sml::is_exhaustive<fsm>(),
    "fsm has a state with no transitions — every state value must appear "
    "as a 'from' or 'to' in some transition"
);

int main() {
    std::println("Reached main(): BUG_FIXED build, FSM is well-formed.");
}
