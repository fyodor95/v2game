#pragma once

enum class tTarget : uint8_t
{
    kCountry,
    kProvince,
    kPop
};

constexpr bool TargetIsCountry (tTarget target) { return target == tTarget::kCountry ; }
constexpr bool TargetIsProvince(tTarget target) { return target == tTarget::kProvince; }
