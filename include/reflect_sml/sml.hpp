#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <meta>

namespace reflect_sml {
template<typename State>
struct transition {
    State from;
    State to;
};

template<typename State>
transition(State, State) -> transition<State>;

template<typename Callable>
struct guard {
    Callable callable;
};

template<typename Callable>
guard(Callable) -> guard<Callable>;

template<typename Callable>
struct action {
    Callable callable;
};

template<typename Callable>
action(Callable) -> action<Callable>;

namespace detail {
consteval bool is_named(std::meta::info member, std::string_view name) {
    return std::meta::has_identifier(member) && std::meta::identifier_of(member) == name;
}

template<typename State>
constexpr std::string_view state_name(State value) noexcept {
    static constexpr auto enums = std::define_static_array(std::meta::enumerators_of(^^State));
    auto out = std::string_view{};
    template for (constexpr auto i: enums) {
        if (value == std::meta::extract<State>(i)) out = std::meta::identifier_of(i);
    }
    return out;
}

template<typename StateMachine>
constexpr std::string_view state_name_of(typename StateMachine::state_type value) noexcept {
    return state_name<typename StateMachine::state_type>(value);
}

consteval bool is_transition_member(std::meta::info member) {
    if (!std::meta::is_function(member)) return false;
    if (std::meta::is_static_member(member)) return false;
    if (!std::meta::has_identifier(member)) return false;
    return true;
}

template<typename Callable, typename Self, typename Event>
constexpr decltype(auto) invoke_callable(Callable&& callable, Self& self, Event const& event) {
    if constexpr (std::is_invocable_v<Callable&, Self&, Event const&>) {
        return std::invoke(std::forward<Callable>(callable), self, event);
    } else if constexpr (std::is_invocable_v<Callable&, Event const&>) {
        return std::invoke(std::forward<Callable>(callable), event);
    } else {
        return std::invoke(std::forward<Callable>(callable));
    }
}

template<std::meta::info Member, typename Self, typename Event>
constexpr bool run_guards(Self& self, Event const& event) {
    auto ok = true;
    static constexpr auto all_annotations = std::define_static_array(std::meta::annotations_of(Member));
    template for (constexpr auto annotation: all_annotations) {
        constexpr auto annotation_type = std::meta::type_of(annotation);
        if constexpr (
            std::meta::has_template_arguments(annotation_type) && std::meta::template_of(annotation_type) == ^^guard
        ) {
            constexpr auto value = std::meta::extract<typename[:annotation_type:]>(annotation);
            if (ok && !invoke_callable(value.callable, self, event)) ok = false;
        }
    }
    return ok;
}

template<std::meta::info Member, typename Self, typename Event>
constexpr void run_actions(Self& self, Event const& event) {
    static constexpr auto all_annotations = std::define_static_array(std::meta::annotations_of(Member));
    template for (constexpr auto annotation: all_annotations) {
        constexpr auto annotation_type = std::meta::type_of(annotation);
        if constexpr (
            std::meta::has_template_arguments(annotation_type) && std::meta::template_of(annotation_type) == ^^action
        ) {
            constexpr auto value = std::meta::extract<typename[:annotation_type:]>(annotation);
            invoke_callable(value.callable, self, event);
        }
    }
}

template<std::meta::info Member, typename State>
consteval transition<State> transition_of() {
    static constexpr auto annotations =
        std::define_static_array(std::meta::annotations_of_with_type(Member, ^^transition<State>));
    return std::meta::extract<transition<State>>(annotations[0]);
}

template<std::meta::info Member, typename State>
consteval bool transition_touches(State value) {
    static constexpr auto annotations =
        std::define_static_array(std::meta::annotations_of_with_type(Member, ^^transition<State>));
    if constexpr (annotations.size() != 1) {
        return false;
    } else {
        constexpr auto edge = std::meta::extract<transition<State>>(annotations[0]);
        return edge.from == value || edge.to == value;
    }
}

template<typename StateMachine, typename State>
consteval bool any_transition_touches(State value) {
    auto mentioned = false;
    static constexpr auto context = std::meta::access_context::current();
    template for (constexpr auto j: std::define_static_array(std::meta::members_of(^^StateMachine, context))) {
        if constexpr (is_transition_member(j)) {
            if (transition_touches<j>(value)) mentioned = true;
        }
    }
    return mentioned;
}

template<std::meta::info Member, typename State>
consteval bool has_transition_annotation() {
    static constexpr auto annotations =
        std::define_static_array(std::meta::annotations_of_with_type(Member, ^^transition<State>));
    return annotations.size() == 1;
}

template<std::meta::info Member, typename Event>
consteval bool accepts_event() {
    static constexpr auto parameters = std::define_static_array(std::meta::parameters_of(Member));
    if constexpr (parameters.size() != 1) {
        return false;
    } else {
        using event_type = std::remove_cvref_t<typename[:std::meta::type_of(parameters[0]):]>;
        return std::is_same_v<event_type, Event>;
    }
}

template<std::meta::info Member, typename State, typename Event>
consteval bool is_handler_for() {
    if constexpr (!is_transition_member(Member)) {
        return false;
    } else if constexpr (!has_transition_annotation<Member, State>()) {
        return false;
    } else {
        return accepts_event<Member, Event>();
    }
}

template<std::meta::info Member, typename StateMachine, typename Event>
constexpr bool try_fire(StateMachine& impl, typename StateMachine::state_type& current, Event const& event) {
    using State = typename StateMachine::state_type;
    constexpr auto edge = transition_of<Member, State>();
    if (current != edge.from) return false;
    if (!run_guards<Member>(impl, event)) return false;
    run_actions<Member>(impl, event);
    (impl.[:Member:])(event);
    current = edge.to;
    return true;
}
} // namespace detail

template<typename StateMachine>
consteval bool is_exhaustive() {
    using State = typename StateMachine::state_type;
    static constexpr auto enums = std::define_static_array(std::meta::enumerators_of(^^State));

    auto ok = true;
    template for (constexpr auto i: enums) {
        constexpr auto value = std::meta::extract<State>(i);
        if (value != StateMachine::initial_state && !detail::any_transition_touches<StateMachine>(value)) ok = false;
    }
    return ok;
}

template<typename StateMachine>
class machine {
public:
    using state_type = typename StateMachine::state_type;

private:
    StateMachine implementation_{};
    state_type current_{StateMachine::initial_state};

public:
    constexpr machine() = default;

    explicit constexpr machine(StateMachine implementation) : implementation_(std::move(implementation)) {}

    constexpr state_type current() const noexcept {
        return current_;
    }

    constexpr std::string_view current_name() const noexcept {
        return detail::state_name_of<StateMachine>(current_);
    }

    template<typename Event>
    constexpr bool dispatch(Event const& event) {
        auto fired = false;
        static constexpr auto context = std::meta::access_context::current();
        template for (constexpr auto i: std::define_static_array(std::meta::members_of(^^StateMachine, context))) {
            if constexpr (detail::is_handler_for<i, state_type, Event>()) {
                if (!fired && detail::try_fire<i>(implementation_, current_, event)) fired = true;
            }
        }
        return fired;
    }

    constexpr StateMachine& implementation() noexcept {
        return implementation_;
    }

    constexpr StateMachine const& implementation() const noexcept {
        return implementation_;
    }
};
} // namespace reflect_sml
