#include <print>
#include <string>

#include <boost/sml.hpp>
#include <reflect_sml/sml.hpp>

// Shared pool — both FSMs below consume these.

struct connect {
    std::string host{};
};

struct established {};

struct ping {};

struct timeout {};

struct disconnect {};

constexpr auto host_is_valid = [](connect const& event) { return !event.host.empty(); };
constexpr auto log_dial = [](connect const& event) { std::println("    dial {}", event.host); };
constexpr auto tickle = [] { std::println("    tickle"); };
constexpr auto backoff = [] { std::println("    backoff"); };
constexpr auto close = [] { std::println("    close"); };

bool network_is_up() {
    return true;
}

namespace classic {
namespace sml = boost::sml;

struct fsm {
    void log_handshake(established const&) const {
        std::println("    handshake");
    }

    auto operator()() const {
        // Boost.SML's `callable` concept rejects raw free-function pointers,
        // so the classic side wraps `network_is_up` in a local lambda.
        constexpr auto net_up = [] { return network_is_up(); };
        using namespace sml;
        return make_transition_table(
            *"idle"_s + event<connect>[host_is_valid] / log_dial = "connecting"_s,
            "connecting"_s + event<established>[net_up] / &fsm::log_handshake = "connected"_s,
            "connected"_s + event<ping> / tickle = "connected"_s,
            "connected"_s + event<timeout> / backoff = "connecting"_s,
            "connected"_s + event<disconnect> / close = "disconnected"_s
        );
    }
};
} // namespace classic

namespace reflective {
enum class state {
    idle,
    connecting,
    connected,
    disconnected,
};

struct fsm {
    using state_type = state;
    static constexpr auto initial_state = state::idle;

    // PMF — the niche third callable form.
    void log_handshake(established const&) const {
        std::println("    handshake");
    }

    // Method names are free — the library identifies handlers by the
    // transition{} annotation and the parameter type, not by name.
    // clang-format off

    [[= reflect_sml::transition{state::idle, state::connecting}]]
    [[= reflect_sml::guard{host_is_valid}]]
    [[= reflect_sml::action{log_dial}]]
    void dial(connect const&) {}

    [[= reflect_sml::transition{state::connecting, state::connected}]]
    [[= reflect_sml::guard{&network_is_up}]]
    [[= reflect_sml::action{&fsm::log_handshake}]]
    void complete(established const&) {}

    [[= reflect_sml::transition{state::connected, state::connected}]]
    [[= reflect_sml::action{tickle}]]
    void keepalive(ping const&) {}

    [[= reflect_sml::transition{state::connected, state::connecting}]]
    [[= reflect_sml::action{backoff}]]
    void retry(timeout const&) {}

    [[= reflect_sml::transition{state::connected, state::disconnected}]]
    [[= reflect_sml::action{close}]]
    void shutdown(disconnect const&) {}

    // clang-format on
};

static_assert(reflect_sml::is_exhaustive<fsm>());
} // namespace reflective

int main() {
    using namespace boost::sml;

    std::println("============================================");
    std::println(" CLASSIC Boost.SML");
    std::println("============================================");

    auto classic_fsm = classic::fsm{};
    auto classic_machine = boost::sml::sm<classic::fsm>{classic_fsm};
    classic_machine.process_event(connect{""}); // guard blocks — no dial
    classic_machine.process_event(connect{"example.com"});
    classic_machine.process_event(established{});
    classic_machine.process_event(ping{});
    classic_machine.process_event(timeout{});
    classic_machine.process_event(established{});
    classic_machine.process_event(disconnect{});

    std::println("  ended in 'disconnected'? {}", classic_machine.is("disconnected"_s));

    std::println();
    std::println("============================================");
    std::println(" REFLECTIVE (C++26)");
    std::println("============================================");

    auto reflective_machine = reflect_sml::machine<reflective::fsm>{};
    std::println("  initial: {}", reflective_machine.current_name());
    reflective_machine.dispatch(connect{""}); // guard blocks — no dial
    std::println("  after empty connect: {}", reflective_machine.current_name());
    reflective_machine.dispatch(connect{"example.com"});
    reflective_machine.dispatch(established{});
    reflective_machine.dispatch(ping{});
    reflective_machine.dispatch(timeout{});
    reflective_machine.dispatch(established{});
    reflective_machine.dispatch(disconnect{});

    std::println("  final:   {}", reflective_machine.current_name());
}
