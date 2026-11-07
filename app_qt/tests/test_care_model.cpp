// clazy:skip
#define DOCTEST_CONFIG_IMPLEMENT
#include <deque>
#include <utility>

#include <QCoreApplication>
#include <doctest.h>

#include "care_model.h"
#include "translator.h"

namespace {

struct Pending {
    mv::CareAction action;
    mv::MoveOnlyFunction<void(mv::CareResult)> callback;
};
std::deque<Pending> pending;
bool accept = true;
void complete(mv::CareResult result) {
    REQUIRE(pending.size() == 1);
    auto callback = std::move(pending.front().callback);
    pending.pop_front();
    callback(std::move(result));
}
mv::CareResult failure() {
    return std::unexpected(mv::MainverteError{mv::MainverteError::Code::Database, "Controlled failure"});
}

}  //  namespace

bool mv::manage_care(CareAction action, MoveOnlyFunction<void(CareResult)> callback) {
    pending.push_back({action, std::move(callback)});
    return accept;
}

TEST_CASE("Care model waits for completion and preserves confirmed data on failures") {
    pending.clear();
    accept = true;
    ts::select_initial_language("en");
    CareModel model;
    int saved = 0;
    QObject::connect(&model, &CareModel::saved, [&] { ++saved; });
    model.setSpecimenId("42");
    CHECK(model.busy());
    CHECK_FALSE(model.loaded());
    CHECK(model.availableTypes().empty());
    complete(std::vector<mv::CareNeed>{});
    CHECK(model.loaded());
    CHECK(model.availableTypes().size() == 8);
    SUBCASE("accepted write failure") {}
    SUBCASE("queue refusal delivers failure") { accept = false; }
    model.add(0);
    CHECK(model.busy());
    CHECK(model.needs().empty());
    CHECK(saved == 0);
    complete(failure());
    CHECK_FALSE(model.busy());
    CHECK_FALSE(model.error().isEmpty());
    CHECK(model.needs().empty());
    CHECK(model.availableTypes().size() == 8);
    CHECK(saved == 0);
    accept = true;
    model.add(0);
    complete(std::vector<mv::CareNeed>{{1,42,mv::NeedType::Watering}});
    REQUIRE(model.needs().size() == 1);
    CHECK(model.availableTypes().size() == 7);
    CHECK(model.error().isEmpty());
    CHECK(saved == 1);
    model.add(0);
    CHECK(pending.empty()); // UI duplicate restriction only.
    model.remove("1");
    CHECK(model.needs().size() == 1);
    complete(failure());
    CHECK(model.needs().size() == 1);
    CHECK(saved == 1);
    model.remove("1");
    complete(std::vector<mv::CareNeed>{});
    CHECK(model.needs().empty());
    CHECK(model.availableTypes().size() == 8);
    CHECK(saved == 2);
}

TEST_CASE("Care model ignores stale callbacks and guards destroyed receivers") {
    pending.clear();
    CareModel model;
    model.setSpecimenId("42");
    model.setSpecimenId("43");
    complete(std::vector<mv::CareNeed>{{1,42,mv::NeedType::Watering}});
    CHECK(model.needs().empty());
    CHECK(model.busy());
    REQUIRE(pending.size() == 1);
    CHECK(pending.front().action.specimen_id == 43);
    complete(failure());
    CHECK_FALSE(model.loaded());
    CHECK(model.availableTypes().empty());
    CHECK_FALSE(model.error().isEmpty());
    model.refresh();
    complete(std::vector<mv::CareNeed>{});
    CHECK(model.loaded());
    {
        CareModel temporary;
        temporary.setSpecimenId("42");
    }
    complete(std::vector<mv::CareNeed>{});
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    doctest::Context context(argc, argv);
    return context.run();
}
