#pragma once

#include <glm/vec3.hpp>

#include <map>
#include <string>

/**
 * @brief Lookup table for canonical Cartesian, view, and anatomical directions.
 *
 * The class stores Entropy's conventional unit vectors and short labels for
 * coordinate directions. Human anatomical directions follow LPS-positive
 * coordinates: left, posterior, and superior are positive X, Y, and Z.
 */
class Directions
{
public:
  /// Cartesian basis directions and commonly used diagonal directions.
  enum class Cartesian
  {
    PosX, //!< (1, 0, 0)
    PosY, //!< (0, 1, 0)
    PosZ, //!< (0, 0, 1)
    NegX, //!< (-1, 0, 0)
    NegY, //!< (0, -1, 0)
    NegZ, //!< (0, 0, -1)
    XY,   //!< (1, 1, 0)
    YZ,   //!< (0, 1, 1)
    ZX,   //!< (1, 0, 1)
    XYZ   //!< (1, 1, 1)
  };

  /// Directions relative to the viewer looking at the screen.
  enum class View
  {
    Left,
    Right,
    Down,
    Up,
    Front, //!< into screen (away from viewer)
    Back   //!< out of screen (towards viewer)
  };

  /// Directions relative to a human subject in LPS-positive coordinates.
  enum class Anatomy
  {
    Right,
    Left,
    Anterior,
    Posterior,
    Inferior,
    Superior
  };

  /// Directions relative to a rodent subject.
  enum class Animal
  {
    Right,
    Left,
    Rostral,
    Dorsal,
    Caudal,
    Ventral
  };

  /// Return the normalized Cartesian direction vector.
  static glm::vec3 get(const Cartesian& dir);

  /// Return the normalized view direction vector.
  static glm::vec3 get(const View& dir);

  /// Return the normalized human anatomical direction vector.
  static glm::vec3 get(const Anatomy& dir);

  /// Return the normalized rodent anatomical direction vector.
  static glm::vec3 get(const Animal& dir);

  /// Return the short Cartesian label, such as "+x" or "xy".
  static const std::string& abbrev(const Cartesian& dir);

  /// Return the short human anatomical label, such as "L" or "S".
  static const std::string& abbrev(const Anatomy& dir);

  /// Return the short rodent anatomical label, such as "Ros" or "Ven".
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
