#ifndef MESH_INFO_H
#define MESH_INFO_H

#include "mesh/MeshTypes.h"

#include <boost/variant.hpp>

#include <cstddef>
#include <variant>


class MeshInfo
{
public:

    MeshInfo( MeshSource meshSource,
              MeshPrimitiveType primitiveType,
              boost::variant< double, uint32_t > scalarValue );

    MeshInfo( const MeshInfo& ) = default;
    MeshInfo& operator=( const MeshInfo& ) = default;

    MeshInfo( MeshInfo&& ) = default;
    MeshInfo& operator=( MeshInfo&& ) = default;

    ~MeshInfo() = default;

    MeshSource meshSource() const;
    MeshPrimitiveType primitiveType() const;

    // Scalar value is an index for label meshes
    boost::variant< double, uint32_t > scalarValue() const;

    double isoValue() const;

    uint32_t labelIndex() const;


private:

    MeshSource m_meshSource;
    MeshPrimitiveType m_primitiveType;

    // Either isovalue or label index
    boost::variant< double, uint32_t > m_scalarValue;

    struct IsoValueVisitor : public boost::static_visitor< double >
    {
        double operator()( double s ) const { return s; }
        double operator()( uint32_t ) const { return -1.0; }
    };

    struct LabelVisitor : public boost::static_visitor< uint32_t >
    {
        uint32_t operator()( double ) const { return 0; }
        uint32_t operator()( uint32_t s ) const { return s; }
    };
};

//std::ostream& operator<< ( std::ostream&, const imageio::MeshInfo& );

#endif // MESH_INFO_H
