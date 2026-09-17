// Tests for the ZJSON:: convenience block appended at the end of zjson.hpp
// (direct-child access, object/array safe mutation, member counting).
#include "../src/zjson.hpp"

#include <gtest/gtest.h>

// ParseJsonStrict requires an errMsg out parameter; wrap it for brevity.
static ZJSON::Json P(const char* text)
{
    std::string err;
    return ZJSON::Json::ParseJsonStrict(std::string(text), err);
}

TEST(UtilDirectChild, FindsDirectChildOnly)
{
    const ZJSON::Json doc = P("{\"a\":{\"b\":1},\"c\":2}");
    ASSERT_TRUE(doc.isObject());

    const ZJSON::Json* a = ZJSON::directChild(doc, "a");
    ASSERT_NE(a, nullptr);
    EXPECT_TRUE(a->isObject());

    // Deep search would find "b"; directChild must not.
    EXPECT_EQ(ZJSON::directChild(doc, "b"), nullptr);
    EXPECT_EQ(ZJSON::directChild(doc, "missing"), nullptr);
}

TEST(UtilDirectChild, NonObjectReturnsNull)
{
    const ZJSON::Json arr = P("[1,2,3]");
    const ZJSON::Json num = P("42");
    EXPECT_EQ(ZJSON::directChild(arr, "0"), nullptr);
    EXPECT_EQ(ZJSON::directChild(num, "x"), nullptr);
}

TEST(UtilHasChild, PresenceCheck)
{
    const ZJSON::Json doc = P("{\"name\":\"x\",\"n\":0}");
    EXPECT_TRUE(ZJSON::hasChild(doc, "name"));
    EXPECT_TRUE(ZJSON::hasChild(doc, "n"));   // falsy value still counts
    EXPECT_FALSE(ZJSON::hasChild(doc, "other"));
    EXPECT_FALSE(ZJSON::hasChild(doc, "x"));  // nested one level down: not direct
}

TEST(UtilChildValueOr, FallsBackToDefault)
{
    const ZJSON::Json doc = P("{\"s\":\"v\",\"i\":7}");
    EXPECT_EQ(ZJSON::childValueOr(doc, "s").toString(), "v");
    EXPECT_EQ(ZJSON::childValueOr(doc, "i").toInt(), 7);
    // Json()'s default is an empty object, so the no-arg fallback yields one.
    const ZJSON::Json missing = ZJSON::childValueOr(doc, "missing");
    EXPECT_TRUE(ZJSON::isEmptyObject(missing));
    EXPECT_EQ(ZJSON::childValueOr(doc, "missing", ZJSON::Json(3.5)).toDouble(), 3.5);
}

TEST(UtilOwnedKey, MaterializesStringView)
{
    const ZJSON::Json doc = P("{\"k\":1}");
    auto it = doc.cbegin();
    const std::string owned = ZJSON::ownedKey(it->key());
    EXPECT_EQ(owned, "k");
}

TEST(UtilMemberCount, CountsMembers)
{
    EXPECT_EQ(ZJSON::memberCount(P("{}")), 0);
    EXPECT_EQ(ZJSON::memberCount(P("{\"a\":1,\"b\":2,\"c\":3}")), 3);
    // Non-objects report 0 members rather than a bogus negative size.
    EXPECT_EQ(ZJSON::memberCount(P("[1,2]")), 0);
    EXPECT_EQ(ZJSON::memberCount(P("5")), 0);

    // The isEmpty() trap: an object always reports isEmpty() == true.
    const ZJSON::Json obj = P("{\"a\":1}");
    EXPECT_TRUE(obj.isEmpty());                       // documented upstream quirk
    EXPECT_FALSE(ZJSON::isEmptyObject(obj));          // helper gives the truth
    EXPECT_TRUE(ZJSON::isEmptyObject(P("{}")));
}

TEST(UtilSetElement, ReplacesWithoutTruncatingSiblings)
{
    ZJSON::Json arr = P("[0,1,2,3]");
    EXPECT_TRUE(ZJSON::setElement(arr, 1, ZJSON::Json(std::string("x"))));
    EXPECT_EQ(arr.toString(), "[0,\"x\",2,3]");       // brothers after index 1 survive

    EXPECT_TRUE(ZJSON::setElement(arr, 0, ZJSON::Json(false)));
    EXPECT_TRUE(ZJSON::setElement(arr, 3, ZJSON::Json(9)));
    EXPECT_EQ(arr.toString(), "[false,\"x\",2,9]");
}

TEST(UtilSetElement, RejectsBadIndex)
{
    ZJSON::Json arr = P("[1,2]");
    EXPECT_FALSE(ZJSON::setElement(arr, -1, ZJSON::Json(0)));
    EXPECT_FALSE(ZJSON::setElement(arr, 2, ZJSON::Json(0)));
    EXPECT_FALSE(ZJSON::setElement(arr, 99, ZJSON::Json(0)));
    ZJSON::Json notArray = P("{\"a\":1}");
    EXPECT_FALSE(ZJSON::setElement(notArray, 0, ZJSON::Json(0)));
}

TEST(UtilSetChild, ReplacesExistingPreservingOrder)
{
    ZJSON::Json obj = P("{\"a\":1,\"b\":2,\"c\":3}");
    ZJSON::setChild(obj, "b", ZJSON::Json(std::string("new")));
    EXPECT_EQ(obj.toString(), "{\"a\":1,\"b\":\"new\",\"c\":3}");

    // The add()-only-append trap: no duplicate "b" member may appear.
    int bCount = 0;
    for (auto it = obj.cbegin(); it != obj.cend(); ++it) {
        if (it->key() == "b") ++bCount;
    }
    EXPECT_EQ(bCount, 1);
}

TEST(UtilSetChild, AddsMissingKeyAndIgnoresBadInput)
{
    ZJSON::Json obj = P("{\"a\":1}");
    ZJSON::setChild(obj, "z", ZJSON::Json(5));
    EXPECT_EQ(obj.toString(), "{\"a\":1,\"z\":5}");

    ZJSON::Json copy = obj;
    ZJSON::setChild(copy, "", ZJSON::Json(0));        // empty key: no-op
    EXPECT_EQ(copy.toString(), obj.toString());

    ZJSON::Json notObject = P("[1]");
    ZJSON::setChild(notObject, "a", ZJSON::Json(0));  // non-object: no-op
    EXPECT_TRUE(notObject.isArray());
}

TEST(UtilSetChild, ReplaceSemanticsVsDeepSearchKey)
{
    // operator[] would deep-search; setChild must treat keys literally and
    // only touch the direct member.
    ZJSON::Json obj = P("{\"n\":{\"n\":1},\"m\":2}");
    ZJSON::setChild(obj, "m", ZJSON::Json(9));
    EXPECT_EQ(obj.toString(), "{\"n\":{\"n\":1},\"m\":9}");
    // Setting "n" replaces the direct member wholesale, not the inner one.
    ZJSON::setChild(obj, "n", ZJSON::Json(1));
    EXPECT_EQ(obj.toString(), "{\"n\":1,\"m\":9}");
}

TEST(UtilQtMacroCompat, HelpersUsableFromQtMacroTU)
{
    // The util block must compile fine even in TUs that define Qt's keyword
    // macros (slots/signals/emit/foreach expand to nothing).
    ZJSON::Json obj = P("{\"k\":\"v\"}");
    EXPECT_TRUE(ZJSON::hasChild(obj, "k"));
    EXPECT_EQ(ZJSON::childValueOr(obj, "k").toString(), "v");
}
