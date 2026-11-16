// Диагностический протокол заранее зафиксирован в docs/plans/gpc-distribution.md.
// Настоящий scheduler, фиксированные seeds/N/допуски, никакого доступа к игре.
#include "scheduler.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr std::uint32_t kBound = 1000000;
constexpr std::uint32_t kThreshold = 555;
constexpr std::uint32_t kAttempts = 1000000;
constexpr std::uint32_t kDays = 2 * kAttempts;
constexpr std::uint32_t kMirrorAttempts = 32;
constexpr std::uint32_t kRestrictedHorizon = 6000;
constexpr double kProbability = 555.0 / 1000000.0;
constexpr double kZLimit = 8.0;
constexpr double kCdfLimit = 0.04;
constexpr double kRestrictedRelativeLimit = 0.10;
constexpr std::array<std::uint32_t, 32> kSeeds{
    0x00000001,0x00000002,0x00000003,0x00000007,0x0000000B,0x0000001F,0x00000100,0x00010000,
    0x01000000,0x10000000,0x80000000,0xFFFFFFFF,0x12345678,0x87654321,0xDEADBEEF,0xCAFEBABE,
    0x9E3779B9,0x6D2B79F5,0xA5A5A5A5,0x5A5A5A5A,0x13579BDF,0x2468ACE0,0x31415926,0x27182818,
    0x02475043,0x00475043,0x4C6FBF7B,0xF2B58529,0x0000FFFF,0xFFFF0000,0x7FFFFFFF,0xC001D00D
};
constexpr std::array<std::uint32_t, 10> kCdfGrid{1,10,100,500,1000,1250,1800,3600,6000,12000};
static_assert(gpc::kScheduleChanceScale == kBound, "Production bound changed");
static_assert(gpc::kScheduleMtth60Threshold == kThreshold, "Production threshold changed");
static_assert(kDays <= gpc::kScheduleMaximumDay, "Protocol exceeds native date range");

void Require(bool condition, const char* name) {
    if (!condition) throw std::runtime_error(name);
}

std::string Quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << char(c);
        else if (c < 32) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(c);
        else out << char(c);
    }
    out << '"';
    return out.str();
}

std::string Hex(std::uint32_t value) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

const char* Boolean(bool value) { return value ? "true" : "false"; }

double ExpectedEvents(std::uint64_t attempts) { return double(attempts) * kProbability; }
double EventZ(std::uint64_t events, std::uint64_t attempts) {
    return (double(events) - ExpectedEvents(attempts)) /
        std::sqrt(double(attempts) * kProbability * (1.0 - kProbability));
}
double TheoreticalCdf(std::uint32_t attempts) {
    return -std::expm1(double(attempts) * std::log1p(-kProbability));
}

struct Bucket { std::uint64_t events = 0, censored = 0; };
using Histogram = std::map<std::uint32_t, Bucket>;

struct Counts {
    std::uint32_t seed = 0, final_rng = 0, final_pass = 0, first_success_attempt = 0;
    std::uint64_t attempts_odd = 0, attempts_even = 0, events_odd = 0, events_even = 0;
    std::uint64_t draws_odd = 0, draws_even = 0, max_draws_per_attempt = 0;
    std::uint64_t completed_intervals = 0, completed_wait_sum = 0;
    std::uint32_t censored_tail_attempts = 0;
    std::uint64_t mirror_draws = 0;
};

Counts Sample(std::uint32_t seed, Histogram& histogram) {
    Counts counts;
    counts.seed = seed;
    auto state = gpc::InitializeSchedule(seed, 0);
    std::uint32_t mirror_rng = seed, last_success_attempt = 0;
    for (std::uint32_t day = 1; day <= kDays; ++day) {
        const auto tick = gpc::TickSchedule(state, day, true, kThreshold);
        Require(tick.next.schema == state.schema && tick.next.seed == seed && tick.next.rng != 0,
                "state_identity_changed");
        Require(tick.next.last_day == day, "last_day_mismatch");
        Require(tick.next.pass_sequence == state.pass_sequence + (tick.due ? 1u : 0u),
                "pass_increment_mismatch");
        if (day & 1u) {
            Require(tick.attempted && tick.draws >= 1, "odd_day_without_attempt");
            ++counts.attempts_odd;
            counts.events_odd += tick.due ? 1u : 0u;
            counts.draws_odd += tick.draws;
            if (tick.draws > counts.max_draws_per_attempt) counts.max_draws_per_attempt = tick.draws;
            if (counts.attempts_odd <= kMirrorAttempts) {
                const auto direct = gpc::DrawNonzeroBounded([&]() {
                    mirror_rng = gpc::XorShift32(mirror_rng);
                    return mirror_rng;
                }, kBound);
                counts.mirror_draws += direct.draws;
                Require(direct.value < kBound && direct.draws == tick.draws &&
                        mirror_rng == tick.next.rng && (direct.value < kThreshold) == tick.due,
                        "direct_bounded_draw_mismatch");
            }
            if (tick.due) {
                const auto attempt = static_cast<std::uint32_t>(counts.attempts_odd);
                const auto duration = attempt - last_success_attempt;
                Require(duration > 0, "zero_waiting_interval");
                ++histogram[duration].events;
                ++counts.completed_intervals;
                counts.completed_wait_sum += duration;
                if (!counts.first_success_attempt) counts.first_success_attempt = attempt;
                last_success_attempt = attempt;
            }
        } else {
            counts.attempts_even += tick.attempted ? 1u : 0u;
            counts.events_even += tick.due ? 1u : 0u;
            counts.draws_even += tick.draws;
            Require(!tick.attempted && !tick.due && tick.draws == 0 &&
                    tick.next.rng == state.rng && tick.next.pass_sequence == state.pass_sequence,
                    "even_day_changed_random_state");
        }
        state = tick.next;
    }
    Require(counts.attempts_odd == kAttempts && counts.draws_odd >= kAttempts,
            "attempt_or_raw_draw_count_mismatch");
    counts.censored_tail_attempts = kAttempts - last_success_attempt;
    if (counts.censored_tail_attempts) ++histogram[counts.censored_tail_attempts].censored;
    Require(counts.completed_wait_sum + counts.censored_tail_attempts == kAttempts,
            "waiting_exposure_mismatch");
    counts.final_rng = state.rng;
    counts.final_pass = state.pass_sequence;
    Require(counts.final_pass == counts.events_odd && state.last_day == kDays,
            "final_state_mismatch");
    return counts;
}

struct Check { std::string name; bool passed; };
struct Checks {
    std::vector<Check> items;
    void Add(std::string name, bool passed) { items.push_back({std::move(name),passed}); }
    std::size_t Passed() const {
        std::size_t count = 0;
        for (const auto& check : items) if (check.passed) ++count;
        return count;
    }
    bool Ok() const { return Passed() == items.size(); }
};

struct CdfPoint {
    std::uint32_t attempts;
    double expected, empirical;
    bool identifiable;
    std::uint64_t at_risk_before, at_risk_after, events_through, censored_through;
};
struct Waiting {
    std::vector<CdfPoint> cdf;
    double restricted_mean = 0;
    bool restricted_identifiable = true;
};

Waiting KaplanMeier(const Histogram& histogram) {
    Waiting result;
    std::uint64_t risk = 0, events = 0, censored = 0;
    for (const auto& item : histogram) risk += item.second.events + item.second.censored;
    Require(risk > 0, "empty_waiting_risk_set");
    double survival = 1.0;
    bool identifiable = true;
    std::size_t next_grid = 0;
    for (std::uint32_t t = 0; t <= kCdfGrid.back(); ++t) {
        const auto before = risk;
        if (t) {
            // Последний цензурированный интервал наблюдался ВКЛЮЧАЯ свою
            // duration: S(t) на этой границе ещё определена, S(t+1) уже нет.
            if (!risk && survival > 0.0) identifiable = false;
            const auto at = histogram.find(t);
            if (at != histogram.end()) {
                const auto& bucket = at->second;
                Require(bucket.events + bucket.censored <= risk && risk != 0,
                        "invalid_waiting_risk_set");
                // При равной duration событие предшествует цензурированию.
                survival *= 1.0 - double(bucket.events) / double(risk);
                risk -= bucket.events + bucket.censored;
                events += bucket.events;
                censored += bucket.censored;
            }
        }
        if (t < kRestrictedHorizon) {
            if (!identifiable) result.restricted_identifiable = false;
            result.restricted_mean += survival;
        }
        if (next_grid < kCdfGrid.size() && t == kCdfGrid[next_grid]) {
            result.cdf.push_back({t,TheoreticalCdf(t),1.0-survival,identifiable,
                                  before,risk,events,censored});
            ++next_grid;
        }
    }
    return result;
}

void CheckWaitingEstimator(Checks& checks) {
    const auto tied = KaplanMeier(Histogram{{1,{1,1}},{10,{1,0}}});
    Require(tied.cdf.front().identifiable && tied.cdf.front().at_risk_before == 3 &&
            tied.cdf.front().at_risk_after == 1 &&
            std::abs(tied.cdf.front().empirical-1.0/3.0) < 1e-12 &&
            tied.restricted_identifiable && std::abs(tied.restricted_mean-7.0) < 1e-12,
            "km_tied_event_censor_selfcheck");
    checks.Add("km_tied_event_censor_selfcheck",true);
    const auto censored = KaplanMeier(Histogram{{1,{0,1}}});
    Require(censored.cdf.front().identifiable && censored.cdf.front().empirical == 0.0 &&
            !censored.cdf[1].identifiable && !censored.restricted_identifiable,
            "km_censored_support_selfcheck");
    checks.Add("km_censored_support_selfcheck",true);
    const auto complete = KaplanMeier(Histogram{{2,{1,0}},{4,{1,0}}});
    Require(complete.restricted_identifiable && std::abs(complete.restricted_mean-3.0) < 1e-12 &&
            complete.cdf.front().empirical == 0.0 && complete.cdf[1].empirical == 1.0,
            "km_complete_intervals_selfcheck");
    checks.Add("km_complete_intervals_selfcheck",true);
    const auto boundary = KaplanMeier(Histogram{{kRestrictedHorizon,{0,1}}});
    Require(boundary.restricted_identifiable && boundary.restricted_mean == kRestrictedHorizon &&
            boundary.cdf[8].identifiable && !boundary.cdf[9].identifiable,
            "km_last_observed_boundary_selfcheck");
    checks.Add("km_last_observed_boundary_selfcheck",true);
}

void NumberOrNull(std::ostream& out, bool defined, double value) {
    if (defined && std::isfinite(value)) out << value;
    else out << "null";
}

std::string Run(bool& ok) {
    Histogram histogram;
    Checks checks;
    CheckWaitingEstimator(checks);
    std::vector<Counts> seeds;
    std::uint64_t attempts = 0, events = 0, raw_draws = 0, censored_count = 0, censored_exposure = 0;
    for (auto seed : kSeeds) {
        auto counts = Sample(seed,histogram);
        attempts += counts.attempts_odd;
        events += counts.events_odd;
        raw_draws += counts.draws_odd;
        censored_count += counts.censored_tail_attempts != 0 ? 1u : 0u;
        censored_exposure += counts.censored_tail_attempts;
        checks.Add("seed_"+Hex(seed)+"_daily_and_mirror_contracts",true);
        checks.Add("seed_"+Hex(seed)+"_event_frequency",
                   std::abs(EventZ(counts.events_odd,counts.attempts_odd)) <= kZLimit);
        seeds.push_back(counts);
    }
    constexpr std::uint64_t domain = 4294967295ull;
    constexpr std::uint64_t limit = (domain / kBound) * kBound;
    constexpr double reject_probability = double(domain-limit) / double(domain);
    const double expected_rejections = double(attempts)*reject_probability/(1.0-reject_probability);
    const double rejection_variance = double(attempts)*reject_probability/
        ((1.0-reject_probability)*(1.0-reject_probability));
    const auto rejections = raw_draws-attempts;
    const double rejection_z = (double(rejections)-expected_rejections)/std::sqrt(rejection_variance);
    checks.Add("aggregate_event_frequency",std::abs(EventZ(events,attempts)) <= kZLimit);
    checks.Add("aggregate_rejection_frequency",std::abs(rejection_z) <= kZLimit);
    checks.Add("aggregate_waiting_exposure",[&]() {
        std::uint64_t completed = 0;
        for (const auto& seed : seeds) completed += seed.completed_wait_sum;
        return completed+censored_exposure == attempts;
    }());

    const auto waiting = KaplanMeier(histogram);
    bool complete = waiting.restricted_identifiable;
    for (const auto& point : waiting.cdf) {
        complete = complete && point.identifiable;
        checks.Add("waiting_cdf_"+std::to_string(point.attempts),point.identifiable &&
                   std::abs(point.empirical-point.expected) <= kCdfLimit);
    }
    const double expected_restricted_mean = TheoreticalCdf(kRestrictedHorizon)/kProbability;
    const double restricted_relative_error = waiting.restricted_mean/expected_restricted_mean-1.0;
    checks.Add("restricted_mean_identifiable",waiting.restricted_identifiable);
    checks.Add("restricted_mean_relative_error",waiting.restricted_identifiable &&
               std::abs(restricted_relative_error) <= kRestrictedRelativeLimit);
    ok = complete && checks.Ok();

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(15) << "{\"schema\":1,\"complete\":" << Boolean(complete)
        << ",\"ok\":" << Boolean(ok) << ",\"diagnostic_only\":true,\"protocol\":{"
        << "\"seed_count\":" << kSeeds.size() << ",\"days_per_seed\":" << kDays
        << ",\"attempts_per_seed\":" << kAttempts << ",\"threshold\":" << kThreshold
        << ",\"bound\":" << kBound << ",\"z_limit\":" << kZLimit
        << ",\"cdf_absolute_limit\":" << kCdfLimit
        << ",\"restricted_relative_limit\":" << kRestrictedRelativeLimit
        << ",\"restricted_horizon_attempts\":" << kRestrictedHorizon
        << ",\"mirror_attempts_per_seed\":" << kMirrorAttempts << ",\"seeds_hex\":[";
    for (std::size_t n=0;n<kSeeds.size();++n) { if(n) out << ','; out << Quote(Hex(kSeeds[n])); }
    out << "]},\"theory\":{\"probability_per_attempt\":" << kProbability
        << ",\"event_rate_per_calendar_day\":" << kProbability/2.0
        << ",\"mean_waiting_attempts\":" << 1.0/kProbability
        << ",\"mean_renewal_waiting_days\":" << 2.0/kProbability
        << ",\"mean_first_waiting_days\":" << 2.0/kProbability-1.0
        << ",\"raw_domain\":" << domain << ",\"acceptance_limit\":" << limit
        << ",\"rejection_probability\":" << reject_probability
        << ",\"restricted_mean_attempts\":" << expected_restricted_mean
        << ",\"restricted_mean_renewal_days\":" << 2.0*expected_restricted_mean
        << "},\"seeds\":[";
    for (std::size_t n=0;n<seeds.size();++n) {
        const auto& s=seeds[n];
        if(n) out << ',';
        out << "{\"seed\":" << s.seed << ",\"seed_hex\":" << Quote(Hex(s.seed))
            << ",\"days\":" << kDays << ",\"attempted_odd\":" << s.attempts_odd
            << ",\"attempted_even\":" << s.attempts_even << ",\"due_odd\":" << s.events_odd
            << ",\"due_even\":" << s.events_even << ",\"raw_draws_odd\":" << s.draws_odd
            << ",\"raw_draws_even\":" << s.draws_even
            << ",\"rejections\":" << s.draws_odd-s.attempts_odd
            << ",\"max_draws_per_attempt\":" << s.max_draws_per_attempt
            << ",\"event_count_expected\":" << ExpectedEvents(s.attempts_odd)
            << ",\"event_count_z\":" << EventZ(s.events_odd,s.attempts_odd)
            << ",\"completed_intervals\":" << s.completed_intervals
            << ",\"completed_wait_sum_attempts\":" << s.completed_wait_sum
            << ",\"right_censored_intervals\":" << (s.censored_tail_attempts ? 1 : 0)
            << ",\"right_censored_attempts\":" << s.censored_tail_attempts
            << ",\"first_success_day\":";
        if(s.first_success_attempt) out << 2u*s.first_success_attempt-1u; else out << "null";
        out << ",\"final_rng\":" << s.final_rng << ",\"final_pass_sequence\":" << s.final_pass
            << ",\"mirror_raw_draws_excluded\":" << s.mirror_draws << '}';
    }
    out << "],\"aggregate\":{\"calendar_days\":" << std::uint64_t(kDays)*kSeeds.size()
        << ",\"attempts\":" << attempts << ",\"events\":" << events
        << ",\"event_count_expected\":" << ExpectedEvents(attempts)
        << ",\"event_count_z\":" << EventZ(events,attempts)
        << ",\"probability_per_attempt_empirical\":" << double(events)/double(attempts)
        << ",\"event_rate_per_calendar_day_empirical\":" << double(events)/(2.0*double(attempts))
        << ",\"raw_draws\":" << raw_draws
        << ",\"raw_draws_expected\":" << double(attempts)/(1.0-reject_probability)
        << ",\"rejections\":" << rejections << ",\"rejections_expected\":" << expected_rejections
        << ",\"rejections_z\":" << rejection_z << ",\"completed_intervals\":" << events
        << ",\"right_censored_intervals\":" << censored_count
        << ",\"right_censored_attempts\":" << censored_exposure
        << ",\"parametric_mean_attempts_from_all_exposure\":";
    NumberOrNull(out,events!=0,double(attempts)/double(events ? events : 1));
    out << ",\"parametric_mean_renewal_days_from_all_exposure\":";
    NumberOrNull(out,events!=0,2.0*double(attempts)/double(events ? events : 1));
    out << ",\"restricted_mean_attempts_km\":";
    NumberOrNull(out,waiting.restricted_identifiable,waiting.restricted_mean);
    out << ",\"restricted_mean_renewal_days_km\":";
    NumberOrNull(out,waiting.restricted_identifiable,2.0*waiting.restricted_mean);
    out << ",\"restricted_mean_relative_error\":";
    NumberOrNull(out,waiting.restricted_identifiable,restricted_relative_error);
    out << "},\"waiting_cdf_km\":[";
    for (std::size_t n=0;n<waiting.cdf.size();++n) {
        const auto& p=waiting.cdf[n];
        if(n) out << ',';
        out << "{\"attempts\":" << p.attempts << ",\"renewal_days\":" << 2u*p.attempts
            << ",\"first_wait_days\":" << 2u*p.attempts-1u
            << ",\"identifiable\":" << Boolean(p.identifiable) << ",\"expected\":" << p.expected
            << ",\"empirical\":";
        NumberOrNull(out,p.identifiable,p.empirical);
        out << ",\"absolute_error\":";
        NumberOrNull(out,p.identifiable,std::abs(p.empirical-p.expected));
        out << ",\"at_risk_before\":" << p.at_risk_before << ",\"at_risk_after\":" << p.at_risk_after
            << ",\"events_through\":" << p.events_through << ",\"censored_through\":" << p.censored_through << '}';
    }
    out << "],\"checks\":{\"total\":" << checks.items.size() << ",\"passed\":" << checks.Passed()
        << ",\"failed\":" << checks.items.size()-checks.Passed() << ",\"conditions\":[";
    for (std::size_t n=0;n<checks.items.size();++n) {
        if(n) out << ',';
        out << "{\"name\":" << Quote(checks.items[n].name)
            << ",\"passed\":" << Boolean(checks.items[n].passed) << '}';
    }
    out << "]}}\n";
    return out.str();
}
} // namespace

int main(int argc, char**) {
    std::cout.imbue(std::locale::classic());
    try {
        Require(argc == 1,"protocol_has_no_runtime_parameters");
        bool ok = false;
        const auto report = Run(ok);
        std::cout << report;
        return std::cout.good() && ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cout << "{\"schema\":1,\"complete\":false,\"ok\":false,\"error\":"
                  << Quote(error.what()) << ",\"checks\":{\"total\":1,\"passed\":0,\"failed\":1,"
                     "\"conditions\":[{\"name\":\"structural_contract_or_exception\",\"passed\":false}]}}\n";
        return 1;
    }
}
