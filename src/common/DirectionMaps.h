#pragma once

#include <glm/vec3.hpp>

#include <map>
#include <string>

class Directions
{
public:
  enum class Cartesian
  {
    PosX, //!< (1, 0, 0)
    PosY, //!< (0, 1, 0)
    PosZ, //!< (0, 0, 1)
    NegX, //!< (-1, 0, 0)
    NegY, //!< (0, -1, 0)
    NegZ, //!< (0, 0, -1)
    XY, //!< (1, 1, 0)
    YZ, //!< (0, 1, 1)
    ZX, //!< (1, 0, 1)
    XYZ //!< (1, 1, 1)
  };

  /// Directions relative to the viewer looking at the screen
  enum class View
  {
    Left,
    Right,
    Down,
    Up,
    Front, //!< into screen (away from viewer)
    Back   //!< out of screen (towards viewer)
  };

  /// Directions relative to human subject
  enum class Anatomy
  {
    Right,
    Left,
    Anterior,
    Posterior,
    Inferior,
    Superior
  };

  /// Directions relative to rodent animal subject
  enum class Animal
  {
    Right,
    Left,
    Rostral,
    Dorsal,
    Caudal,
    Ventral
  };

  static glm::vec3 get(const Cartesian& dir);
  static glm::vec3 get(const View& dir);
  static glm::vec3 get(const Anatomy& dir);
  static glm::vec3 get(const Animal& dir);

  static const std::string& abbrev(const Cartesian& dir);
  static const std::string& abbrev(const Anatomy& dir);
  static const std::string& abbrev(const Animal& dir);

private:
  static const std::map<Cartesian, glm::vec3> s_cartesianDirections;
  static const std::map<View, glm::vec3> s_viewDirections;
  static const std::map<Anatomy, glm::vec3> s_anatomicalDirections;
  static const std::map<Animal, glm::vec3> s_animalDirections;

  static const std::map<Cartesian, std::string> s_cartesianAbbrevs;
  static const std::map<Anatomy, std::string> s_anatomicalAbbrevs;
  static const std::map<Animal, std::string> s_animalAbbrevs;
};
