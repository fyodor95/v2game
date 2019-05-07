#include "cultures.h"

tCulture::tCulture(char const * const name, char const * const localization)
    : Name(name)
    , Localization(localization)
{
}

SINGLETON(tCultures, Cultures)
{
    return
    {
          {"north_german", "Северные германцы"}
        , {"south_german", "Южные германцы"}
        , {"ashkenazi", "Евреи"}
        , {"dutch", "Голландцы"}
        , {"flemish", "Фламандцы"}
        , {"swiss", "Швейцарцы"}
        , {"swedish", "Шведы"}
        , {"danish", "Датчане"}
        , {"norwegian", "Норвежцы"}
        , {"icelandic", "Исландцы"}
        , {"finnish", "Финны"}
        , {"sami", "Саамская"}
        , {"british", "Англичане"}
        , {"irish", "Ирландцы"}
        , {"north_italian", "Северные итальянцы"}
        , {"south_italian", "Южные итальянцы"}
        , {"maltese", "Мальтийцы"}
        , {"basque", "Баски"}
        , {"spanish", "Испанцы"}
        , {"catalan", "Каталанцы"}
        , {"portuguese", "Португальцы"}
        , {"french", "Французы"}
        , {"croat", "Хорваты"}
        , {"serb", "Сербы"}
        , {"bulgarian", "Болгары"}
        , {"albanian", "Албанцы"}
        , {"slovene", "Словены"}
        , {"bosniak", "Босняки"}
        , {"romanian", "Румыны"}
        , {"hungarian", "Венгры"}
        , {"polish", "Поляки"}
        , {"lithuanian", "Литовцы"}
        , {"czech", "Чехи"}
        , {"slovak", "Словаки"}
        , {"russian", "Русские"}
        , {"byelorussian", "Белорусы"}
        , {"ukrainian", "Украинцы"}
        , {"ugrian", "Финно-угры"}
        , {"latvian", "Латыши"}
        , {"estonian", "Эстонцы"}
        , {"greek", "Греки"}
        , {"georgian", "Грузины"}
        , {"armenian", "Армяне"}
        , {"sephardic", "Сефарды"}
        , {"turkish", "Турки"}
        , {"azerbaijani", "Азербайджанцы"}
        , {"north_caucasian", "Кавказцы"}
        , {"maghrebi", "Магрибцы"}
        , {"misri", "Египтяне"}
        , {"mashriqi", "Машрики"}
        , {"bedouin", "Бедуины"}
        , {"berber", "Берберы"}
        , {"persian", "Персы"}
        , {"uzbek", "Узбеки"}
        , {"kazak", "Казахи"}
        , {"kirgiz", "Киргизы"}
        , {"tajik", "Таджики"}
        , {"uighur", "Уйгуры"}
        , {"pashtun", "Пуштуны"}
        , {"baluchi", "Белуджи"}
        , {"hazara", "Газара"}
        , {"turkmen", "Туркмены"}
        , {"kurdish", "Курды"}
        , {"tatar", "Татары"}
        , {"mongol", "Монголы"}
        , {"siberian", "Сибирцы"}
        , {"yakut", "Якуты"}
        , {"tibetan", "Тибетцы"}
        , {"assamese", "Ассамцы"}
        , {"bengali", "Бенгальцы"}
        , {"bihari", "Бихарцы"}
        , {"manipuri", "Манипури"}
        , {"nepali", "Непальцы"}
        , {"oriya", "Ория"}
        , {"sinhala", "Сингалы"}
        , {"avadhi", "Авадхи"}
        , {"kanauji", "Канауджи"}
        , {"panjabi", "Пенджабцы"}
        , {"kashmiri", "Кашмирцы"}
        , {"gujarati", "Гуджарати"}
        , {"marathi", "Маратхи"}
        , {"sindi", "Синды"}
        , {"rajput", "Раджпуты"}
        , {"kannada", "Каннара"}
        , {"malayalam", "Малаялам"}
        , {"tamil", "Тамилы"}
        , {"telegu", "Телугу"}
        , {"vietnamese", "Вьетнамцы"}
        , {"khmer", "Кхмеры"}
        , {"malay", "Малайзийцы"}
        , {"filipino", "Филиппинцы"}
        , {"javan", "Яванцы"}
        , {"moluccan", "Молуккцы"}
        , {"thai", "Тайцы"}
        , {"lao", "Лао"}
        , {"shan", "Шань"}
        , {"burmese", "Бирманцы"}
        , {"asian_minor", "Азиатские меньшинства"}
        , {"kachin", "Качины"}
        , {"karen", "Карены"}
        , {"japanese", "Японцы"}
        , {"manchu", "Манчжуры"}
        , {"beifaren", "Пекинцы"}
        , {"nanfaren", "Нанкинцы"}
        , {"korean", "Корейцы"}
        , {"ainu", "Айны"}
        , {"hakka", "Хакка"}
        , {"miao", "Шанхайцы"}
        , {"min", "Мин"}
        , {"zhuang", "Чжуаны"}
        , {"yi", "Юэ"}
        , {"yue", "Кантонцы"}
        , {"polynesian", "Полинезийцы"}
        , {"hawaiian", "Гавайцы"}
        , {"maori", "Маори"}
        , {"aborigine", "Аборигены"}
        , {"melanesian", "Меланезийцы"}
        , {"micronesian", "Микронезийцы"}
        , {"zapotec", "Сапотеки"}
        , {"mayan", "Майя"}
        , {"nahua", "Науа"}
        , {"tarascan", "Тараски"}
        , {"guajiro", "Гуахиро"}
        , {"tupinamba", "Тупинамба"}
        , {"metis", "Метисы"}
        , {"dakota", "Дакота"}
        , {"cherokee", "Чероки"}
        , {"pueblo", "Пуэбло"}
        , {"inuit", "Инуиты"}
        , {"cree", "Кри"}
        , {"navajo", "Навахо"}
        , {"native_american_minor", "Индейские меньшинства"}
        , {"yankee", "Янки"}
        , {"dixie", "Дикси"}
        , {"texan", "Техасцы"}
        , {"boer", "Буры"}
        , {"australian", "Австралийцы"}
        , {"anglo_canadian", "Англо-канадцы"}
        , {"french_canadian", "Франко-канадцы"}
        , {"mexican", "Мексиканцы"}
        , {"central_american", "Центральные американцы"}
        , {"caribeno", "Карибцы"}
        , {"north_andean", "Северные андцы"}
        , {"south_andean", "Южные андцы"}
        , {"platinean", "Аргентинцы"}
        , {"brazilian", "Бразильцы"}
        , {"afro_brazilian", "Афробразильцы"}
        , {"quechua", "Кечуа"}
        , {"guarani", "Гуарани"}
        , {"aimara", "Аймара"}
        , {"amazonian", "Амазонцы"}
        , {"patagonian", "Патагонцы"}
        , {"afro_american", "Афроамериканцы"}
        , {"afro_caribbean", "Афрокарибы"}
        , {"afro_caribeno", "Афрокарибеньо"}
        , {"afro_antillean", "Афроантильцы"}
        , {"akan", "Акан"}
        , {"bambara", "Бамбара"}
        , {"dyula", "Дьюла"}
        , {"edo", "Эдо"}
        , {"ewe", "Эве"}
        , {"fon", "Фон"}
        , {"fulbe", "Фульбе"}
        , {"hausa", "Хауса"}
        , {"ibibio", "Ибибио"}
        , {"ibo", "Ибо"}
        , {"kru", "Кру"}
        , {"mande", "Манде"}
        , {"maures", "Мавры"}
        , {"mossi", "Моси"}
        , {"senufo", "Сенуфо"}
        , {"songhai", "Сонгай"}
        , {"tiv", "Тив"}
        , {"tuareg", "Туареги"}
        , {"wolof", "Волоф"}
        , {"yoruba", "Йоруба"}
        , {"bakongo", "Баконго"}
        , {"baguirmi", "Багирми"}
        , {"fang", "Фанг"}
        , {"kanuri", "Канури"}
        , {"luba", "Балуба"}
        , {"lunda", "Балунда"}
        , {"mongo", "Монго"}
        , {"sara", "Сара"}
        , {"teda", "Теда"}
        , {"african_minor", "Африканские меньшинства"}
        , {"amhara", "Амхара"}
        , {"afar", "Афар"}
        , {"azande", "Азанде"}
        , {"baganda", "Баганда"}
        , {"beja", "Беджа"}
        , {"dinka", "Динка"}
        , {"fur", "Фор"}
        , {"kikuyu", "Кикуйю"}
        , {"luo", "Луо"}
        , {"maasai", "Масаи"}
        , {"nuer", "Нуэр"}
        , {"nuba", "Нубийцы"}
        , {"oromo", "Оромо"}
        , {"ruanda", "Руанди"}
        , {"rundi", "Рунди"}
        , {"sidama", "Сидамо"}
        , {"somali", "Сомалийцы"}
        , {"sudanese", "Суданцы"}
        , {"sukuma", "Сукума"}
        , {"swahili", "Суахили"}
        , {"tigray", "Тиграй"}
        , {"unyamwezi", "Ньямвези"}
        , {"chewa", "Чева"}
        , {"herero", "Гереро"}
        , {"khoisan", "Койсаны"}
        , {"lomwe", "Ломве"}
        , {"makua", "Макуа"}
        , {"malagasy", "Малагасийцы"}
        , {"nguni", "Нгуни"}
        , {"ovimbundu", "Овимбунду"}
        , {"sena", "Сену"}
        , {"shona", "Шона"}
        , {"sotho", "Сото"}
        , {"tonga", "Тонганцы"}
        , {"tswana", "Тсвана"}
        , {"xhosa", "Коса"}
        , {"yao", "Яо"}
        , {"zulu", "Зулусы"}
        , {"polar_bears", "Белые медведи"}
        , {"anglo_african", "Англо-африканцы"}
        , {"buryat", "Буряты"}
        , {"kalmyk", "Калмыки"}
        , {"crimean_tatar", "Крымские татары"}
        , {"tuvan", "Тувинцы"}
        , {"cubans", "Кубинцы"}
        , {"scottish", "Шотландцы"}
        , {"new_zelandian", "Новозеландцы"}
        , {"wallonian", "Валлоны"}
    };
}
