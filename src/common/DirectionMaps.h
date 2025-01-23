#ifndef DIRECTION_MAPS_H
#define DIRECTION_MAPS_H

#include <glm/vec3.hpp>

#include <map>
#include <string>

class Directions
{
public:
  enum class Cartesian
  {
    X,  //!< (1, 0, 0)
    Y,  //!< (0, 1, 0)
    Z,  //!< (0, 0, 1)
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

  static const std::string& abbrev(const Anatomy& dir);
  static const std::string& abbrev(const Animal& dir);

  using AnatomicalAbbreviationMap = std::map<Anatomy, std::string>;
  static const AnatomicalAbbreviationMap s_anatomicalAbbrevs;

  using AnimalAbbreviationMap = std::map<Animal, std::string>;
  static const AnimalAbbreviationMap s_animalAbbrevs;

private:
  using CartesianDirectionsMap = std::map<Cartesian, glm::vec3>;
  static const CartesianDirectionsMap s_cartesianDirections;

  using ViewDirectionsMap = std::map<View, glm::vec3>;
  static const ViewDirectionsMap s_viewDirections;

  using AnatomicalDirectionsMap = std::map<Anatomy, glm::vec3>;
  static const AnatomicalDirectionsMap s_anatomicalDirections;

  using AnimalDirectionsMap = std::map<Animal, glm::vec3>;
  static const AnimalDirectionsMap s_animalDirections;
};

#endif // DIRECTION_MAPS_H
