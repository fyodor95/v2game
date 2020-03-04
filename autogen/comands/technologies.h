#pragma once

class tTechnology;

using tTechArea = std::array<std::array<tTechnology, 6u>, 5u>;

class tTechnology
{
public:
    std::string const & ToString() const;

private:
    std::string xName;

    friend tTechArea ConstructArmyTechArea();
};

tTechArea const & ArmyTech();
