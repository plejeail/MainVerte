#include "mvlog/mvlog.h"

#include <doctest/doctest.h>

TEST_CASE("Qt log formatters preserve UTF-8, views and formatting options") {
    const auto text = QString::fromUtf8("\xc3\xa9t\xc3\xa9");
    CHECK(std::format("{}", text) == "\xc3\xa9t\xc3\xa9");
    CHECK(std::format("{}", QStringView{text}) == "\xc3\xa9t\xc3\xa9");
    CHECK(std::format("{}", QStringLiteral("temporary")) == "temporary");
    CHECK(std::format("{:>5}", QStringLiteral("abc")) == "  abc");
    const QByteArray bytes{"a\0bc", 4};
    CHECK(std::format("{}", bytes) == std::string("a\0bc", 4));
    CHECK(std::format("{}", QByteArrayView{bytes}.first(3)) == std::string("a\0b", 3));
    CHECK(std::format("{}", QStringList{QStringLiteral("a"), QStringLiteral("b")}) == "a,b");
    mvlog::dbg("Qt formatter: {}", text);
}
