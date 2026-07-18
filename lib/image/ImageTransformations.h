#pragma once

#include <filesystem>
#include "image/ImageHeaderOverrides.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <optional>
#include <ostream>

/**
 * @brief Container for image transformations. There are four image spaces:
 *
 * TEXTURE SPACE (T): Representation of image in GPU texture space, where the 3D volumetric elements
 * are called "texels". Coordinate axes are normalized to the range [0.0, 1.0], with 0.0 and 1.0
 * denoting the EDGES of the first and last image pixels (not the pixel centers). Image samples are
 * positioned at the centers of pixels. The three texel coordinates are labeled (s, t, p).
 *
 * PIXEL/VOXEL SPACE (P): Representation of image on disk and RAM. Coordinates along an image
 * dimension run from [0, N-1], where N is the number of pixels along the dimension and where 0 and
 * N-1 denote the CENTERS (not edges) of the first and last pixels. Note: the term "pixel" is used
 * synonymously with "voxel", even for 3D images. The three pixel coordinates are labeled (i, j, k).
 *
 * NATIVE SUBJECT SPACE (S): Native (untransformed) space of the subject in physical units,
 * most commonly millimeters. The transformation from Pixel space to Native Subject space is
 * computed from the image pixel size, origin, and orientation direction vectors. These values are
 * defined in the image header. This space is defined such that positive coordinates (x, y, z)
 * correspond to physical directions Left, Posterior, and Superior (or, LPS) for human subjects.
 *
 * AFFINE-REGISTERED SUBJECT SPACE (A): Space of the image following affine registration. The affine
 * registration is loaded from a file on disk and is not set manually in Entropy.
 *
 * DEFORMED SUBJECT SPACE (D): Space of the image following manual registration in Entropy.
 *
 * WORLD SPACE (W): Space of the image following deformable registration. This is the space in
 * which the image is rendered. Prior to registration, it is identical to Subject space
 * (i.e. world_T_subject == identity). However, the user may choose to load and apply affine and
 * non-linear transformations between Subject and World space. This is useful when co-registering
 * images to each other or when otherwise transforming the subject.
 *
 * The full image transformation chain is
 * [World (W) <-- Deformed World (D) <-- Affine Subject (A) <-- Native Subject (S) <-- Pixel (P) <--
 * Texture (T)]
 *
 * The rendering transformation chain is
 * [Window Viewport (pixels) <-- View (pixels) <-- Clip/NDC <-- Camera/Eye <-- World (W)]
 */
class ImageTransformations
{
public:
  /// @brief Type of manual subject transformation.
  enum class ManualTransformationType
  {
    Rigid,     //!< translation (3 DOF) + rotation (3 DOF)
    Similarity //!< translation (3 DOF) + rotation (3 DOF) + scale (3 DOF)
  };

  /// @brief Construct identity/default transformations.
  ImageTransformations() = default;

  /**
   * @brief Construct from image header information
   *
   * @param[in] pixelDimensions Image dimensions in pixel/voxel units
   * @param[in] pixelSpacing Spacings of image pixels/voxels
   * @param[in] pixelOrigin Position of image pixel/voxel (0, 0, 0) in Subject space
   * @param[in] pixelDirections Directions of image pixel/voxel axes in Subject space
   */
  ImageTransformations(
    const glm::uvec3& pixelDimensions,
    const glm::vec3& pixelSpacing,
    const glm::vec3& pixelOrigin,
    const glm::mat3& pixelDirections);

  ImageTransformations(const ImageTransformations&) = default;
  ImageTransformations& operator=(const ImageTransformations&) = default;

  ImageTransformations(ImageTransformations&&) = default;
  ImageTransformations& operator=(ImageTransformations&&) = default;

  ~ImageTransformations() = default;

  /// @brief Apply image-header geometry overrides and update dependent transforms.
  void setHeaderOverrides(const ImageHeaderOverrides& overrides);
  /// @brief Get the current image-header geometry overrides.
  const ImageHeaderOverrides& getHeaderOverrides() const;

  /// @brief Return whether manual worldDef_T_affine edits are locked.
  bool is_worldDef_T_affine_locked() const;
  /// @brief Set whether manual worldDef_T_affine edits are locked.
  void set_worldDef_T_affine_locked(bool locked);

  /// @brief Return whether transformations have changed since the dirty flag was cleared.
  bool isDirty() const;
  /// @brief Set the dirty flag used by callers to detect transform changes.
  void setDirty(bool set);

  /// @brief Get reciprocal pixel dimensions used for texture-to-pixel transforms.
  glm::vec3 invPixelDimensions() const;

  /// @brief Set manual translation from affine subject space to deformed world space.
  void set_worldDef_T_affine_translation(glm::vec3 get_worldDef_T_affine_translation);
  /// @brief Get manual translation from affine subject space to deformed world space.
  const glm::vec3& get_worldDef_T_affine_translation() const;

  /// @brief Set manual rotation from affine subject space to deformed world space.
  void set_worldDef_T_affine_rotation(glm::quat worldDef_T_affine_rotation);
  /// @brief Get manual rotation from affine subject space to deformed world space.
  const glm::quat& get_worldDef_T_affine_rotation() const;

  /// @brief Set manual scale from affine subject space to deformed world space.
  void set_worldDef_T_affine_scale(glm::vec3 worldDef_T_affine_scale);
  /// @brief Get manual scale from affine subject space to deformed world space.
  const glm::vec3& get_worldDef_T_affine_scale() const;

  /// @brief Get enabled manual transform, or identity when disabled.
  const glm::mat4& get_worldDef_T_affine() const;

  /// @brief Set the manual transformation matrix directly.
  void set_worldDef_T_affine(glm::mat4 worldDef_T_affine);

  /// @brief Reset the manual transformation matrix and parameters to identity.
  void reset_worldDef_T_affine();

  /// @brief Set the affine-registration matrix from native subject to affine subject space.
  void set_affine_T_subject(glm::mat4 affine_T_subject);
  /// @brief Get enabled affine-registration matrix, or identity when disabled.
  const glm::mat4& get_affine_T_subject() const;

  /// @brief Set the path of the file that supplied affine_T_subject.
  void set_affine_T_subject_fileName(const std::optional<std::filesystem::path>& fileName);
  /// @brief Get the path of the file that supplied affine_T_subject, if any.
  const std::optional<std::filesystem::path>& get_affine_T_subject_fileName() const;

  /// @brief Enable or disable the manual worldDef_T_affine transform.
  void set_enable_worldDef_T_affine(bool enable);
  /// @brief Return whether the manual worldDef_T_affine transform is enabled.
  bool get_enable_worldDef_T_affine() const;

  /// @brief Enable or disable the loaded affine_T_subject transform.
  void set_enable_affine_T_subject(bool enable);
  /// @brief Return whether the loaded affine_T_subject transform is enabled.
  bool get_enable_affine_T_subject() const;

  const glm::mat4& worldDef_T_subject() const; //!< Get tx from image Subject to Deformed World space
  const glm::mat4& subject_T_worldDef() const; //!< Get tx from Deformed World to image Subject space
  const glm::mat3& subject_T_worldDef_invTransp()
    const; /// Get inverse-transpose of tx from World to image Subject space

  const glm::mat4& subject_T_pixel() const; //!< Get tx from image Pixel to Subject space
  const glm::mat4& pixel_T_subject() const; //!< Get tx from image Subject to Pixel space

  const glm::mat4& pixel_T_texture() const; //!< Get tx from image Texture to Pixel space
  const glm::mat4& texture_T_pixel() const; //!< Get tx from image Pixel to Texture space

  const glm::mat4& subject_T_texture() const; //!< Get tx from image Texture to Subject space
  const glm::mat4& texture_T_subject() const; //!< Get tx from image Subject to Texture space

  const glm::mat4& worldDef_T_texture() const; //!< Get tx from image Texture to Deformed World space
  const glm::mat4& texture_T_worldDef() const; //!< Get tx from Deformed World to image Texture space

  const glm::mat4& worldDef_T_pixel() const;           //!< Get tx from image Pixel to Deformed World space
  const glm::mat4& pixel_T_worldDef() const;           //!< Get tx from Deformed World to image Pixel space
  const glm::mat3& pixel_T_worldDef_invTransp() const; /// Get inverse-transpose of tx from World to image Pixel space

  friend std::ostream& operator<<(std::ostream&, const ImageTransformations&);

private:
  void initializeTransformations();

  /// Update the transformations that involve Subject space, including world_T_subject (and its
  /// inverse)
  void updateTransformations();

  /// Overrides to the original image header
  ImageHeaderOverrides m_headerOverrides;

  /// When true, prevents the worldDef_T_affine ("manual") transformation from changing
  bool m_is_worldDef_T_affine_locked;

  /// Inverses of the pixel dimensions
  glm::vec3 m_invPixelDimensions;

  /// Constraints applied to affine2_T_affine1
  ManualTransformationType m_worldDef_T_affine_TxType;

  glm::mat4 m_subject_T_pixel; //!< Pixel to Subject space
  glm::mat4 m_pixel_T_subject; //!< Subject to Pixel space (inverse of above)

  glm::mat4 m_texture_T_pixel; //!< Pixel to Texture space
  glm::mat4 m_pixel_T_texture; //!< Texture to Pixel space (inverse of above)

  glm::mat4 m_texture_T_subject; //!< Subject to Texture space
  glm::mat4 m_subject_T_texture; //!< Texture to Subject space (inverse of above)

  // Parameters of the user-applied manual transformation:
  glm::vec3 m_worldDef_T_affine_translation; //!< Translation component of worldDef_T_affine (applied 3rd)
  glm::quat m_worldDef_T_affine_rotation{
    1.0f,
    0.0f,
    0.0f,
    0.0f};                             //!< Rotation component of worldDef_T_affine  (applied 2nd)
  glm::vec3 m_worldDef_T_affine_scale; //!< Scale component of worldDef_T_affine (applied 1st)

  glm::mat4 m_worldDef_T_affine;   //!< User-applied manual transformation (defined by the above parameters)
  bool m_enable_worldDef_T_affine; //!< Is the worldDef_T_affine transformation used?

  glm::mat4 m_affine_T_subject;   //!< Affine matrix loaded from disk, mapping Subject to AffineA space
  bool m_enable_affine_T_subject; //!< Is the affine_T_subject transformation used?

  std::optional<std::filesystem::path> m_affine_T_subject_fileName; //!< affine_T_subject matrix file name (if used)

  glm::mat4 m_worldDef_T_subject;           //!< Subject to Deformed World space
  glm::mat4 m_subject_T_worldDef;           //!< Deformed World to Subject space (inverse of above)
  glm::mat3 m_subject_T_worldDef_invTransp; //!< Inverse-transpose of Deformed World to Subject space tx

  glm::mat4 m_worldDef_T_texture; //!< Texture to Deformed World space
  glm::mat4 m_texture_T_worldDef; //!< Deformed World to Texture space (inverse of above)

  glm::mat4 m_worldDef_T_pixel;           //!< Pixel to Deformed World space
  glm::mat4 m_pixel_T_worldDef;           //!< Deformed World to Pixel space
  glm::mat3 m_pixel_T_worldDef_invTransp; //!< Inverse-transpose of Deformed World to Pixel space tx
};

std::ostream& operator<<(std::ostream&, const ImageTransformations&);

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<ImageTransformations> : ostream_formatter
{
};
#endif
