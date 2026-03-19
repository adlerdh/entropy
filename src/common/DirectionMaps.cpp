#include "common/DirectionMaps.h"

#include <glm/glm.hpp>

const std::map<Directions::Cartesian, glm::vec3> Directions::s_cartesianDirections = {
  {Directions::Cartesian::PosX, glm::vec3{1, 0, 0}},
  {Directions::Cartesian::PosY, glm::vec3{0, 1, 0}},
  {Directions::Cartesian::PosZ, glm::vec3{0, 0, 1}},
  {Directions::Cartesian::NegX, glm::vec3{-1, 0, 0}},
  {Directions::Cartesian::NegY, glm::vec3{0, -1, 0}},
  {Directions::Cartesian::NegZ, glm::vec3{0, 0, -1}},
  {Directions::Cartesian::XY, glm::normalize(glm::vec3{1, 1, 0})},
  {Directions::Cartesian::YZ, glm::normalize(glm::vec3{0, 1, 1})},
  {Directions::Cartesian::ZX, glm::normalize(glm::vec3{1, 0, 1})},
  {Directions::Cartesian::XYZ, glm::normalize(glm::vec3{1, 1, 1})},
};

const std::map<Directions::View, glm::vec3> Directions::s_viewDirections = {
  {Directions::View::Left, -glm::vec3{1, 0, 0}},
  {Directions::View::Right, glm::vec3{1, 0, 0}},
  {Directions::View::Down, -glm::vec3{0, 1, 0}},
  {Directions::View::Up, glm::vec3{0, 1, 0}},
  {Directions::View::Front, -glm::vec3{0, 0, 1}},
  {Directions::View::Back, glm::vec3{0, 0, 1}}
};

const std::map<Directions::Anatomy, glm::vec3> Directions::s_anatomicalDirections = {
  {Directions::Anatomy::Right, -glm::vec3{1, 0, 0}},
  {Directions::Anatomy::Left, glm::vec3{1, 0, 0}},
  {Directions::Anatomy::Anterior, -glm::vec3{0, 1, 0}},
  {Directions::Anatomy::Posterior, glm::vec3{0, 1, 0}},
  {Directions::Anatomy::Inferior, -glm::vec3{0, 0, 1}},
  {Directions::Anatomy::Superior, glm::vec3{0, 0, 1}}
};

const std::map<Directions::Animal, glm::vec3> Directions::s_animalDirections = {
  {Directions::Animal::Right, -glm::vec3{1, 0, 0}},
  {Directions::Animal::Left, glm::vec3{1, 0, 0}},
  {Directions::Animal::Ventral, -glm::vec3{0, 1, 0}},
  {Directions::Animal::Dorsal, glm::vec3{0, 1, 0}},
  {Directions::Animal::Caudal, -glm::vec3{0, 0, 1}},
  {Directions::Animal::Rostral, glm::vec3{0, 0, 1}}
};

const std::map<Directions::Anatomy, std::string> Directions::s_anatomicalAbbrevs = {
  {Directions::Anatomy::Right, "R"},
  {Directions::Anatomy::Left, "L"},
  {Directions::Anatomy::Anterior, "A"},
  {Directions::Anatomy::Posterior, "P"},
  {Directions::Anatomy::Inferior, "I"},
  {Directions::Anatomy::Superior, "S"}
};

const std::map<Directions::Animal, std::string> Directions::s_animalAbbrevs = {
  {Directions::Animal::Right, "R"},
  {Directions::Animal::Left, "L"},
  {Directions::Animal::Rostral, "Ros"},
  {Directions::Animal::Dorsal, "Dor"},
  {Directions::Animal::Caudal, "Cau"},
  {Directions::Animal::Ventral, "Ven"}
};

const std::map<Directions::Cartesian, std::string> Directions::s_cartesianAbbrevs = {
  {Directions::Cartesian::PosX, "+x"},
  {Directions::Cartesian::PosY, "+y"},
  {Directions::Cartesian::PosZ, "+z"},
  {Directions::Cartesian::NegX, "-x"},
  {Directions::Cartesian::NegY, "-y"},
  {Directions::Cartesian::NegZ, "-z"},
  {Directions::Cartesian::XY, "xy"},
  {Directions::Cartesian::YZ, "yz"},
  {Directions::Cartesian::ZX, "zx"},
  {Directions::Cartesian::XYZ, "xyz"}
};

glm::vec3 Directions::get(const Cartesian& dir)
{
  return s_cartesianDirections.at(dir);
}

glm::vec3 Directions::get(const View& dir)
{
  return s_viewDirections.at(dir);
}

glm::vec3 Directions::get(const Anatomy& dir)
{
  return s_anatomicalDirections.at(dir);
}

glm::vec3 Directions::get(const Animal& dir)
{
  return s_animalDirections.at(dir);
}

const std::string& Directions::abbrev(const Cartesian& dir)
{
  return s_cartesianAbbrevs.at(dir);
}

const std::string& Directions::abbrev(const Anatomy& dir)
{
  return s_anatomicalAbbrevs.at(dir);
}

const std::string& Directions::abbrev(const Animal& dir)
{
  return s_animalAbbrevs.at(dir);
}
