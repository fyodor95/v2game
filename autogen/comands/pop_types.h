#pragma once

enum class tPopType : uint16_t
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

char const * ToString(tPopType);
