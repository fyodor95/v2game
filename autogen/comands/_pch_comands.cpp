#include "_pch_comands_private.h"

void DynamicAssertion(bool const expr, char const * const file,
                      int  const line, char const * const exprStr)
{
    if (expr)
        return;
    char buffer[600];
    sprintf(buffer, "%s(%d): assertion(%s) failed", file, line, exprStr);
    std::cerr << buffer;
    throw std::runtime_error(buffer);
}

std::string ToString(float const num)
{
    std::string res = std::to_string(num);
    assertion(res.find('.') != res.npos);
    size_t removeSuffixFrom = res.size();
    size_t lastNormal = removeSuffixFrom - 1;
    for (; res[lastNormal] == '0'; removeSuffixFrom = lastNormal--);
    res.erase(res[lastNormal] == '.' ? lastNormal : removeSuffixFrom);
    return res;
}

char const * YesOrNo(tYesOrNo const yesOrNo)
{
    return yesOrNo == _Yes ? "yes" : "no";
}

char const * LocLineEnd()
{
    return ";X\n";
}
