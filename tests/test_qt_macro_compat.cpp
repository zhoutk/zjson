// Qt keyword-macro compatibility.
//
// Qt's <qobjectdefs.h> defines its fancy keywords as macros that expand to
// nothing:
//
//     #define slots   Q_SLOTS      // Q_SLOTS is empty
//     #define signals Q_SIGNALS
//     #define emit    Q_EMIT
//     #define foreach Q_FOREACH
//
// Any header that uses one of those words as an *identifier* therefore fails to
// compile inside a Qt translation unit.  That is not hypothetical: the hash
// index inside jsonNodeAllocator's key index had a member named `slots`, so
// `slots[position]` pre-expanded to `[position]` - a lambda introducer - and the
// header became unusable for Qt consumers (geode's maple target) while still
// compiling fine in Qt-free ones (the ORM tests).  The fix renamed that private
// member to `slotTable`.
//
// This test pins the property down: it defines the Qt macros first, then
// includes the library and exercises it.  If a colliding identifier is ever
// reintroduced, this TU stops compiling.
//
// Keep the #defines above the include, and keep this file free of those words.

#define slots
#define signals
#define emit
#define foreach

// A second, Qt-like strictness: Qt also defines these to nothing when
// QT_NO_KEYWORDS is used inconsistently, so the test does not re-enable them.

#include "../src/zjson.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using ZJSON::Json;
using ZJSON::JsonType;

TEST(QtMacroCompat, ParsesAndSerializesWithQtKeywordsMacroDefined)
{
    std::string err;
    const Json parsed = Json::ParseJsonStrict(R"({"name":"geode","n":42,"ok":true})", err);
    ASSERT_FALSE(parsed.isError()) << err;
    EXPECT_EQ(parsed["name"].toString(), "geode");
    EXPECT_EQ(parsed["n"].toInt(), 42);
    EXPECT_TRUE(parsed["ok"].toBool());

    // Round-trip through the same accessors the ORM relies on.
    const std::string text = parsed.toString();
    EXPECT_NE(text.find("\"name\""), std::string::npos);
    EXPECT_EQ(text.find('\n'), std::string::npos);  // compact
}

TEST(QtMacroCompat, KeyIndexAndIterationWork)
{
    // Exercises the hash key index (the code that used to hold the colliding
    // member name) plus object iteration.
    Json object;
    for (int i = 0; i < 200; ++i)
        object.add("k" + std::to_string(i), i);

    EXPECT_TRUE(object.contains("k199"));
    EXPECT_EQ(object["k199"].toInt(), 199);
    EXPECT_FALSE(object.contains("missing"));

    int seen = 0;
    for (auto it = object.cbegin(); it != object.cend(); ++it) {
        // key() yields a string_view; materialize like the ORM does.
        const std::string key(it->key());
        ASSERT_FALSE(key.empty());
        ++seen;
    }
    EXPECT_EQ(seen, 200);

    // Mutation paths used by JsonFileDb.
    Json array(JsonType::Array);
    array.push_back(Json(1));
    array.push_back(Json(2));
    array.insert(1, Json(9));
    array.remove(0);
    ASSERT_EQ(array.size(), 2);
    EXPECT_EQ(array[0].toInt(), 9);

    Json holder;
    holder.add("nested", object);
    holder.remove("nested");
    EXPECT_FALSE(holder.contains("nested"));
}
