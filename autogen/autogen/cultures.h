#pragma once

struct tCulture
{
    tCulture(char const * name, char const * localization);

    char const * const Name;
    char const * const Localization;
};

DEFINE_CONTAINER(tCulture);

// Возвращает контейнер из всех существующих культур.
tCultures const & Cultures();
