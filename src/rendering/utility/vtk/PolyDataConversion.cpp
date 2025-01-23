#include "rendering/utility/vtk/PolyDataConversion.h"

#include "common/Exception.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <vtkArrayDispatch.h>
#include <vtkAssume.h>
#include <vtkCellData.h>
#include <vtkDataArrayAccessor.h>
#include <vtkIdTypeArray.h>
#include <vtkPointData.h>

#include <cmath>
#include <type_traits>

#define DEBUG_PRINT 0

namespace vtkconvert
{

namespace details
{

template<typename ToType>
class PointsArrayPacker : public VectorArrayBuffer<ToType>
{
public:
  PointsArrayPacker()
    : VectorArrayBuffer<ToType>()
  {
    if (!std::is_same<ToType, float>::value)
    {
      throw_debug("PointsArrayPacker can only convert to float type.");
    }
  }

  template<class FromVectorArrayType>
  void operator()(FromVectorArrayType* points)
  {
    if (!points)
      return;

    VTK_ASSUME(points->GetNumberOfComponents() == 3);

    m_vectorCount = static_cast<size_t>(points->GetNumberOfTuples()); // Num points
    m_bufferLength = 3 * m_vectorCount;                               // Num elements in buffer
    m_bufferByteCount = static_cast<int64_t>(sizeof(ToType))
                        * m_bufferLength; // Num bytes in buffer
    m_buffer = std::make_unique<ToType[]>(static_cast<size_t>(m_bufferLength));

    vtkDataArrayAccessor<FromVectorArrayType> v(points);

#if (DEBUG_PRINT)
    std::cout << "....................Points........................." << std::endl;
#endif

    for (size_t i = 0; i < m_vectorCount; ++i)
    {
      m_buffer[3 * i + 0] = static_cast<float>(v.Get(i, 0));
      m_buffer[3 * i + 1] = static_cast<float>(v.Get(i, 1));
      m_buffer[3 * i + 2] = static_cast<float>(v.Get(i, 2));

#if (DEBUG_PRINT)
      std::cout << i << " : " << m_buffer[3 * i + 0] << ", " << m_buffer[3 * i + 1] << ", "
                << m_buffer[3 * i + 2] << std::endl;
#endif
    }

#if (DEBUG_PRINT)
    std::cout << "............................................." << std::endl;
#endif
  }

private:
  using VectorArrayBuffer<ToType>::m_vectorCount;
  using VectorArrayBuffer<ToType>::m_bufferLength;
  using VectorArrayBuffer<ToType>::m_bufferByteCount;
  using VectorArrayBuffer<ToType>::m_buffer;
};

template<typename ToType>
class NormalsArrayPacker : public VectorArrayBuffer<ToType>
{
public:
  NormalsArrayPacker()
    : VectorArrayBuffer<ToType>()
  {
    if (!std::is_same<ToType, uint32_t>::value)
    {
      throw_debug("NormalsArrayPacker can only convert to uint32_t type.");
    }
  }

  template<class FromVectorArrayType>
  void operator()(FromVectorArrayType* normals)
  {
    if (!normals)
      return;

    VTK_ASSUME(normals->GetNumberOfComponents() == 3);

    m_vectorCount = static_cast<size_t>(normals->GetNumberOfTuples()); // Num vectors
    m_bufferLength = m_vectorCount;                                    // Num coordinates in buffer
    m_bufferByteCount = static_cast<int64_t>(sizeof(ToType))
                        * m_bufferLength; // Num bytes in buffer
    m_buffer = std::make_unique<ToType[]>(static_cast<size_t>(m_bufferLength));

    vtkDataArrayAccessor<FromVectorArrayType> v(normals);

#if (DEBUG_PRINT)
    std::cout << "....................Normals........................." << std::endl;
#endif

    for (size_t i = 0; i < m_vectorCount; ++i)
    {
      const glm::vec3
        n(static_cast<float>(v.Get(i, 0)),
          static_cast<float>(v.Get(i, 1)),
          static_cast<float>(v.Get(i, 2)));

      m_buffer[i] = glm::packSnorm3x10_1x2(glm::vec4{glm::clamp(n, -1.0f, 1.0f), 0.0f});

#if (DEBUG_PRINT)
      std::cout << i << " : " glm::to_string(n) << std::endl;
#endif
    }

#if (DEBUG_PRINT)
    std::cout << "............................................." << std::endl;
#endif
  }

private:
  using VectorArrayBuffer<ToType>::m_vectorCount;
  using VectorArrayBuffer<ToType>::m_bufferLength;
  using VectorArrayBuffer<ToType>::m_bufferByteCount;
  using VectorArrayBuffer<ToType>::m_buffer;
};

template<typename ToType>
class TCoordsArrayPacker : public VectorArrayBuffer<ToType>
{
public:
  TCoordsArrayPacker()
    : VectorArrayBuffer<ToType>()
  {
    if (!std::is_same<ToType, uint32_t>::value)
    {
      throw_debug("TCoordsArrayPacker can only convert to uint32_t type.");
    }
  }

  template<class FromVectorArrayType>
  void operator()(FromVectorArrayType* tcoords)
  {
    if (!tcoords)
      return;

    VTK_ASSUME(tcoords->GetNumberOfComponents() == 2);

    m_vectorCount = static_cast<size_t>(tcoords->GetNumberOfTuples()); // Num vectors
    m_bufferLength = m_vectorCount;                                    // Num coordinates in buffer
    m_bufferByteCount = static_cast<int64_t>(sizeof(ToType))
                        * m_bufferLength; // Num bytes in buffer
    m_buffer = std::make_unique<ToType[]>(static_cast<size_t>(m_bufferLength));

    vtkDataArrayAccessor<FromVectorArrayType> v(tcoords);

#if (DEBUG_PRINT)
    std::cout << "....................TexCoords........................." << std::endl;
#endif

    for (size_t i = 0; i < m_vectorCount; ++i)
    {
      const glm::vec2 t(static_cast<float>(v.Get(i, 0)), static_cast<float>(v.Get(i, 1)));

      m_buffer[i] = glm::packUnorm2x16(glm::clamp(t, 0.0f, 1.0f));

#if (DEBUG_PRINT)
      std::cout << i << " : " << glm::to_string(t) << std::endl;
#endif
    }

#if (DEBUG_PRINT)
    std::cout << "............................................." << std::endl;
#endif
  }

private:
  using VectorArrayBuffer<ToType>::m_vectorCount;
  using VectorArrayBuffer<ToType>::m_bufferLength;
  using VectorArrayBuffer<ToType>::m_bufferByteCount;
  using VectorArrayBuffer<ToType>::m_buffer;
};

template<typename ToType>
class TCoordsFloatArrayPacker : public VectorArrayBuffer<ToType>
{
public:
  TCoordsFloatArrayPacker()
    : VectorArrayBuffer<ToType>()
  {
    if (!std::is_same<ToType, float>::value)
    {
      throw_debug("TCoordsFloatArrayPacker can only convert to float type.");
    }
  }

  template<class FromVectorArrayType>
  void operator()(FromVectorArrayType* tcoords)
  {
    if (!tcoords)
      return;

    VTK_ASSUME(tcoords->GetNumberOfComponents() == 2);

    m_vectorCount = static_cast<size_t>(tcoords->GetNumberOfTuples()); // Num vectors
    m_bufferLength = 2 * m_vectorCount;                                // Num coordinates in buffer
    m_bufferByteCount = static_cast<int64_t>(sizeof(ToType))
                        * m_bufferLength; // Num bytes in buffer
    m_buffer = std::make_unique<ToType[]>(static_cast<size_t>(m_bufferLength));

    vtkDataArrayAccessor<FromVectorArrayType> v(tcoords);

#if (DEBUG_PRINT)
    std::cout << "....................TexCoords........................." << std::endl;
#endif

    for (size_t i = 0; i < m_vectorCount; ++i)
    {
      m_buffer[2 * i + 0] = static_cast<float>(v.Get(i, 0));
      m_buffer[2 * i + 1] = static_cast<float>(v.Get(i, 1));

#if (DEBUG_PRINT)
      std::cout << i << " : " << static_cast<float>(v.Get(i, 0)) << ", "
                << static_cast<float>(v.Get(i, 1)) << std::endl;
#endif
    }

#if (DEBUG_PRINT)
    std::cout << "............................................." << std::endl;
#endif
  }

private:
  using VectorArrayBuffer<ToType>::m_vectorCount;
  using VectorArrayBuffer<ToType>::m_bufferLength;
  using VectorArrayBuffer<ToType>::m_bufferByteCount;
  using VectorArrayBuffer<ToType>::m_buffer;
};

template<typename ToType>
class IndicesArrayPacker : public VectorArrayBuffer<ToType>
{
public:
  IndicesArrayPacker()
    : VectorArrayBuffer<ToType>()
  {
    if (!std::is_same<ToType, uint32_t>::value)
    {
      throw_debug("IndicesArrayPacker can only convert to float type.");
    }
  }

  template<class FromVectorArrayType>
  void operator()(FromVectorArrayType* indices)
  {
    if (!indices)
      return;

    VTK_ASSUME(indices->GetNumberOfComponents() == 1);

    m_vectorCount = static_cast<size_t>(indices->GetNumberOfValues()) / 4; // Num triangles
    m_bufferLength = 3 * m_vectorCount; // Num indices (three per triangle) in buffer
    m_bufferByteCount = static_cast<int64_t>(sizeof(ToType))
                        * m_bufferLength; // Num bytes in buffer
    m_buffer = std::make_unique<ToType[]>(static_cast<size_t>(m_bufferLength));

    vtkDataArrayAccessor<FromVectorArrayType> v(indices);

#if (DEBUG_PRINT)
    std::cout << "....................Indices........................." << std::endl;
#endif

    for (size_t i = 0; i < m_vectorCount; ++i)
    {
      m_buffer[3 * i + 0] = static_cast<uint32_t>(v.Get(4 * i + 1, 0));
      m_buffer[3 * i + 1] = static_cast<uint32_t>(v.Get(4 * i + 2, 0));
      m_buffer[3 * i + 2] = static_cast<uint32_t>(v.Get(4 * i + 3, 0));

#if (DEBUG_PRINT)
      std::cout << i << " : " << m_buffer[3 * i + 0] << ", " << m_buffer[3 * i + 1] << ", "
                << m_buffer[3 * i + 2] << std::endl;
#endif
    }

#if (DEBUG_PRINT)
    std::cout << "............................................." << std::endl;
#endif
  }

private:
  using VectorArrayBuffer<ToType>::m_vectorCount;
  using VectorArrayBuffer<ToType>::m_bufferLength;
  using VectorArrayBuffer<ToType>::m_bufferByteCount;
  using VectorArrayBuffer<ToType>::m_buffer;
};

} // namespace details

using RealArrayDispatcher = vtkArrayDispatch::DispatchByValueType<vtkArrayDispatch::Reals>;
using IntegralArrayDispatcher = vtkArrayDispatch::DispatchByValueType<vtkArrayDispatch::Integrals>;

std::unique_ptr<VectorArrayBuffer<float> > extractPointsToFloatArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData
)
{
  if (!polyData->GetPoints())
  {
    return nullptr;
  }

  vtkDataArray* pointsArray = polyData->GetPoints()->GetData();

  if (!pointsArray)
  {
    return nullptr;
  }

  std::unique_ptr<details::PointsArrayPacker<float> > pointsCaster
    = std::make_unique<details::PointsArrayPacker<float> >();

  if (!RealArrayDispatcher::Execute(pointsArray, *pointsCaster))
  {
    (*pointsCaster)(pointsArray); // vtkDataArray fallback
  }

  //        std::cout << "------------- extractPointsToFloatArrayBuffer ------------------ " << std::endl;
  //        for ( int i = 0; i < 3 * polyData->GetNumberOfPoints(); ++i )
  //        {
  //            std::cout << pointsCaster->buffer()[i] << " ";
  //        }
  //        std::cout << "------------------------------- " << std::endl;

  return pointsCaster;
}

std::unique_ptr<VectorArrayBuffer<uint32_t> > extractNormalsToUIntArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData
)
{
  if (!polyData->GetPointData())
  {
    return nullptr;
  }

  vtkDataArray* normalsArray = polyData->GetPointData()->GetNormals();

  if (!normalsArray)
  {
    return nullptr;
  }

  std::unique_ptr<details::NormalsArrayPacker<uint32_t> > normalsCaster
    = std::make_unique<details::NormalsArrayPacker<uint32_t> >();

  if (!RealArrayDispatcher::Execute(normalsArray, *normalsCaster))
  {
    (*normalsCaster)(normalsArray);
  }

  //    for ( int i = 0; i < polyData->GetNumberOfPoints(); ++i )
  //    {
  //        std::cout << glm::to_string( glm::unpackSnorm3x10_1x2( normalsPacker.m_packedNormalsBuffer[i]) ) << " ";
  //    }

  //    std::cout << "------------------------------- " << std::endl;

  return normalsCaster;
}

std::unique_ptr<VectorArrayBuffer<uint32_t> > extractTexCoordsToUIntArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData
)
{
  if (!polyData->GetPointData())
  {
    return nullptr;
  }

  vtkDataArray* texCoordsArray = polyData->GetPointData()->GetTCoords();

  if (!texCoordsArray)
  {
    return nullptr;
  }

  std::unique_ptr<details::TCoordsArrayPacker<uint32_t> > tcoordsCaster
    = std::make_unique<details::TCoordsArrayPacker<uint32_t> >();

  if (!RealArrayDispatcher::Execute(texCoordsArray, *tcoordsCaster))
  {
    (*tcoordsCaster)(texCoordsArray);
  }

  //    const uint32_t* B = static_cast< const uint32_t* >( tcoordsCaster->buffer() );

  //    for ( int i = 0; i < tcoordsCaster->length(); ++i )
  //    {
  //        std::cout << i << ": " << glm::to_string( glm::unpackUnorm2x16( B[i]) ) << std::endl;
  //    }

  //    std::cout << "------------------------------- " << std::endl;

  return tcoordsCaster;
}

std::unique_ptr<VectorArrayBuffer<float> > extractTexCoordsToFloatArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData
)
{
  if (!polyData->GetPointData())
  {
    return nullptr;
  }

  vtkDataArray* texCoordsArray = polyData->GetPointData()->GetTCoords();

  if (!texCoordsArray)
  {
    return nullptr;
  }

  std::unique_ptr<details::TCoordsFloatArrayPacker<float> > tcoordsCaster
    = std::make_unique<details::TCoordsFloatArrayPacker<float> >();

  if (!RealArrayDispatcher::Execute(texCoordsArray, *tcoordsCaster))
  {
    (*tcoordsCaster)(texCoordsArray);
  }

  //    std::cout << "------------- extractTexCoordsToFloatArrayBuffer ------------------ " << std::endl;
  //    for ( int i = 0; i < 2 * polyData->GetNumberOfPoints(); ++i )
  //    {
  //        std::cout << tcoordsCaster->buffer()[i] << " ";
  //    }
  //    std::cout << "------------------------------- " << std::endl;

  return tcoordsCaster;
}

std::unique_ptr<VectorArrayBuffer<uint32_t> > extractIndicesToUIntArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData
)
{
  if (!polyData->GetPolys())
  {
    return nullptr;
  }

  vtkIdTypeArray* indicesArray = polyData->GetPolys()->GetData();

  if (!indicesArray)
  {
    return nullptr;
  }

  std::unique_ptr<details::IndicesArrayPacker<uint32_t> > indicesCaster
    = std::make_unique<details::IndicesArrayPacker<uint32_t> >();

  if (!IntegralArrayDispatcher::Execute(indicesArray, *indicesCaster))
  {
    (*indicesCaster)(indicesArray);
  }

  return indicesCaster;
}

} // namespace vtkconvert
