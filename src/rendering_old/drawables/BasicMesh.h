#ifndef BASIC_MESH_H
#define BASIC_MESH_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"
#include "rendering_old/common/MeshColorLayer.h"

#include "common/ObjectCounter.hpp"

#include <array>
#include <memory>
#include <utility>

class MeshGpuRecord;

/**
 * @brief
 *
 * @todo Implement layering with offset towards/away from viewer.
 */
class BasicMesh final : public DrawableBase, public ObjectCounter<BasicMesh>
{
public:
  BasicMesh(
    std::string name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    std::weak_ptr<MeshGpuRecord> meshGpuRecord
  );

  BasicMesh(const BasicMesh&) = delete;
  BasicMesh& operator=(const BasicMesh&) = delete;

  ~BasicMesh() override = default;

  bool isOpaque() const override;

  DrawableOpacity opacityFlag() const override;

  void setMeshGpuRecord(std::weak_ptr<MeshGpuRecord> meshGpuRecord);
  std::weak_ptr<MeshGpuRecord> meshGpuRecord();

  void setUseOctantClipPlanes(bool set);

  void setLayerOpacityMultiplier(BasicMeshColorLayer, float m);
  float getLayerOpacityMultiplier(BasicMeshColorLayer) const;

  void setLayerOpacity(BasicMeshColorLayer, float a);
  float getLayerOpacity(BasicMeshColorLayer) const;

  void enableLayer(BasicMeshColorLayer);
  void disableLayer(BasicMeshColorLayer);

  /**
     * @brief Set mesh material color as NON-premultiplied RGB
     * @param color RGB (non-premultiplied)
     */
  void setMaterialColor(const glm::vec3& color);
  glm::vec3 getMaterialColor() const;

  void setMaterialShininess(float);
  float getMaterialShininess() const;

  void setBackfaceCull(bool);
  bool getBackfaceCull() const;

  void setUseXrayMode(bool);
  void setXrayPower(float);

  void setEnablePolygonOffset(bool enable);
  void setPolygonOffsetValues(float factor, float units);

  void setAmbientLightFactor(float);
  void setDiffuseLightFactor(float);
  void setSpecularLightFactor(float);
  void setAdsLightFactors(float a, float d, float s);

private:
  void doSetupState() override;
  void doRender(const RenderStage& stage) override;
  void doTeardownState() override;

  void doUpdate(
    double time, const Viewport&, const camera::Camera&, const CoordinateFrame& crosshairs
  ) override;

  void initVao();
  void updateLayerOpacities();

  ShaderProgramActivatorType m_shaderProgramActivator;
  UniformsProviderType m_uniformsProvider;

  std::string m_stdShaderName;
  std::string m_peelShaderName;

  GLVertexArrayObject m_vao;
  std::unique_ptr<GLVertexArrayObject::IndexedDrawParams> m_vaoParams;

  std::weak_ptr<MeshGpuRecord> m_meshGpuRecord;

  std::array<float, static_cast<size_t>(BasicMeshColorLayer::NumLayers)> m_layerOpacities;
  std::array<float, static_cast<size_t>(BasicMeshColorLayer::NumLayers)> m_layerOpacityMultipliers;
  std::array<float, static_cast<size_t>(BasicMeshColorLayer::NumLayers)> m_finalLayerOpacities;

  float m_overallOpacity;

  Uniforms m_stdUniforms;
  Uniforms m_initUniforms;
  Uniforms m_peelUniforms;

  glm::mat4 m_clip_O_camera;
  glm::mat4 m_camera_O_world;

  bool m_cameraIsOrthographic;

  glm::vec3 m_worldCameraPos;
  glm::vec3 m_worldCameraDir;
  glm::vec3 m_worldLightPos;
  glm::vec3 m_worldLightDir;

  // Equation of plane with normal n = (A, B, C) and point q = (x0, y0, z0):
  // A*x + B*y + C*z + D = 0
  // D = −A*x0 − B*y0 − C*z0 = -dot(n, q)

  bool m_useOctantClipPlanes;
  std::array<glm::vec4, 3> m_worldClipPlanes;

  // Material properties
  glm::vec3 m_materialColor;
  float m_materialShininess;

  // ADS light colors
  glm::vec3 m_ambientLightColor;
  glm::vec3 m_diffuseLightColor;
  glm::vec3 m_specularLightColor;

  // ADS factors for normal mode
  float m_ambientLightFactor;
  float m_diffuseLightFactor;
  float m_specularLightFactor;

  // ADS factors for x-ray mode
  float m_xrayAmbientLightFactor;
  float m_xrayDiffuseLightFactor;
  float m_xraySpecularLightFactor;

  bool m_wireframe;
  bool m_backfaceCull;

  bool m_xrayMode;
  float m_xrayPower;

  bool m_enablePolygonOffset;
  float m_polygonOffsetFactor;
  float m_polygonOffsetUnits;
};

#endif // BASIC_MESH_H
