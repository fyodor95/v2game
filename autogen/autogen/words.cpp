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

struct tWord
{
    tWord(std::string const & s) : ID(atoll(s.data())) {}
    int64_t ID;
    std::string word = Get();
    std::string translation = Get();
    std::vector<std::string> explanation = GetVector();
    std::vector<std::string> examples = GetVector();
    std::string desc = std::string() + explanation + "\\n" + examples;
};

void GenerateWordEvents()
{
    std::ifstream input("autogen/words.txt");

    std::ofstream        eventOutput      ("events\\words.txt");
    std::ofstream localizationOutput("localisation\\words.csv");

    std::mt19937 g(time(0) % (1ll + UINT_MAX));
    std::vector<size_t> ids;
    size_t freshness = 0u;
    uint id = 50000u;
    std::string s;
    std::vector<tWord> words;
    while (Get(s))
    {
        words.emplace_back(s);
        ids.push_back(ids.size());
    }
    assertion(words.size() == ids.size());

    for (tWord const & wordStorage : words)
    {
        int64_t const wordID = wordStorage.ID;
        std::string const & word = wordStorage.word;
        std::string const & translation = wordStorage.translation;
        std::string const & desc = wordStorage.desc;
        float const months = 5.f;
        uint const answerID = ++id;
        tConditions<tTarget::kCountry> conditions{IsAI(_No)};

        std::array<std::string, 10u> flags;
        for (size_t i = 0; i != flags.size(); ++i)
            flags[i] = "eng_words_" + std::to_string(wordID) + '_' + std::to_string(i);

        std::string const picture = "eng_words_" + std::to_string(wordID);
        TriggeredCountryEvent(answerID, "Answer", desc.data(), eventOutput,
                              localizationOutput, {Option<tTarget::kCountry>({}, answerID)}, {"Ok"}, picture.data());

        tModifiers <tTarget::kCountry> modifiers;
        tEffects   <tTarget::kCountry> posEffects, negEffects{EventInCountry(tTag::kThis, answerID)};
        for (uint i = 0u; i != flags.size(); ++i)
        {
            char const * const flag = flags[i].data();
            modifiers.emplace_back(2.f,  HasGlobalFlag(flag));
            posEffects.push_back(        SetGlobalFlag(flag));
            negEffects.push_back(     RemoveGlobalFlag(flag));
        }

        for (uint i = 0u; i != flags.size(); ++i)
        {
            uint const rusID = ++id;
            uint const engID = ++id;

            tOptions<tTarget::kCountry> optionsRus{Option<tTarget::kCountry>({posEffects[i], EventInCountry(tTag::kThis, answerID)}, rusID)};
            tOptions<tTarget::kCountry> optionsEng{Option<tTarget::kCountry>({posEffects[i], EventInCountry(tTag::kThis, answerID)}, engID)};

            tOptionDescriptions optDescsRus{        word.data()};
            tOptionDescriptions optDescsEng{ translation.data()};

            for (uint j = 2u; j != 5u; ++j)
            {
                do
                {
                    if (!freshness)
                    {
                        std::shuffle(ids.begin(), ids.end(), g);
                        freshness = ids.size();
                    }
                    --freshness;
                }
                while (word == words[ids[freshness]].word);

                optDescsRus.push_back(words[ids[freshness]].word.data());

                do
                {
                    if (!freshness)
                    {
                        std::shuffle(ids.begin(), ids.end(), g);
                        freshness = ids.size();
                    }
                    --freshness;
                }
                while (translation == words[ids[freshness]].translation);

                optDescsEng.push_back(words[ids[freshness]].translation.data());

                optionsRus.push_back(Option<tTarget::kCountry>(tEffects<tTarget::kCountry>(negEffects), rusID, j));
                optionsEng.push_back(Option<tTarget::kCountry>(tEffects<tTarget::kCountry>(negEffects), engID, j));
            }

            std::shuffle(optionsRus.begin(), optionsRus.end(), g);
            std::shuffle(optionsEng.begin(), optionsEng.end(), g);

            tCondition<tTarget::kCountry> const condition =
            (
                And<tTarget::kCountry>({IsAI(_No), Not(HasGlobalFlag(flags[i].data()))})
            );

            std::string const rusPicture = "eng_words_" + std::to_string(wordID) + "_rus";
            CountryEvent(rusID, "It's time to learn words, my addict",
                         "Choose the correct translation.", eventOutput, localizationOutput,
                         Trigger          <tTarget::kCountry>({condition}),
                         MeanTimeToHappen <tTarget::kCountry>(months, tModifiers<tTarget::kCountry>(modifiers)),
                         std::move(optionsRus), optDescsRus, rusPicture.data());

            std::string const engPicture = "eng_words_" + std::to_string(wordID) + "_eng";
            CountryEvent(engID, "It's time to learn words, my addict",
                         "Choose the correct translation.", eventOutput, localizationOutput,
                         Trigger          <tTarget::kCountry>({condition}),
                         MeanTimeToHappen <tTarget::kCountry>(months, tModifiers<tTarget::kCountry>(modifiers)),
                         std::move(optionsEng), optDescsEng, engPicture.data());
        }
    }
}
