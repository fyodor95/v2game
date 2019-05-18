#pragma once

enum class tPopType
{
    aristocrats, // аристократы
    artisans,    // кустари
    bureaucrats, // чиновники
    capitalists, // капиталисты
    clergymen,   // священники
    clerks,      // квалифицированные рабочие
    craftsmen,   // рабочие
    farmers,     // крестьяне
    labourers,   // шахтёры
    officers,    // офицеры
    slaves,      // рабы
    soldiers     // солдаты
};

namespace nsPopType
{
    template <tPopType>
    char const * ToString();
}
