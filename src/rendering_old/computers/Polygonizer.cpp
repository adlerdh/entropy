#include "rendering_old/computers/Polygonizer.h"
#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering_old/ShaderNames.h"

#include "common/Exception.hpp"
#include "common/MathFuncs.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <fstream>
#include <iostream>
#include <vector>

namespace
{

/// The Marching Cubes triangle table. It stores vertex index list for generating
/// triangles in each of the 256 possible configurations of an iso-surface intersecting
/// (or not intersecting) a cube. This list contains 256 rows x 16 columns:
/// one row per cube configuration. The value 255 indicates no triangle.
///
/// @see http://paulbourke.net/geometry/polygonise/
static const std::vector<float> sk_triangleTable
  = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   8,   3,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   1,   9,   255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 1,   8,   3,   9,   8,   1,   255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 1,   2,   10,  255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 0,   8,   3,   1,   2,   10,  255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 9,   2,   10,  0,   2,   9,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 2,   8,
     3,   2,   10,  8,   10,  9,   8,   255, 255, 255, 255, 255, 255, 255, 3,   11,  2,   255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   11,  2,   8,   11,  0,   255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 1,   9,   0,   2,   3,   11,  255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 1,   11,  2,   1,   9,   11,  9,   8,   11,  255, 255, 255, 255, 255,
     255, 255, 3,   10,  1,   11,  10,  3,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,
     10,  1,   0,   8,   10,  8,   11,  10,  255, 255, 255, 255, 255, 255, 255, 3,   9,   0,   3,
     11,  9,   11,  10,  9,   255, 255, 255, 255, 255, 255, 255, 9,   8,   10,  10,  8,   11,  255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 4,   7,   8,   255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 4,   3,   0,   7,   3,   4,   255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 0,   1,   9,   8,   4,   7,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     4,   1,   9,   4,   7,   1,   7,   3,   1,   255, 255, 255, 255, 255, 255, 255, 1,   2,   10,
     8,   4,   7,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 3,   4,   7,   3,   0,   4,
     1,   2,   10,  255, 255, 255, 255, 255, 255, 255, 9,   2,   10,  9,   0,   2,   8,   4,   7,
     255, 255, 255, 255, 255, 255, 255, 2,   10,  9,   2,   9,   7,   2,   7,   3,   7,   9,   4,
     255, 255, 255, 255, 8,   4,   7,   3,   11,  2,   255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 11,  4,   7,   11,  2,   4,   2,   0,   4,   255, 255, 255, 255, 255, 255, 255, 9,   0,
     1,   8,   4,   7,   2,   3,   11,  255, 255, 255, 255, 255, 255, 255, 4,   7,   11,  9,   4,
     11,  9,   11,  2,   9,   2,   1,   255, 255, 255, 255, 3,   10,  1,   3,   11,  10,  7,   8,
     4,   255, 255, 255, 255, 255, 255, 255, 1,   11,  10,  1,   4,   11,  1,   0,   4,   7,   11,
     4,   255, 255, 255, 255, 4,   7,   8,   9,   0,   11,  9,   11,  10,  11,  0,   3,   255, 255,
     255, 255, 4,   7,   11,  4,   11,  9,   9,   11,  10,  255, 255, 255, 255, 255, 255, 255, 9,
     5,   4,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 9,   5,   4,   0,
     8,   3,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   5,   4,   1,   5,   0,   255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 8,   5,   4,   8,   3,   5,   3,   1,   5,   255,
     255, 255, 255, 255, 255, 255, 1,   2,   10,  9,   5,   4,   255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 3,   0,   8,   1,   2,   10,  4,   9,   5,   255, 255, 255, 255, 255, 255, 255,
     5,   2,   10,  5,   4,   2,   4,   0,   2,   255, 255, 255, 255, 255, 255, 255, 2,   10,  5,
     3,   2,   5,   3,   5,   4,   3,   4,   8,   255, 255, 255, 255, 9,   5,   4,   2,   3,   11,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   11,  2,   0,   8,   11,  4,   9,   5,
     255, 255, 255, 255, 255, 255, 255, 0,   5,   4,   0,   1,   5,   2,   3,   11,  255, 255, 255,
     255, 255, 255, 255, 2,   1,   5,   2,   5,   8,   2,   8,   11,  4,   8,   5,   255, 255, 255,
     255, 10,  3,   11,  10,  1,   3,   9,   5,   4,   255, 255, 255, 255, 255, 255, 255, 4,   9,
     5,   0,   8,   1,   8,   10,  1,   8,   11,  10,  255, 255, 255, 255, 5,   4,   0,   5,   0,
     11,  5,   11,  10,  11,  0,   3,   255, 255, 255, 255, 5,   4,   8,   5,   8,   10,  10,  8,
     11,  255, 255, 255, 255, 255, 255, 255, 9,   7,   8,   5,   7,   9,   255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 9,   3,   0,   9,   5,   3,   5,   7,   3,   255, 255, 255, 255, 255,
     255, 255, 0,   7,   8,   0,   1,   7,   1,   5,   7,   255, 255, 255, 255, 255, 255, 255, 1,
     5,   3,   3,   5,   7,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 9,   7,   8,   9,
     5,   7,   10,  1,   2,   255, 255, 255, 255, 255, 255, 255, 10,  1,   2,   9,   5,   0,   5,
     3,   0,   5,   7,   3,   255, 255, 255, 255, 8,   0,   2,   8,   2,   5,   8,   5,   7,   10,
     5,   2,   255, 255, 255, 255, 2,   10,  5,   2,   5,   3,   3,   5,   7,   255, 255, 255, 255,
     255, 255, 255, 7,   9,   5,   7,   8,   9,   3,   11,  2,   255, 255, 255, 255, 255, 255, 255,
     9,   5,   7,   9,   7,   2,   9,   2,   0,   2,   7,   11,  255, 255, 255, 255, 2,   3,   11,
     0,   1,   8,   1,   7,   8,   1,   5,   7,   255, 255, 255, 255, 11,  2,   1,   11,  1,   7,
     7,   1,   5,   255, 255, 255, 255, 255, 255, 255, 9,   5,   8,   8,   5,   7,   10,  1,   3,
     10,  3,   11,  255, 255, 255, 255, 5,   7,   0,   5,   0,   9,   7,   11,  0,   1,   0,   10,
     11,  10,  0,   255, 11,  10,  0,   11,  0,   3,   10,  5,   0,   8,   0,   7,   5,   7,   0,
     255, 11,  10,  5,   7,   11,  5,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 10,  6,
     5,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   8,   3,   5,   10,
     6,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 9,   0,   1,   5,   10,  6,   255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 1,   8,   3,   1,   9,   8,   5,   10,  6,   255, 255,
     255, 255, 255, 255, 255, 1,   6,   5,   2,   6,   1,   255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 1,   6,   5,   1,   2,   6,   3,   0,   8,   255, 255, 255, 255, 255, 255, 255, 9,
     6,   5,   9,   0,   6,   0,   2,   6,   255, 255, 255, 255, 255, 255, 255, 5,   9,   8,   5,
     8,   2,   5,   2,   6,   3,   2,   8,   255, 255, 255, 255, 2,   3,   11,  10,  6,   5,   255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 11,  0,   8,   11,  2,   0,   10,  6,   5,   255,
     255, 255, 255, 255, 255, 255, 0,   1,   9,   2,   3,   11,  5,   10,  6,   255, 255, 255, 255,
     255, 255, 255, 5,   10,  6,   1,   9,   2,   9,   11,  2,   9,   8,   11,  255, 255, 255, 255,
     6,   3,   11,  6,   5,   3,   5,   1,   3,   255, 255, 255, 255, 255, 255, 255, 0,   8,   11,
     0,   11,  5,   0,   5,   1,   5,   11,  6,   255, 255, 255, 255, 3,   11,  6,   0,   3,   6,
     0,   6,   5,   0,   5,   9,   255, 255, 255, 255, 6,   5,   9,   6,   9,   11,  11,  9,   8,
     255, 255, 255, 255, 255, 255, 255, 5,   10,  6,   4,   7,   8,   255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 4,   3,   0,   4,   7,   3,   6,   5,   10,  255, 255, 255, 255, 255, 255,
     255, 1,   9,   0,   5,   10,  6,   8,   4,   7,   255, 255, 255, 255, 255, 255, 255, 10,  6,
     5,   1,   9,   7,   1,   7,   3,   7,   9,   4,   255, 255, 255, 255, 6,   1,   2,   6,   5,
     1,   4,   7,   8,   255, 255, 255, 255, 255, 255, 255, 1,   2,   5,   5,   2,   6,   3,   0,
     4,   3,   4,   7,   255, 255, 255, 255, 8,   4,   7,   9,   0,   5,   0,   6,   5,   0,   2,
     6,   255, 255, 255, 255, 7,   3,   9,   7,   9,   4,   3,   2,   9,   5,   9,   6,   2,   6,
     9,   255, 3,   11,  2,   7,   8,   4,   10,  6,   5,   255, 255, 255, 255, 255, 255, 255, 5,
     10,  6,   4,   7,   2,   4,   2,   0,   2,   7,   11,  255, 255, 255, 255, 0,   1,   9,   4,
     7,   8,   2,   3,   11,  5,   10,  6,   255, 255, 255, 255, 9,   2,   1,   9,   11,  2,   9,
     4,   11,  7,   11,  4,   5,   10,  6,   255, 8,   4,   7,   3,   11,  5,   3,   5,   1,   5,
     11,  6,   255, 255, 255, 255, 5,   1,   11,  5,   11,  6,   1,   0,   11,  7,   11,  4,   0,
     4,   11,  255, 0,   5,   9,   0,   6,   5,   0,   3,   6,   11,  6,   3,   8,   4,   7,   255,
     6,   5,   9,   6,   9,   11,  4,   7,   9,   7,   11,  9,   255, 255, 255, 255, 10,  4,   9,
     6,   4,   10,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 4,   10,  6,   4,   9,   10,
     0,   8,   3,   255, 255, 255, 255, 255, 255, 255, 10,  0,   1,   10,  6,   0,   6,   4,   0,
     255, 255, 255, 255, 255, 255, 255, 8,   3,   1,   8,   1,   6,   8,   6,   4,   6,   1,   10,
     255, 255, 255, 255, 1,   4,   9,   1,   2,   4,   2,   6,   4,   255, 255, 255, 255, 255, 255,
     255, 3,   0,   8,   1,   2,   9,   2,   4,   9,   2,   6,   4,   255, 255, 255, 255, 0,   2,
     4,   4,   2,   6,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 8,   3,   2,   8,   2,
     4,   4,   2,   6,   255, 255, 255, 255, 255, 255, 255, 10,  4,   9,   10,  6,   4,   11,  2,
     3,   255, 255, 255, 255, 255, 255, 255, 0,   8,   2,   2,   8,   11,  4,   9,   10,  4,   10,
     6,   255, 255, 255, 255, 3,   11,  2,   0,   1,   6,   0,   6,   4,   6,   1,   10,  255, 255,
     255, 255, 6,   4,   1,   6,   1,   10,  4,   8,   1,   2,   1,   11,  8,   11,  1,   255, 9,
     6,   4,   9,   3,   6,   9,   1,   3,   11,  6,   3,   255, 255, 255, 255, 8,   11,  1,   8,
     1,   0,   11,  6,   1,   9,   1,   4,   6,   4,   1,   255, 3,   11,  6,   3,   6,   0,   0,
     6,   4,   255, 255, 255, 255, 255, 255, 255, 6,   4,   8,   11,  6,   8,   255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 7,   10,  6,   7,   8,   10,  8,   9,   10,  255, 255, 255, 255,
     255, 255, 255, 0,   7,   3,   0,   10,  7,   0,   9,   10,  6,   7,   10,  255, 255, 255, 255,
     10,  6,   7,   1,   10,  7,   1,   7,   8,   1,   8,   0,   255, 255, 255, 255, 10,  6,   7,
     10,  7,   1,   1,   7,   3,   255, 255, 255, 255, 255, 255, 255, 1,   2,   6,   1,   6,   8,
     1,   8,   9,   8,   6,   7,   255, 255, 255, 255, 2,   6,   9,   2,   9,   1,   6,   7,   9,
     0,   9,   3,   7,   3,   9,   255, 7,   8,   0,   7,   0,   6,   6,   0,   2,   255, 255, 255,
     255, 255, 255, 255, 7,   3,   2,   6,   7,   2,   255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 2,   3,   11,  10,  6,   8,   10,  8,   9,   8,   6,   7,   255, 255, 255, 255, 2,   0,
     7,   2,   7,   11,  0,   9,   7,   6,   7,   10,  9,   10,  7,   255, 1,   8,   0,   1,   7,
     8,   1,   10,  7,   6,   7,   10,  2,   3,   11,  255, 11,  2,   1,   11,  1,   7,   10,  6,
     1,   6,   7,   1,   255, 255, 255, 255, 8,   9,   6,   8,   6,   7,   9,   1,   6,   11,  6,
     3,   1,   3,   6,   255, 0,   9,   1,   11,  6,   7,   255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 7,   8,   0,   7,   0,   6,   3,   11,  0,   11,  6,   0,   255, 255, 255, 255, 7,
     11,  6,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 7,   6,   11,  255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 3,   0,   8,   11,  7,   6,   255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   1,   9,   11,  7,   6,   255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 8,   1,   9,   8,   3,   1,   11,  7,   6,   255, 255, 255, 255,
     255, 255, 255, 10,  1,   2,   6,   11,  7,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     1,   2,   10,  3,   0,   8,   6,   11,  7,   255, 255, 255, 255, 255, 255, 255, 2,   9,   0,
     2,   10,  9,   6,   11,  7,   255, 255, 255, 255, 255, 255, 255, 6,   11,  7,   2,   10,  3,
     10,  8,   3,   10,  9,   8,   255, 255, 255, 255, 7,   2,   3,   6,   2,   7,   255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 7,   0,   8,   7,   6,   0,   6,   2,   0,   255, 255, 255,
     255, 255, 255, 255, 2,   7,   6,   2,   3,   7,   0,   1,   9,   255, 255, 255, 255, 255, 255,
     255, 1,   6,   2,   1,   8,   6,   1,   9,   8,   8,   7,   6,   255, 255, 255, 255, 10,  7,
     6,   10,  1,   7,   1,   3,   7,   255, 255, 255, 255, 255, 255, 255, 10,  7,   6,   1,   7,
     10,  1,   8,   7,   1,   0,   8,   255, 255, 255, 255, 0,   3,   7,   0,   7,   10,  0,   10,
     9,   6,   10,  7,   255, 255, 255, 255, 7,   6,   10,  7,   10,  8,   8,   10,  9,   255, 255,
     255, 255, 255, 255, 255, 6,   8,   4,   11,  8,   6,   255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 3,   6,   11,  3,   0,   6,   0,   4,   6,   255, 255, 255, 255, 255, 255, 255, 8,
     6,   11,  8,   4,   6,   9,   0,   1,   255, 255, 255, 255, 255, 255, 255, 9,   4,   6,   9,
     6,   3,   9,   3,   1,   11,  3,   6,   255, 255, 255, 255, 6,   8,   4,   6,   11,  8,   2,
     10,  1,   255, 255, 255, 255, 255, 255, 255, 1,   2,   10,  3,   0,   11,  0,   6,   11,  0,
     4,   6,   255, 255, 255, 255, 4,   11,  8,   4,   6,   11,  0,   2,   9,   2,   10,  9,   255,
     255, 255, 255, 10,  9,   3,   10,  3,   2,   9,   4,   3,   11,  3,   6,   4,   6,   3,   255,
     8,   2,   3,   8,   4,   2,   4,   6,   2,   255, 255, 255, 255, 255, 255, 255, 0,   4,   2,
     4,   6,   2,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 1,   9,   0,   2,   3,   4,
     2,   4,   6,   4,   3,   8,   255, 255, 255, 255, 1,   9,   4,   1,   4,   2,   2,   4,   6,
     255, 255, 255, 255, 255, 255, 255, 8,   1,   3,   8,   6,   1,   8,   4,   6,   6,   10,  1,
     255, 255, 255, 255, 10,  1,   0,   10,  0,   6,   6,   0,   4,   255, 255, 255, 255, 255, 255,
     255, 4,   6,   3,   4,   3,   8,   6,   10,  3,   0,   3,   9,   10,  9,   3,   255, 10,  9,
     4,   6,   10,  4,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 4,   9,   5,   7,   6,
     11,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   8,   3,   4,   9,   5,   11,  7,
     6,   255, 255, 255, 255, 255, 255, 255, 5,   0,   1,   5,   4,   0,   7,   6,   11,  255, 255,
     255, 255, 255, 255, 255, 11,  7,   6,   8,   3,   4,   3,   5,   4,   3,   1,   5,   255, 255,
     255, 255, 9,   5,   4,   10,  1,   2,   7,   6,   11,  255, 255, 255, 255, 255, 255, 255, 6,
     11,  7,   1,   2,   10,  0,   8,   3,   4,   9,   5,   255, 255, 255, 255, 7,   6,   11,  5,
     4,   10,  4,   2,   10,  4,   0,   2,   255, 255, 255, 255, 3,   4,   8,   3,   5,   4,   3,
     2,   5,   10,  5,   2,   11,  7,   6,   255, 7,   2,   3,   7,   6,   2,   5,   4,   9,   255,
     255, 255, 255, 255, 255, 255, 9,   5,   4,   0,   8,   6,   0,   6,   2,   6,   8,   7,   255,
     255, 255, 255, 3,   6,   2,   3,   7,   6,   1,   5,   0,   5,   4,   0,   255, 255, 255, 255,
     6,   2,   8,   6,   8,   7,   2,   1,   8,   4,   8,   5,   1,   5,   8,   255, 9,   5,   4,
     10,  1,   6,   1,   7,   6,   1,   3,   7,   255, 255, 255, 255, 1,   6,   10,  1,   7,   6,
     1,   0,   7,   8,   7,   0,   9,   5,   4,   255, 4,   0,   10,  4,   10,  5,   0,   3,   10,
     6,   10,  7,   3,   7,   10,  255, 7,   6,   10,  7,   10,  8,   5,   4,   10,  4,   8,   10,
     255, 255, 255, 255, 6,   9,   5,   6,   11,  9,   11,  8,   9,   255, 255, 255, 255, 255, 255,
     255, 3,   6,   11,  0,   6,   3,   0,   5,   6,   0,   9,   5,   255, 255, 255, 255, 0,   11,
     8,   0,   5,   11,  0,   1,   5,   5,   6,   11,  255, 255, 255, 255, 6,   11,  3,   6,   3,
     5,   5,   3,   1,   255, 255, 255, 255, 255, 255, 255, 1,   2,   10,  9,   5,   11,  9,   11,
     8,   11,  5,   6,   255, 255, 255, 255, 0,   11,  3,   0,   6,   11,  0,   9,   6,   5,   6,
     9,   1,   2,   10,  255, 11,  8,   5,   11,  5,   6,   8,   0,   5,   10,  5,   2,   0,   2,
     5,   255, 6,   11,  3,   6,   3,   5,   2,   10,  3,   10,  5,   3,   255, 255, 255, 255, 5,
     8,   9,   5,   2,   8,   5,   6,   2,   3,   8,   2,   255, 255, 255, 255, 9,   5,   6,   9,
     6,   0,   0,   6,   2,   255, 255, 255, 255, 255, 255, 255, 1,   5,   8,   1,   8,   0,   5,
     6,   8,   3,   8,   2,   6,   2,   8,   255, 1,   5,   6,   2,   1,   6,   255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 1,   3,   6,   1,   6,   10,  3,   8,   6,   5,   6,   9,   8,
     9,   6,   255, 10,  1,   0,   10,  0,   6,   9,   5,   0,   5,   6,   0,   255, 255, 255, 255,
     0,   3,   8,   5,   6,   10,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 10,  5,   6,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 11,  5,   10,  7,   5,   11,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 11,  5,   10,  11,  7,   5,   8,   3,   0,
     255, 255, 255, 255, 255, 255, 255, 5,   11,  7,   5,   10,  11,  1,   9,   0,   255, 255, 255,
     255, 255, 255, 255, 10,  7,   5,   10,  11,  7,   9,   8,   1,   8,   3,   1,   255, 255, 255,
     255, 11,  1,   2,   11,  7,   1,   7,   5,   1,   255, 255, 255, 255, 255, 255, 255, 0,   8,
     3,   1,   2,   7,   1,   7,   5,   7,   2,   11,  255, 255, 255, 255, 9,   7,   5,   9,   2,
     7,   9,   0,   2,   2,   11,  7,   255, 255, 255, 255, 7,   5,   2,   7,   2,   11,  5,   9,
     2,   3,   2,   8,   9,   8,   2,   255, 2,   5,   10,  2,   3,   5,   3,   7,   5,   255, 255,
     255, 255, 255, 255, 255, 8,   2,   0,   8,   5,   2,   8,   7,   5,   10,  2,   5,   255, 255,
     255, 255, 9,   0,   1,   5,   10,  3,   5,   3,   7,   3,   10,  2,   255, 255, 255, 255, 9,
     8,   2,   9,   2,   1,   8,   7,   2,   10,  2,   5,   7,   5,   2,   255, 1,   3,   5,   3,
     7,   5,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   8,   7,   0,   7,   1,   1,
     7,   5,   255, 255, 255, 255, 255, 255, 255, 9,   0,   3,   9,   3,   5,   5,   3,   7,   255,
     255, 255, 255, 255, 255, 255, 9,   8,   7,   5,   9,   7,   255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 5,   8,   4,   5,   10,  8,   10,  11,  8,   255, 255, 255, 255, 255, 255, 255,
     5,   0,   4,   5,   11,  0,   5,   10,  11,  11,  3,   0,   255, 255, 255, 255, 0,   1,   9,
     8,   4,   10,  8,   10,  11,  10,  4,   5,   255, 255, 255, 255, 10,  11,  4,   10,  4,   5,
     11,  3,   4,   9,   4,   1,   3,   1,   4,   255, 2,   5,   1,   2,   8,   5,   2,   11,  8,
     4,   5,   8,   255, 255, 255, 255, 0,   4,   11,  0,   11,  3,   4,   5,   11,  2,   11,  1,
     5,   1,   11,  255, 0,   2,   5,   0,   5,   9,   2,   11,  5,   4,   5,   8,   11,  8,   5,
     255, 9,   4,   5,   2,   11,  3,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 2,   5,
     10,  3,   5,   2,   3,   4,   5,   3,   8,   4,   255, 255, 255, 255, 5,   10,  2,   5,   2,
     4,   4,   2,   0,   255, 255, 255, 255, 255, 255, 255, 3,   10,  2,   3,   5,   10,  3,   8,
     5,   4,   5,   8,   0,   1,   9,   255, 5,   10,  2,   5,   2,   4,   1,   9,   2,   9,   4,
     2,   255, 255, 255, 255, 8,   4,   5,   8,   5,   3,   3,   5,   1,   255, 255, 255, 255, 255,
     255, 255, 0,   4,   5,   1,   0,   5,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 8,
     4,   5,   8,   5,   3,   9,   0,   5,   0,   3,   5,   255, 255, 255, 255, 9,   4,   5,   255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 4,   11,  7,   4,   9,   11,  9,
     10,  11,  255, 255, 255, 255, 255, 255, 255, 0,   8,   3,   4,   9,   7,   9,   11,  7,   9,
     10,  11,  255, 255, 255, 255, 1,   10,  11,  1,   11,  4,   1,   4,   0,   7,   4,   11,  255,
     255, 255, 255, 3,   1,   4,   3,   4,   8,   1,   10,  4,   7,   4,   11,  10,  11,  4,   255,
     4,   11,  7,   9,   11,  4,   9,   2,   11,  9,   1,   2,   255, 255, 255, 255, 9,   7,   4,
     9,   11,  7,   9,   1,   11,  2,   11,  1,   0,   8,   3,   255, 11,  7,   4,   11,  4,   2,
     2,   4,   0,   255, 255, 255, 255, 255, 255, 255, 11,  7,   4,   11,  4,   2,   8,   3,   4,
     3,   2,   4,   255, 255, 255, 255, 2,   9,   10,  2,   7,   9,   2,   3,   7,   7,   4,   9,
     255, 255, 255, 255, 9,   10,  7,   9,   7,   4,   10,  2,   7,   8,   7,   0,   2,   0,   7,
     255, 3,   7,   10,  3,   10,  2,   7,   4,   10,  1,   10,  0,   4,   0,   10,  255, 1,   10,
     2,   8,   7,   4,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 4,   9,   1,   4,   1,
     7,   7,   1,   3,   255, 255, 255, 255, 255, 255, 255, 4,   9,   1,   4,   1,   7,   0,   8,
     1,   8,   7,   1,   255, 255, 255, 255, 4,   0,   3,   7,   4,   3,   255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 4,   8,   7,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 9,   10,  8,   10,  11,  8,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 3,
     0,   9,   3,   9,   11,  11,  9,   10,  255, 255, 255, 255, 255, 255, 255, 0,   1,   10,  0,
     10,  8,   8,   10,  11,  255, 255, 255, 255, 255, 255, 255, 3,   1,   10,  11,  3,   10,  255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 1,   2,   11,  1,   11,  9,   9,   11,  8,   255,
     255, 255, 255, 255, 255, 255, 3,   0,   9,   3,   9,   11,  1,   2,   9,   2,   11,  9,   255,
     255, 255, 255, 0,   2,   11,  8,   0,   11,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     3,   2,   11,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 2,   3,   8,
     2,   8,   10,  10,  8,   9,   255, 255, 255, 255, 255, 255, 255, 9,   10,  2,   0,   9,   2,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 2,   3,   8,   2,   8,   10,  0,   1,   8,
     1,   10,  8,   255, 255, 255, 255, 1,   10,  2,   255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 1,   3,   8,   9,   1,   8,   255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 0,   9,   1,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   3,
     8,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

// In Marching Cubes, a maximum of five triangles can be generated per cube.
//static constexpr size_t sk_maxNumTrianglesPerCube = 5;

// Generally, far fewer triangles are generated per cube.
static constexpr size_t sk_expectedNumTrianglesPerCube = 1;

} // namespace

/**
 No packing or normalization is performed by transform feedback.
 We will manually pack bits into unsigned integers.
 */

Polygonizer::Polygonizer(
  ShaderProgramActivatorType shaderProgramActivator, UniformsProviderType uniformsProvider
)
  : ComputerBase()
  ,

  m_name("Polygonizer")
  , m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_vao()
  , m_vaoParams(nullptr)
  ,

  m_indicesInfo(nullptr)
  , m_positionsInfo(nullptr)
  , m_indicesObject(nullptr)
  , m_positionsObject(nullptr)
  , m_txFeedbackObject(nullptr)
  ,

  m_uniforms()
  ,

  //      m_scalarVolumeTex( nullptr ),
  m_volumeTexture()
  , m_isoValue(0.0f)
  ,

  m_triTableBufferTex(nullptr)
  ,

  m_cubeIndices()
  , m_cubeCorners()
  ,

  m_feedbackTriangles()
{
  std::cout << "START CREATING TRIANGLE TEX!!!" << std::endl;
  createTriangleTableTexture();
  std::cout << "END CREATING TRIANGLE TEX!!!" << std::endl;
}

Polygonizer::~Polygonizer() = default;

void Polygonizer::initialize()
{
  auto texture = m_volumeTexture.lock();

  if (!texture)
  {
    return;
  }

  auto cubeCount = texture->size() - glm::uvec3{1, 1, 1};

  PositionIndexType index = 0;

  for (uint32_t k = 0; k < cubeCount.z; ++k)
  {
    for (uint32_t j = 0; j < cubeCount.y; ++j)
    {
      for (uint32_t i = 0; i < cubeCount.x; ++i)
      {
        m_cubeIndices.push_back(index++);
        m_cubeCorners.emplace_back(glm::vec3{i, j, k});
      }
    }
  }

  m_indicesInfo = std::make_unique<
    VertexIndicesInfo>(IndexType::UInt32, PrimitiveMode::Points, m_cubeIndices.size(), 0);

  m_indicesObject
    = std::make_unique<GLBufferObject>(BufferType::Index, BufferUsagePattern::StaticDraw);

  m_indicesObject->generate();

  m_indicesObject->allocate(m_cubeIndices.size() * sizeof(PositionIndexType), m_cubeIndices.data());

  m_positionsInfo = std::make_unique<VertexAttributeInfo>(
    BufferComponentType::Float,
    BufferNormalizeValues::False,
    3,
    sizeof(PositionType),
    0,
    m_cubeCorners.size()
  );

  m_positionsObject
    = std::make_unique<GLBufferObject>(BufferType::VertexArray, BufferUsagePattern::StaticDraw);

  m_positionsObject->generate();

  m_positionsObject->allocate(m_cubeCorners.size() * sizeof(PositionType), m_cubeCorners.data());

  //    GLuint val = 0;
  //    val = val | (w << 30);
  //    val = val | (z << 20);
  //    val = val | (y << 10);
  //    val = val | (x << 0);

  /**
    If you are passing colors, then more likely than not you want UNSIGNED_INT_2_10_10_10_REV.
    So I'll pretend that's what you asked for.

    The entire field is packed into a single unsigned integer. OpenGL defines an unsigned integer
    to be exactly 32-bits in size, so you should use the GLuint typedef that OpenGL provides to get one.

    However, this format is packed, so you can't just shove an array into it. It's a form of compression,
    so it's generally going to be used when you're serious about making your vertex attribute data small.

    First, the only reason to use 10/10/10/2 for colors is if your original source colors have higher
    color-depth colors than, for example, the regular 8-bit-per-channel colors. After all, the RGB components
    will each have 10 bits; if your source colors only had 8 bits, you haven't actually gained any new information.

    So let's say you have some floating-point color values, with floating-point precision.
    You have to normalize each component down to the [0, 1] range. Then you multiply each value by 210 - 1,
    thus expanding to the [0, 1023] range. Then you convert each component into integers.

    The problem comes with packing it into the actual field. The "REV" in the name means that the order
    of components is revered. Normally in OpenGL, if you see a format like GL_UNSIGNED_SHORT_5_6_5,
    then this means that each unsigned short is broken down into a pattern of 5 bits, followed by 6 bits,
    followed by 5 more. The left-most 5-bit field is the Red, the 6-bit field is Green, and the right-most
    5-bit field is Blue.

    The "REV" means to reverse this. So 2_10_10_10_REV means that the two most significant bits go to
    the alpha. The next 10 bits go to Blue. Then Green. Then Red.

    So you have to bundle your integers on the [0, 1023] range, and then shove them into the right
    place in the GLuint value. And you do that for every such component in the array.
    **/

  //    glm::packSnorm4x8(u);
  //    using NormalType = uint32_t;
  //    VertexAttributeInfo normalsInfo(
  //                BufferComponentType::Int_2_10_10_10,
  //                BufferNormalizeValues::True,
  //                4, sizeof( NormalType ),
  //                sk_offset, sk_numVerts );

  // Create transform feedback buffer
  m_txFeedbackObject
    = std::make_unique<GLBufferObject>(BufferType::VertexArray, BufferUsagePattern::StaticRead);

  m_txFeedbackObject->generate();

  // Allocate space for all vertices and normal vectors generated.
  // There are three vertices and normals per output triangle.
  m_txFeedbackObject->allocate(2000000 * 3 * 2 * sizeof(PositionType), nullptr);
  //                sk_expectedNumTrianglesPerCube * m_cubeCorners.size() *
  //                3 * 2 * sizeof( PositionType ), nullptr );

  m_vao.generate();

  m_vao.bind();
  {
    // "inPosition"
    static constexpr GLuint sk_positionIndex = 0;

    // Bind EBO so that it is part of the VAO state
    m_indicesObject->bind();

    // Saves binding in VAO, since GL_ARRAY_BUFFER is not part of VAO state.
    // Register position VBO with VAO and set/enable attribute pointer
    m_positionsObject->bind();

    m_vao.setAttributeBuffer(sk_positionIndex, *m_positionsInfo);
    m_vao.enableVertexAttribute(sk_positionIndex);
  }
  m_vao.release();

  m_vaoParams = std::make_unique<GLVertexArrayObject::IndexedDrawParams>(*m_indicesInfo);

  m_feedbackTriangles.resize(sk_expectedNumTrianglesPerCube * m_cubeCorners.size() * 3 * 2);

  CHECK_GL_ERROR(m_errorChecker);
}

void Polygonizer::execute()
{
  static const Uniforms::SamplerIndexType sk_tex3DUnit{0};
  static const Uniforms::SamplerIndexType sk_triTableUnit{1};

  if (!m_triTableBufferTex)
  {
    return;
  }

  auto texture = m_volumeTexture.lock();

  if (!texture)
  {
    return;
  }

  if (!m_shaderProgramActivator)
  {
    throw_debug("Unable to access ShaderProgramActivator");
  }

  if (!m_indicesInfo || !m_indicesObject || !m_positionsInfo || !m_positionsObject || !m_txFeedbackObject || !m_vaoParams)
  {
    return;
  }

  Uniforms uniforms = m_uniformsProvider(PolygonizerProgram::name);

  GLShaderProgram* shaderProgram = m_shaderProgramActivator(PolygonizerProgram::name);

  //    shaderProgram->stopUse();

  //    const GLchar* feedbackVaryings[] = { "outPosition", "outNormal" };

  //    glTransformFeedbackVaryings( shaderProgram->handle(), 2,
  //                                 feedbackVaryings, GL_INTERLEAVED_ATTRIBS );

  //    shaderProgram->link();
  //    shaderProgram->use();

  if (!shaderProgram)
  {
    throw_debug("Null shader program");
  }

  if (!m_vaoParams)
  {
    std::ostringstream ss;
    ss << "Null VAO parameters in " << m_name << std::ends;
    throw_debug(ss.str());
  }

  // Transformation from image to texture coordinates
  const glm::mat4 texture_O_image = math::computeImagePixelToTextureTransformation(texture->size());

  // Vector from cube corner to cube corner, in texture coordinates
  const glm::vec3 step = glm::mat3{texture_O_image} * glm::vec3{1.0f, 1.0f, 1.0f};

  const std::array<glm::vec3, 8> vertDecals = {
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{step.x, 0.0f, 0.0f},
    glm::vec3{step.x, step.y, 0.0f},
    glm::vec3{0.0f, step.y, 0.0f},
    glm::vec3{0.0f, 0.0f, step.z},
    glm::vec3{step.x, 0.0f, step.z},
    glm::vec3{step.x, step.y, step.z},
    glm::vec3{0.0f, step.y, step.z}
  };

  // Disable face culling, so that both front- and back-facing polygons are rendered
  glDisable(GL_CULL_FACE);

  // Bind the buffer object for transformation feedback
  m_txFeedbackObject->bind();

  // Generate object to query the number of primitives written
  GLuint query;
  glGenQueries(1, &query);

  // Perform feedback transform
  glEnable(GL_RASTERIZER_DISCARD);

  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, m_txFeedbackObject->id());

  glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, query);
  {
    glBeginTransformFeedback(GL_TRIANGLES);
    {
      using namespace PolygonizerProgram;

      // Bind textures
      texture->bind(0);
      m_triTableBufferTex->bind(1);

      // Set uniforms and samplers
      uniforms.setValue(vert::tex_O_image, texture_O_image);
      uniforms.setValue(geom::tex3D, sk_tex3DUnit);
      uniforms.setValue(geom::triTableTex, sk_triTableUnit);
      uniforms.setValue(geom::isolevel, m_isoValue);
      uniforms.setValue(geom::vertDecals, vertDecals);
      uniforms.setValue(geom::gradDeltas, glm::mat3{texture_O_image});
      uniforms.setValue(geom::world_O_tex, glm::inverse(texture_O_image));

      texture->bind(sk_tex3DUnit.index);
      m_triTableBufferTex->bind(sk_triTableUnit.index);

      shaderProgram->applyUniforms(uniforms);

      m_vao.bind();
      {
        std::cout << "START!!!!!!!!!!!" << std::endl;
        m_vao.drawElements(*m_vaoParams);
      }
      m_vao.release();
    }
    glEndTransformFeedback();
  }
  glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

  glDisable(GL_RASTERIZER_DISCARD);

  glFlush();

  // Fetch and print results
  GLuint primitives;
  glGetQueryObjectuiv(query, GL_QUERY_RESULT, &primitives);

  glGetBufferSubData(
    GL_TRANSFORM_FEEDBACK_BUFFER,
    0,
    primitives * 3 * 2 * sizeof(glm::vec3),
    m_feedbackTriangles.data()
  );

  std::cout << __FUNCTION__ << " : " << __LINE__ << std::endl;

  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
            << std::endl;
  std::cout << "Read back " << primitives << " triangles:" << std::endl << std::endl;

#if 0
        for ( uint i = 0; i < primitives; ++i )
        {
            // print i'th triangle:
            // position                                                            normal
            std::cout << glm::to_string( m_feedbackTriangles[6*i + 0] ) << "\t" << glm::to_string( m_feedbackTriangles[6*i + 1] ) << std::endl;
            std::cout << glm::to_string( m_feedbackTriangles[6*i + 2] ) << "\t" << glm::to_string( m_feedbackTriangles[6*i + 3] ) << std::endl;
            std::cout << glm::to_string( m_feedbackTriangles[6*i + 4] ) << "\t" << glm::to_string( m_feedbackTriangles[6*i + 5] ) << std::endl;
            std::cout << std::endl;
        }
#endif

  std::cout << "Read back " << primitives << " triangles:" << std::endl << std::endl;
  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
            << std::endl;

  glDeleteQueries(1, &query);

  CHECK_GL_ERROR(m_errorChecker);
}

void Polygonizer::setVolumeTexture(std::weak_ptr<GLTexture> texture)
{
  m_volumeTexture = texture;

  std::cout << "START INITIALIZATION!!!" << std::endl;
  initialize();
  std::cout << "DONE INITIALIZATION!!!" << std::endl;

  // Grab internal format of texture
  // If it is not scalar, then warn that only Red component will be used.
}

void Polygonizer::setIsoValue(float value)
{
  m_isoValue = value;
}

void Polygonizer::createTriangleTableTexture()
{
  static constexpr GLint sk_alignment = 1;
  static const tex::WrapMode sk_wrapMode = tex::WrapMode::ClampToEdge;

  GLTexture::PixelStoreSettings pixelPackSettings;
  pixelPackSettings.m_alignment = sk_alignment;

  GLTexture::PixelStoreSettings pixelUnpackSettings = pixelPackSettings;

  // Create triangle table buffer texture

  //    m_triTableBufferTex = std::make_unique<GLBufferTexture>(
  //                tex::SizedInternalBufferTextureFormat::R32F,
  //                BufferUsagePattern::StaticDraw );

  //    m_triTableBufferTex->generate();
  //    m_triTableBufferTex->allocate( sk_triangleTable.size() * sizeof(float),
  //                                   sk_triangleTable.data() );

  //    m_triTableBufferTex->attachBufferToTexture();

  m_triTableBufferTex = std::make_unique<GLTexture>(
    tex::Target::Texture3D, GLTexture::MultisampleSettings(), pixelPackSettings, pixelUnpackSettings
  );

  m_triTableBufferTex->generate();
  m_triTableBufferTex->setMinificationFilter(tex::MinificationFilter::Nearest);
  m_triTableBufferTex->setMagnificationFilter(tex::MagnificationFilter::Nearest);
  m_triTableBufferTex->setWrapMode(sk_wrapMode);
  m_triTableBufferTex->setSize(glm::uvec3{16, 256, 1});
  m_triTableBufferTex->setAutoGenerateMipmaps(false);

  m_triTableBufferTex->setData(
    0,
    tex::SizedInternalFormat::R32F,
    tex::BufferPixelFormat::Red,
    tex::BufferPixelDataType::Float32,
    sk_triangleTable.data()
  );
}
