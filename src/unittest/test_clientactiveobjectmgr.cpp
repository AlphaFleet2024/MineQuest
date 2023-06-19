/*
Minetest
Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "test.h"

#include "client/activeobjectmgr.h"

#include <catch.hpp>

class TestClientActiveObject : public ClientActiveObject
{
public:
	TestClientActiveObject() : ClientActiveObject(0, nullptr, nullptr) {}
	~TestClientActiveObject() = default;
	ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_TEST; }
	virtual void addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr) {}
};

class TestSelectableClientActiveObject : public ClientActiveObject
{
public:
	TestSelectableClientActiveObject(aabb3f _selection_box)
		: ClientActiveObject(0, nullptr, nullptr)
		, selection_box(_selection_box)
	{}

	~TestSelectableClientActiveObject() = default;
	ActiveObjectType getType() const override { return ACTIVEOBJECT_TYPE_TEST; }
	void addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr) override {}
	bool getSelectionBox(aabb3f *toset) const override { *toset = selection_box; return true; }
	const v3f getPosition() const override { return position; }

	v3f position;
	aabb3f selection_box;
};

class TestClientActiveObjectMgr : public TestBase
{
public:
	TestClientActiveObjectMgr() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestClientActiveObjectMgr"; }

	void runTests(IGameDef *gamedef);

	void testGetActiveSelectableObjects();
};

static TestClientActiveObjectMgr g_test_instance;

void TestClientActiveObjectMgr::runTests(IGameDef *gamedef)
{
	TEST(testGetActiveSelectableObjects)
}

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("test client active object manager")
{
	client::ActiveObjectMgr caomgr;
	auto tcao1 = new TestClientActiveObject();
	REQUIRE(caomgr.registerObject(tcao1) == true);

	SECTION("When we register a client object, "
			"then it should be assigned a unique ID.")
	{
		for (int i = 0; i < UINT8_MAX; ++i) {
			auto other_tcao = new TestClientActiveObject();
			REQUIRE(caomgr.registerObject(other_tcao) == true);
			CHECK(other_tcao->getId() != tcao1->getId());
		}
	}

	SECTION("two registered objects")
	{
		auto tcao2 = new TestClientActiveObject();
		REQUIRE(caomgr.registerObject(tcao2) == true);
		auto tcao2_id = tcao2->getId();

		auto obj1 = caomgr.getActiveObject(tcao1->getId());
		REQUIRE(obj1 != nullptr);

		auto obj2 = caomgr.getActiveObject(tcao2_id);
		REQUIRE(obj2 != nullptr);

		SECTION("When we query an object by its ID, "
				"then we should get back an object with that ID.")
		{
			CHECK(obj1->getId() == tcao1->getId());
			CHECK(obj2->getId() == tcao2->getId());
		}

		SECTION("When we register and query for an object, "
				"its memory location should not have changed.")
		{
			CHECK(obj1 == tcao1);
			CHECK(obj2 == tcao2);
		}
	}

	SECTION("Given an object has been removed, "
			"when we query for it, "
			"then we should get nullptr.")
	{
		auto id = tcao1->getId();
		caomgr.removeObject(tcao1->getId()); // may invalidate tcao1
		CHECK(caomgr.getActiveObject(id) == nullptr);
	}

	caomgr.clear();
}

void TestClientActiveObjectMgr::testGetActiveSelectableObjects()
{
	client::ActiveObjectMgr caomgr;
	auto obj_u = std::make_unique<TestSelectableClientActiveObject>(
			aabb3f{v3f{-1, -1, -1}, v3f{1, 1, 1}});
	auto obj = obj_u.get();
	UASSERT(caomgr.registerObject(std::move(obj_u)));

	auto assert_obj_selected = [&] (v3f a, v3f b) {
		auto actual = caomgr.getActiveSelectableObjects({a, b});
		UASSERTEQ(auto, actual.size(), 1u);
		UASSERTEQ(auto, actual.at(0).obj, obj);
	};

	auto assert_obj_missed = [&] (v3f a, v3f b) {
		auto actual = caomgr.getActiveSelectableObjects({a, b});
		UASSERTEQ(auto, actual.size(), 0u);
	};

	float x = 12, y = 3, z = 6;
	obj->position = {x, y, z};

	assert_obj_selected({0, 0, 0}, {x-1, y-1, z-1});
	assert_obj_selected({0, 0, 0}, {2*(x-1), 2*(y-1), 2*(z-1)});
	assert_obj_selected({0, 0, 0}, {2*(x+1), 2*(y-1), 2*(z+1)});
	assert_obj_selected({0, 0, 0}, {20, 5, 10});

	assert_obj_selected({30, -12, 17}, {x+1, y+1, z-1});
	assert_obj_selected({30, -12, 17}, {x, y+1, z});
	assert_obj_selected({30, -12, 17}, {-6, 20, -5});
	assert_obj_selected({30, -12, 17}, {-8, 20, -7});

	assert_obj_selected({-21, 6, -13}, {x+1.4f, y, z});
	assert_obj_selected({-21, 6, -13}, {x-1.4f, y, z});
	assert_obj_missed({-21, 6, -13}, {x-3.f, y, z});

	assert_obj_selected({-21, 6, -13}, {x, y-1.4f, z});
	assert_obj_selected({-21, 6, -13}, {x, y+1.4f, z});
	assert_obj_missed({-21, 6, -13}, {x, y+3.f, z});

	assert_obj_selected({-21, 6, -13}, {x, y, z+1.4f});
	assert_obj_selected({-21, 6, -13}, {x, y, z-1.4f});
	assert_obj_missed({-21, 6, -13}, {x, y, z-3.f});

	caomgr.clear();
}
