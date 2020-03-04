#include "adding_cores.h"
#include "capital_cultures_acceptance.h"
#include "events_id_checker.h"
#include "events_to_change_primary_culturies.h"
#include "new_primary_culture_for_dxx.h"
#include "peaceful_settlement.h"
#include "plus_minus_cultures_events.h"
#include "simple_events.h"
#include "test.h"

int main()
{
#ifdef NDEBUG
    AddingAndRemovingCultures();
    CoreAddingEvents();
    CapitalCulturesAcceptance();
    EventsToChangePrimaryCultures();
    GenerateSimpleEvents();
    NewPrimaryCulturesForDXX();
    PeacefulSettlement();
#else
    Test();
#endif

    CheckEventsID();
}
