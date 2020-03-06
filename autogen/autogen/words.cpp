#include "events.h"
#include "words.h"

static UNCONST_SINGLETON(Input, std::ifstream)
{
    return std::ifstream("autogen/words.txt");
}

static bool Get(std::string & s)
{
    return !!std::getline(Input(), s);
}

static std::string Get()
{
    std::string res;
    bool const status = ::Get(res);
    assertion(status);
    return res;
}

static std::vector<std::string> GetVector()
{
    std::vector<std::string> res;
    do
    {
        res.push_back(Get());
    }
    while (res.back() != "####");
    res.pop_back();
    return res;
}

static std::string operator+(std::string const & str, std::vector<std::string> const & vec)
{
    std::string res = str;
    for (std::string const & elem : vec)
        (res += elem) += "\\n";
    return res;
}

using tOptDescStorage = std::array<std::string, 12u>;

static SINGLETON(OptDescStorage, tOptDescStorage)
{
    tOptDescStorage res;
    uint v = 1;
    for (std::string & desc : res)
    {
        v *= 2;
        desc = "Show it once in ";
        desc += std::to_string(v);
        desc += " months";
    }
    return res;
}

void GenerateWordEvents()
{
    std::ifstream input("autogen/words.txt");

    std::ofstream        eventOutput      ("events\\words.txt");
    std::ofstream localizationOutput("localisation\\words.csv");

    uint id = 50000u;
    std::string s;
    while (Get(s))
    {
        int64_t const wordID = atoll(s.data());
        std::string const word = Get();
        std::string const translation = Get();
        std::string const transcription = Get();
        std::vector<std::string> const explanation = GetVector();
        std::vector<std::string> const examples = GetVector();
        std::string const desc = std::string() + explanation + examples;
        float months = 1.f;
        std::array<std::string, 11u> flags;
        for (int i = 0; i < 11; ++i)
            flags[i] = "eng_words_" + std::to_string(wordID) + '_' + std::to_string(i);
        for (uint i = 0u; i < 12u; ++i)
        {
            ++id;
            months *= 2;
            tConditions<tTarget::kCountry> conditions{IsAI(_No)};
            std::vector<tEffects<tTarget::kCountry>> effects;
            if (i)
            {
                char const * const flag = flags[i - 1u].data();
                conditions.push_back(HasGlobalFlag(flag));
                effects.push_back({RemoveGlobalFlag(flag)});
            }
            effects.emplace_back();
            if (i != 11u)
            {
                char const * const flag = flags[i].data();
                conditions.push_back(Not(HasGlobalFlag(flag)));
                effects.push_back({SetGlobalFlag(flag)});
            }
            tOptions<tTarget::kCountry> options;
            tOptionDescriptions optDescs;
            options.reserve(effects.size());
            for (uint j = 0u; j < effects.size(); ++j)
            {
                options.push_back(Option(std::move(effects[j]), id, j + 1u));
                optDescs.push_back(OptDescStorage()[i + j - 1u + !i].data());
            }
            std::string const picture = "eng_words_" + std::to_string(wordID);
            CountryEvent(id, "It's time to learn words, my addict", desc.data(), eventOutput, localizationOutput,
                         Trigger          <tTarget::kCountry>(std::move(conditions)),
                         MeanTimeToHappen <tTarget::kCountry>(months),
                         std::move(options), optDescs, picture.data());
        }
    }
}
