#pragma once

#include "citizenship_policy.h"
#include "continents.h"
#include "cultures.h"
#include "flags.h"
#include "governments.h"
#include "pop_types.h"
#include "reforms.h"
#include "tags.h"
#include "target_comand.h"
#include "technologies.h"
#include "war_policy.h"

COMAND(tCondition);

template <tTarget target>
tCondition<target> And(tConditions<target> && conditions)
{
    static_assert(target != tTarget::kPop, "результат этой операции может быть очень неожиданным");
    return tCondition<target>("AND", std::move(CastToComands(conditions)));
}

template <tTarget target>
tCondition<target> Or(tConditions<target> && conditions)
{
    return tCondition<target>("OR", std::move(CastToComands(conditions)));
}

template <tTarget target>
tCondition<target> Not(tCondition<target> && condition)
{
    return tCondition<target>("NOT", {static_cast<tComand &&>(condition)});
}

template <tTarget target>
tCondition<target> NotOr(tCondition<target> && conditions)
{
    return Not<target>(Or<target>(std::move(conditions)));
}

// Возвращает true, если средняя агрессивность населения в данной провинции или стране не меньше, чем num.
template <tTarget target>
tCondition<target> AverageMilitancy(float num);

template <tTarget target = tTarget::kCountry>
tCondition<target> HasGlobalFlag(char const * flag);

// Возвращает true, если для всех провинций выполнено условие condition.
tCondition<tTarget::kCountry> AllProvinces(tCondition<tTarget::kProvince> && condition);

// Возвращает true, если хотя бы для одной провинции, принадлежащей данной стране, выполняется условие condition.
tCondition<tTarget::kCountry> AnyOwnedProvince(tCondition<tTarget::kProvince> && condition);

// Возвращает true, если законом о гражданстве в данной стране является policy.
tCondition<tTarget::kCountry> CitizenshipPolicyIs(tCitizenshipPolicy policy);

// Возвращает true, если данная страна вестернизирована.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kCountry> Civilized(tYesOrNo flag = _Yes);

// Проверяет условия conditions для столицы данной страны. Возвращает true, если выполнено каждое из условий.
tCondition<tTarget::kCountry> ConditionsForCapital(tConditions<tTarget::kProvince> && conditions);

// Проверяет условия conditions для державы tag. Возвращает true, если выполнено каждое из условий.
template <tTarget target = tTarget::kCountry>
tCondition<target> ConditionsForSpecifiedCountry(tTag tag, tConditions<tTarget::kCountry> && conditions);

// Возвращает true, если держава tag существует в данный момент.
inline tCondition<tTarget::kCountry> Exists(tTag tag) { return Set("exists", ToString(tag)); }

// Возвращает true, если данная страна имеют форму правления government.
inline tCondition<tTarget::kCountry> GovernmentIs(tGovernment government)
{ return Set("government", ToString(government)); }

// Возвращает true, если данная держава значительно опережает державу tag по военным технологиям
// (если каждый военный столбик в данной стране выше, чем в стране tag).
tCondition<tTarget::kCountry> HasMuchBetterArmyTechsThan(tTag tag);

// Условие для доминионов (стран с тэгами DXX (01 <= XX <= 50)). По умолчанию их культурой является tCulture::british.
// Возвращает true, если основная культура государства совпадает с культурой по умолчанию.
tCondition<tTarget::kCountry> HasDefaultPrimaryCulture();

inline tCondition<tTarget::kCountry> HasFlag(tCountryFlag flag) { return Set("has_country_flag", ToString(flag)); }

// Возвращает true, если данная держава имеет хотя бы одного соседа, удовлетворяющего всем условиям conditions.
tCondition<tTarget::kCountry> HasNeighborSuchThat(tConditions<tTarget::kCountry> && conditions);

// Возвращает true, если в данной стране существует провинция, удовлетворяющая набору условий conditions.
tCondition<tTarget::kCountry> HasProvince(tConditions<tTarget::kProvince> && conditions);

// Возвращает true, если в данной стране изучена технология tech.
tCondition<tTarget::kCountry> HasTechnology(tTechnology const & tech);

// Возвращает true, если данная страна имеет вассала tag.
tCondition<tTarget::kCountry> HasVassal(tTag tag);

// Возвращает true, если культура culture является признанной в данной стране.
inline tCondition<tTarget::kCountry> IsAccepted(tCulture culture) { return Set("accepted_culture", ToString(culture)); }

// Возвращает true, если культура culture является признанной или основной в данной стране.
tCondition<tTarget::kCountry> IsAcceptedOrPrimary(tCulture culture);

tCondition<tTarget::kCountry> IsAI(tYesOrNo flag = _Yes);

// Возвращает true, если тэгом является DXX (01 <= XX <= 50).
tCondition<tTarget::kCountry> IsDominion();

// Возвращает true, если данная страна не является ни автономией, ни сателлитом.
tCondition<tTarget::kCountry> IsIndependant(tYesOrNo flag = _Yes);

// Возвращает true, если данная войска данной державы в настоящий момент мобилизованы.
tCondition<tTarget::kCountry> IsMobilised(tYesOrNo flag = _Yes);

// Возвращает true, если данная держава имеет общую границу с державой tag.
inline tCondition<tTarget::kCountry> IsNeighbor(tTag tag) { return Set("neighbour", ToString(tag)); }

// Возвращает true, если данная держава находится в чьей-либо сфере влияния.
tCondition<tTarget::kCountry> IsPartOfSphere(tYesOrNo flag = _Yes);

// Возвращает true, если культура culture является основной в данной стране.
inline tCondition<tTarget::kCountry> IsPrimary(tCulture culture) { return Set("primary_culture", ToString(culture)); }

// Возвращает true, если данная держава имеет не более [число бригад державы tag]/n бригад (включая мобилизованные).
inline tCondition<tTarget::kCountry> NumOfBrigadesNTimesLessThan(tTag tag, float n)
{ return tCondition<tTarget::kCountry>(ToString(tag), {Set("brigades_compare", ToString(n))}); }

// Возвращает true, если уровень реформы tReformType в данной стране равен level.
template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIs()
{
    return Set(std::string(nsReforms::ToString<tReformType>()), nsReforms::ToString<tReformType, level>());
}

// Возвращает true, если уровень реформы tReformType в данной стране меньше, чем level.
template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIsLessThan();

// Возвращает true, если уровень реформы tReformType в данной стране больше или равен level.
template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIsGreaterOrEaqualThan();

// Возвращает true, если отношения между данной державой и державой tag не меньше, чем value.
tCondition<tTarget::kCountry> RelationWithCountryIsHigherOrEqual(tTag tag, int value);

inline tCondition<tTarget::kCountry> TagIs(tTag tag) { return Set("tag", ToString(tag)); }

// Возвращает true, если военной политикой данной державы является policy.
inline tCondition<tTarget::kCountry> WarPolicyIs(tWarPolicy policy) { return Set("war_policy", ToString(policy)); }

// Возвращает true, если данная страна находится в состоянии войны.
tCondition<tTarget::kCountry> War(tYesOrNo flag = _Yes);

// Возвращает true, если данная страна находится в состоянии войны с державой tag.
inline tCondition<tTarget::kCountry> WarWith(tTag tag) { return Set("war_with", ToString(tag)); }

// Проверяет условия conditions для владельца данной провинции. Возвращает true, если выполнено каждое из условий.
tCondition<tTarget::kProvince> ConditionsForOwner(tConditions<tTarget::kCountry> && conditions);

// Возвращает true, если данная провинция в данный момент контролируется державой tag.
inline tCondition<tTarget::kProvince> ControlledBy(tTag tag) { return Set("controlled_by", ToString(tag)); }

// Возвращает true, если данная провинция оккупирована повстанцами.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kProvince> ControlledByRebels(tYesOrNo flag = _Yes);

// Возвращает true, если данная провинция не принадлежит никому (то есть ещё ни кем не колонизированной).
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kProvince> Empty(tYesOrNo flag = _Yes);

// Возвращает true, если в данной провинции существует поп с агрессивностью не менее, чем num.
tCondition<tTarget::kProvince> ExistsPopWithMilitancy(float num);

// Возвращает true, если среди соседей данной провинции существует хотя бы одна НЕПУСТАЯ провинция, удовлетворяющая
// набору условий conditions.
tCondition<tTarget::kProvince> HasNeighborProvince(tConditions<tTarget::kProvince> && conditions);

// Возвращает true, если в данной провинции проживает поп, удовлетворяющий условию condition.
tCondition<tTarget::kProvince> HasPop(tCondition<tTarget::kPop> && condition);

// Возвращает true, если данная провинция находится на континенте continent.
inline tCondition<tTarget::kProvince> IsInContinent(tContinent continent) { return Set("continent", ToString(continent)); }

// Возвращает true, если данная провинция находится в Америке или Океании.
tCondition<tTarget::kProvince> IsInNewWorld();

// Возвращает true, если данная провинция является национальной для державы tag.
inline tCondition<tTarget::kProvince> IsNationalFor(tTag tag)
{
    return Set("is_core", ToString(tag));
}

// Возвращает true, если culture является самой многочисленной культурой данной провинции.
inline tCondition<tTarget::kProvince> MajorityCultureIs(tCulture culture) { return Set("culture", ToString(culture)); }

// Возвращает true, если наиболее многочисленная культура в данной провинции является признанной или основной.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kProvince> MajorityCultureIsAccepted(tYesOrNo flag = _Yes);

// Возвращает true, если наиболее многочисленная культура в данной провинции является основной.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kProvince> MajorityCultureIsPrimary(tYesOrNo flag = _Yes);

// Возвращает true, если в количество населения в данной провинции не меньше, чем num.
tCondition<tTarget::kProvince> PopulationIsGreaterOrEqual(uint num);

// Возвращает true, если культурой попа является culture.
inline tCondition<tTarget::kPop> HasCulture(tCulture culture)
{
    return Set("has_pop_culture", ToString(culture));
}

// Возвращает true, если поп имеет тип type.
inline tCondition<tTarget::kPop> HasType(tPopType type)
{
    return Set("type", ToString(type));
}

// Возвращает true, если культура попа является признанной культурой страны, в которой он проживает.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kPop> HasAcceptedCulture(tYesOrNo flag = _Yes);

// Возвращает true, если культура попа совпадает с основной культурой страны, в которой он проживает.
// Если flag == _No, то возвращаемое значение инвертируется.
tCondition<tTarget::kPop> HasPrimaryCulture(tYesOrNo flag = _Yes);

template <class tReformType, uint16_t from, uint16_t to>
void AddConditions(tConditions<tTarget::kCountry> & where)
{
    if constexpr (from == to)
        return;
    else
    {
        static_assert(from != to);
        where.push_back(ReformLevelIs<tReformType, static_cast<tReformType>(from)>());
        AddConditions<tReformType, from + 1u, to>(where);
    }
}

template <tTarget target>
tCondition<target> ConditionsForSpecifiedCountry(tTag const tag, tConditions<tTarget::kCountry> && conditions)
{
    if constexpr (target == tTarget::kCountry)
        return tCondition<target>(ToString(tag), std::move(CastToComands(conditions)));
    else
        return tCondition<target>(ConditionsForOwner({ConditionsForSpecifiedCountry(tag, std::move(conditions))}));
}

inline tCondition<tTarget::kCountry> HasMuchBetterArmyTechsThan(tTag tag)
{
    tConditions<tTarget::kCountry> conditions;
    for (int i = 0; i < 5; ++i)
    {
        tConditions<tTarget::kCountry> subconditions;
        for (int j = 0; j < 6; ++j)
        {
            tTechnology const & tech = ArmyTech()[i][j];
            subconditions.push_back
            (
                And<tTarget::kCountry>
                ({
                    HasTechnology(tech),
                    ConditionsForSpecifiedCountry(tag, {Not(HasTechnology(tech))})
                })
            );
        }
        conditions.push_back(Or<tTarget::kCountry>(std::move(subconditions)));
    }
    return And<tTarget::kCountry>(std::move(conditions));
}

inline tCondition<tTarget::kCountry> IsAcceptedOrPrimary(tCulture culture)
{
    return Or<tTarget::kCountry>({IsAccepted(culture), IsPrimary(culture)});
}

inline tCondition<tTarget::kCountry> RelationWithCountryIsHigherOrEqual(tTag tag, int const value)
{
    assertion(-200 < value && value <= 200);
    return tCondition<tTarget::kCountry>("relation",
                                         {Set("who", ToString(tag)), Set("value", std::to_string(value))});
}

template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIsLessThan()
{
    static_assert(static_cast<uint16_t>(level));
    if constexpr (static_cast<uint16_t>(level) > static_cast<uint16_t>(tReformType::END) - static_cast<uint16_t>(level))
        return Not(ReformLevelIsGreaterOrEaqualThan<tReformType, level>());
    else
    {
        tConditions<tTarget::kCountry> conditions;
        AddConditions<tReformType, 0u, static_cast<uint16_t>(level)>(conditions);
        return Or<tTarget::kCountry>(std::move(conditions));
    }
}

template <class tReformType, tReformType level>
tCondition<tTarget::kCountry> ReformLevelIsGreaterOrEaqualThan()
{
    static_assert(static_cast<uint16_t>(level) < static_cast<uint16_t>(tReformType::END) - 1u);
    if constexpr (static_cast<uint16_t>(level) <= static_cast<uint16_t>(tReformType::END) - static_cast<uint16_t>(level))
        return Not(ReformLevelIsLessThan<tReformType, level>());
    else
    {
        tConditions<tTarget::kCountry> conditions;
        AddConditions<tReformType, static_cast<uint16_t>(level), static_cast<uint16_t>(tReformType::END)>(conditions);
        return Or<tTarget::kCountry>(std::move(conditions));
    }
}
