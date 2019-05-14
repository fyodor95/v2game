#pragma once

namespace nsReforms
{

    template <class tReformType>
    char const * ConstructReformTypeName()
    {
        static std::string res = typeid(tReformType).name();
        return res.data() + (res.rfind(':') + 1u);
    }

    template <class tReformType>
    char const * ToString()
    {
        static char const * result = ConstructReformTypeName<tReformType>();
        return result;
    }

    template <class tReformType, tReformType>
    char const * ToString();

    enum class tType : uint8_t
    {
          social
        , political
    };

    namespace nsType
    {
        template <tType>
        char const * ToString();
    }

    template <class tReformType>
    constexpr tType Type();

    enum class wage_reform : uint8_t
    {
          no
        , trinket
        , low
        , acceptable
        , good
        , END
    };

    enum class work_hours : uint8_t
    {
          no
        , fourteen
        , twelve
        , ten
        , eight
        , END
    };

    enum class safety_regulations : uint8_t
    {
          no
        , trinket
        , low
        , acceptable
        , good
        , END
    };

    enum class unemployment_subsidies : uint8_t
    {
          no
        , trinket
        , low
        , acceptable
        , good
        , END
    };

    enum class pensions : uint8_t
    {
          no
        , trinket
        , low
        , acceptable
        , good
        , END
    };

    enum class health_care : uint8_t
    {
          no
        , trinket
        , low
        , acceptable
        , good
        , END
    };

    enum class school_reforms : uint8_t
    {
          no
        , low
        , acceptable
        , good
        , END
    };

    enum class slavery : uint8_t
    {
          yes
        , no
        , END
    };

    enum class vote_franschise : uint8_t
    {
          none
        , landed
        , wealth_weighted
        , wealth
        , universal_weighted
        , universal
        , END
    };

    enum class upper_house_composition : uint8_t
    {
          party_appointed
        , appointed
        , state_equal_weight
        , population_equal_weight
        , END
    };

    enum class voting_system : uint8_t
    {
          first_past_the_post
        , jefferson_method
        , proportional_representation
        , END
    };

    enum class public_meetings : uint8_t
    {
          no
        , yes
        , END
    };

    enum class press_rights : uint8_t
    {
          state
        , censored
        , free
        , END
    };

    enum class trade_unions : uint8_t
    {
          no_trade_unions
        , state_controlled
        , non_socialist
        , all_trade_unions
        , END
    };

    enum class political_parties : uint8_t
    {
          underground_parties
        , harassment
        , gerrymandering
        , non_secret_ballots
        , secret_ballots
        , END
    };

    template <class tReformType>
    constexpr tType Type()
    {
        if constexpr (   std::is_same_v<tReformType, wage_reform            >
                      || std::is_same_v<tReformType, work_hours             >
                      || std::is_same_v<tReformType, safety_regulations     >
                      || std::is_same_v<tReformType, unemployment_subsidies >
                      || std::is_same_v<tReformType, pensions               >
                      || std::is_same_v<tReformType, health_care            >
                      || std::is_same_v<tReformType, school_reforms         >)
            return tType::social;
        else
            return tType::political;
    }

}
