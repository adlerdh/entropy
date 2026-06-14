#include "common/Viewport.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("default viewport is a unit logical viewport", "[common][viewport]")
{
  const Viewport viewport;

  CHECK(viewport.left() == Catch::Approx(0.0f));
  CHECK(viewport.bottom() == Catch::Approx(0.0f));
  CHECK(viewport.width() == Catch::Approx(1.0f));
  CHECK(viewport.height() == Catch::Approx(1.0f));
  CHECK(viewport.area() == Catch::Approx(1.0f));
  CHECK(viewport.aspectRatio() == Catch::Approx(1.0f));
}

TEST_CASE("viewport stores and updates logical bounds", "[common][viewport]")
{
  Viewport viewport{2.0f, 3.0f, 640.0f, 320.0f};
  const Viewport fromVec4{glm::vec4{6.0f, 7.0f, 8.0f, 9.0f}};

  CHECK(viewport.getAsVec4() == glm::vec4{2.0f, 3.0f, 640.0f, 320.0f});
  CHECK(fromVec4.getAsVec4() == glm::vec4{6.0f, 7.0f, 8.0f, 9.0f});
  CHECK(viewport.area() == Catch::Approx(204800.0f));
  CHECK(viewport.aspectRatio() == Catch::Approx(2.0f));

  viewport.setLeft(4.0f);
  viewport.setBottom(5.0f);
  viewport.setWidth(80.0f);
  viewport.setHeight(40.0f);

  CHECK(viewport.getAsVec4() == glm::vec4{4.0f, 5.0f, 80.0f, 40.0f});

  viewport.setAsVec4({10.0f, 20.0f, 30.0f, 15.0f});

  CHECK(viewport.getAsVec4() == glm::vec4{10.0f, 20.0f, 30.0f, 15.0f});
}

TEST_CASE("viewport converts logical bounds to device-pixel bounds", "[common][viewport]")
{
  Viewport viewport{10.0f, 20.0f, 100.0f, 50.0f};
  viewport.setDevicePixelRatio({2.0f, 3.0f});

  CHECK(viewport.devicePixelRatio() == glm::vec2{2.0f, 3.0f});
  CHECK(viewport.deviceLeft() == Catch::Approx(20.0f));
  CHECK(viewport.deviceBottom() == Catch::Approx(60.0f));
  CHECK(viewport.deviceWidth() == Catch::Approx(200.0f));
  CHECK(viewport.deviceHeight() == Catch::Approx(150.0f));
  CHECK(viewport.deviceArea() == Catch::Approx(30000.0f));
  CHECK(viewport.deviceAspectRatio() == Catch::Approx(4.0f / 3.0f));
  CHECK(viewport.getDeviceAsVec4() == glm::vec4{20.0f, 60.0f, 200.0f, 150.0f});
}
