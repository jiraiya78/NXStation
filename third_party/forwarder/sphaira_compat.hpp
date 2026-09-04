#pragma once

#include <switch.h>

#include <cstdint>
#include <utility>

enum {
    Module_Sphaira = 505,
};

enum class SphairaResult : Result {
    OwoBadArgs,
};

#define MAKE_SPHAIRA_RESULT_ENUM(x) Result_##x = MAKERESULT(Module_Sphaira, (Result)SphairaResult::x)

enum : Result {
    MAKE_SPHAIRA_RESULT_ENUM(OwoBadArgs),
};

#undef MAKE_SPHAIRA_RESULT_ENUM

#define R_SUCCEED() return (Result)0

#define R_THROW(_rc) return _rc

#define R_TRY(r)                          \
    {                                     \
        if (const auto _rc = (r); R_FAILED(_rc)) { \
            R_THROW(_rc);                 \
        }                                 \
    }

#define R_UNLESS(expr, res) \
    {                       \
        if (!(expr)) {      \
            R_THROW(res);   \
        }                   \
    }

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)
#define ANONYMOUS_VARIABLE(pref) CONCATENATE(pref, __COUNTER__)

template<typename Function>
struct ScopeGuard {
    explicit ScopeGuard(Function&& function) : m_function(std::forward<Function>(function)) {}
    ~ScopeGuard() { m_function(); }

    ScopeGuard(const ScopeGuard&) = delete;
    void operator=(const ScopeGuard&) = delete;

private:
    const Function m_function;
};

#define ON_SCOPE_EXIT(_f) ScopeGuard ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE_){[&] { _f; }};
