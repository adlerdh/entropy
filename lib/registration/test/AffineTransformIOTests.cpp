#include "registration/AffineTransformIO.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace
{

void checkMatrixNear(const glm::dmat4& actual, const glm::dmat4& expected)
{
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      CHECK(actual[column][row] == Catch::Approx(expected[column][row]));
    }
  }
}

std::filesystem::path tempFile(const char* name)
{
  return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("registration affine reader accepts plain 4x4 matrices", "[registration][affine]")
{
  const std::filesystem::path fileName = tempFile("entropy-registration-plain-affine.txt");
  {
    std::ofstream out{fileName};
    out << "1 0 0 2\n";
    out << "0 1 0 3\n";
    out << "0 0 1 4\n";
    out << "0 0 0 1\n";
  }

  const std::optional<glm::dmat4> matrix = registration::readAffineTransform(fileName);
  REQUIRE(matrix);

  glm::dmat4 expected{1.0};
  expected[3] = glm::dvec4{2.0, 3.0, 4.0, 1.0};
  checkMatrixNear(*matrix, expected);
}

TEST_CASE("registration affine reader converts centered ITK affine to direct 4x4 matrix", "[registration][affine]")
{
  const std::filesystem::path fileName = tempFile("entropy-registration-itk-affine.mat");
  {
    std::ofstream out{fileName};
    out << "#Insight Transform File V1.0\n";
    out << "#Transform 0\n";
    out << "Transform: AffineTransform_double_3_3\n";
    out << "Parameters: 2 0 0 0 3 0 0 0 4 5 6 7\n";
    out << "FixedParameters: 10 20 30\n";
  }

  const std::optional<glm::dmat4> matrix = registration::readAffineTransform(fileName);
  REQUIRE(matrix);

  glm::dmat4 expected{1.0};
  expected[0][0] = 2.0;
  expected[1][1] = 3.0;
  expected[2][2] = 4.0;
  expected[3] = glm::dvec4{-5.0, -34.0, -83.0, 1.0};
  checkMatrixNear(*matrix, expected);
}

TEST_CASE("registration affine writer produces readable ITK affine files", "[registration][affine]")
{
  const std::filesystem::path fileName = tempFile("entropy-registration-written-itk-affine.mat");
  glm::dmat4 expected{1.0};
  expected[0][0] = 1.5;
  expected[1][1] = 2.5;
  expected[2][2] = 3.5;
  expected[3] = glm::dvec4{4.0, 5.0, 6.0, 1.0};

  REQUIRE(registration::writeItkAffineTransform(fileName, expected, 3));

  const std::optional<glm::dmat4> matrix = registration::readAffineTransform(fileName);
  REQUIRE(matrix);
  checkMatrixNear(*matrix, expected);
}

TEST_CASE("registration Greedy affine reader converts RAS matrices to Entropy LPS", "[registration][affine]")
{
  const std::filesystem::path fileName = tempFile("entropy-registration-greedy-ras-affine.mat");
  {
    std::ofstream out{fileName};
    out << "0 -1 0 -10\n";
    out << "1 0 0 -20\n";
    out << "0 0 1 30\n";
    out << "0 0 0 1\n";
  }

  const std::optional<glm::dmat4> matrix = registration::readGreedyAffineTransform(fileName);
  REQUIRE(matrix);

  glm::dmat4 expected{1.0};
  expected[0][0] = 0.0;
  expected[1][0] = -1.0;
  expected[0][1] = 1.0;
  expected[1][1] = 0.0;
  expected[3] = glm::dvec4{10.0, 20.0, 30.0, 1.0};
  checkMatrixNear(*matrix, expected);
}

TEST_CASE("registration Greedy affine writer converts Entropy LPS matrices to RAS", "[registration][affine]")
{
  const std::filesystem::path fileName = tempFile("entropy-registration-written-greedy-affine.mat");
  glm::dmat4 expected{1.0};
  expected[0][0] = 0.0;
  expected[1][0] = -1.0;
  expected[0][1] = 1.0;
  expected[1][1] = 0.0;
  expected[3] = glm::dvec4{10.0, 20.0, 30.0, 1.0};

  REQUIRE(registration::writeGreedyAffineTransform(fileName, expected));

  const std::optional<glm::dmat4> matrix = registration::readGreedyAffineTransform(fileName);
  REQUIRE(matrix);
  checkMatrixNear(*matrix, expected);

  const std::optional<glm::dmat4> rawRasMatrix = registration::readAffineTransform(fileName);
  REQUIRE(rawRasMatrix);
  CHECK((*rawRasMatrix)[3][0] == Catch::Approx(-10.0));
  CHECK((*rawRasMatrix)[3][1] == Catch::Approx(-20.0));
  CHECK((*rawRasMatrix)[3][2] == Catch::Approx(30.0));
}
