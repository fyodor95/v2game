#include "technologies.h"

std::string const & tTechnology::ToString() const
{
    return xName;
}

tTechArea ConstructArmyTechArea()
{
    tTechArea res;

    res[0][0].xName = "post_napoleonic_thought";
    res[0][1].xName = "strategic_mobility";
    res[0][2].xName = "point_defense_system";
    res[0][3].xName = "deep_defense_system";
    res[0][4].xName = "infiltration";
    res[0][5].xName = "modern_army_doctrine";

    res[1][0].xName = "flintlock_rifles";
    res[1][1].xName = "muzzle_loaded_rifles";
    res[1][2].xName = "breech_loaded_rifles";
    res[1][3].xName = "machine_guns";
    res[1][4].xName = "bolt_action_rifles";
    res[1][5].xName = "modern_divisional_structure";

    res[2][0].xName = "bronze_muzzle_loaded_artillery";
    res[2][1].xName = "iron_muzzle_loaded_artillery";
    res[2][2].xName = "iron_breech_loaded_artillery";
    res[2][3].xName = "steel_breech_loaded_artillery";
    res[2][4].xName = "indirect_artillery_fire";
    res[2][5].xName = "heavy_armament_dunno";

    res[3][0].xName = "military_staff_system";
    res[3][1].xName = "military_plans";
    res[3][2].xName = "military_statistics";
    res[3][3].xName = "military_logistics";
    res[3][4].xName = "military_directionism";
    res[3][5].xName = "aeronautics";

    res[4][0].xName = "army_command_principle";
    res[4][1].xName = "army_professionalism";
    res[4][2].xName = "army_decision_making";
    res[4][3].xName = "army_risk_management";
    res[4][4].xName = "army_nco_training";
    res[4][5].xName = "great_war_experience";

    return res;
}

SINGLETON(ArmyTech, tTechArea)
{
    return ConstructArmyTechArea();
}
