#include "mesh/MeshInfo.hpp"

MeshInfo::MeshInfo(
  MeshSource meshSource,
  MeshPrimitiveType primitiveType,
  boost::variant<double, uint32_t> scalarValue
)
  : m_meshSource(std::move(meshSource))
  , m_primitiveType(std::move(primitiveType))
  , m_scalarValue(scalarValue)
{
}

MeshSource MeshInfo::meshSource() const
{
  return m_meshSource;
}

MeshPrimitiveType MeshInfo::primitiveType() const
{
  return m_primitiveType;
}

boost::variant<double, uint32_t> MeshInfo::scalarValue() const
{
  return m_scalarValue;
}

double MeshInfo::isoValue() const
{
  return boost::apply_visitor(IsoValueVisitor(), m_scalarValue);
}

uint32_t MeshInfo::labelIndex() const
{
  return boost::apply_visitor(LabelVisitor(), m_scalarValue);
}

/*
std::ostream&
operator<< ( std::ostream& os, const imageio::MeshInfo& info )
{
    os << "Mesh type: " << info.m_meshType << std::endl
       << "Scalar value: " << info.m_scalarValue << std::endl;

    os << std::endl;

    return os;
}
*/
