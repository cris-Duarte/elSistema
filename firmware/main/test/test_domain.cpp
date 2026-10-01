#include "model.hpp"
#include "services.hpp"

#include "unity.h"

using namespace el_sistema;

TEST_CASE("XP aplica bases, minutos completos y límite temporal", "[el_sistema][xp]")
{
    TEST_ASSERT_EQUAL_UINT32(25, XpService::reward(MissionRank::E, 0));
    TEST_ASSERT_EQUAL_UINT32(120, XpService::reward(MissionRank::C, 10 * 60));
    TEST_ASSERT_EQUAL_UINT32(840, XpService::reward(MissionRank::S, 400 * 60));
}

TEST_CASE("una recompensa puede subir varios niveles", "[el_sistema][xp]")
{
    Profile profile;
    XpService::grant(profile, 1800);
    TEST_ASSERT_EQUAL_UINT32(4, profile.level);
    TEST_ASSERT_EQUAL_UINT32(0, profile.level_xp);
    TEST_ASSERT_EQUAL_UINT64(1800, profile.total_xp);
}

TEST_CASE("la búsqueda ignora mayúsculas y acentos españoles", "[el_sistema][search]")
{
    Mission mission;
    mission.title = "Revisión técnica";
    mission.description = "Diseño del módulo español";
    TEST_ASSERT_TRUE(ApplicationService::matches_search(mission, "revision"));
    TEST_ASSERT_TRUE(ApplicationService::matches_search(mission, "TECNICA"));
    TEST_ASSERT_TRUE(ApplicationService::matches_search(mission, "espanol"));
    TEST_ASSERT_FALSE(ApplicationService::matches_search(mission, "audio"));
}

TEST_CASE("el modelo localiza misiones por identificador estable", "[el_sistema][model]")
{
    AppState state;
    state.missions.push_back(Mission{.id = 7, .title = "Misión"});
    TEST_ASSERT_NOT_NULL(find_mission(state, 7));
    TEST_ASSERT_NULL(find_mission(state, 8));
}
