#pragma once

enum class tEventPicture
{
    NONE,
    Administration
};

namespace nsEventPicture
{
    template <tEventPicture>
    char const * ToString();
}
