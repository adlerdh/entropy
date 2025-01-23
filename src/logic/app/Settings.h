#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "common/ParcellationLabelTable.h"
#include "common/SegmentationTypes.h"
#include "common/Types.h"

#include <glm/vec3.hpp>

#include <optional>

/**
 * @brief Holds all application settings
 *
 * @note the IPC handler for communication of crosshairs coordinates with ITK-SNAP
 * is not hooked up yet. It wasn't working properly across all platforms.
 */
class AppSettings
{
public:
  AppSettings();
  ~AppSettings() = default;

  bool synchronizeZooms() const;
  void setSynchronizeZooms(bool);

  bool overlays() const;
  void setOverlays(bool);

  size_t foregroundLabel() const;
  size_t backgroundLabel() const;

  void setForegroundLabel(size_t label, const ParcellationLabelTable& activeLabelTable);
  void setBackgroundLabel(size_t label, const ParcellationLabelTable& activeLabelTable);

  void adjustActiveSegmentationLabels(const ParcellationLabelTable& activeLabelTable);
  void swapForegroundAndBackgroundLabels(const ParcellationLabelTable& activeLabelTable);

  bool replaceBackgroundWithForeground() const;
  void setReplaceBackgroundWithForeground(bool set);

  bool use3dBrush() const;
  void setUse3dBrush(bool set);

  bool useIsotropicBrush() const;
  void setUseIsotropicBrush(bool set);

  bool useVoxelBrushSize() const;
  void setUseVoxelBrushSize(bool set);

  bool useRoundBrush() const;
  void setUseRoundBrush(bool set);

  bool crosshairsMoveWithBrush() const;
  void setCrosshairsMoveWithBrush(bool set);

  uint32_t brushSizeInVoxels() const;
  void setBrushSizeInVoxels(uint32_t size);

  float brushSizeInMm() const;
  void setBrushSizeInMm(float size);

  double graphCutsWeightsAmplitude() const;
  void setGraphCutsWeightsAmplitude(double amplitude);

  double graphCutsWeightsSigma() const;
  void setGraphCutsWeightsSigma(double sigma);

  GraphNeighborhoodType graphCutsNeighborhood() const;
  void setGraphCutsNeighborhood(const GraphNeighborhoodType&);

  bool crosshairsMoveWhileAnnotating() const;
  void setCrosshairsMoveWhileAnnotating(bool set);

  bool lockAnatomicalCoordinateAxesWithReferenceImage() const;
  void setLockAnatomicalCoordinateAxesWithReferenceImage(bool lock);

private:
  bool m_synchronizeZoom; //!< Synchronize zoom between views
  bool m_overlays;        //!< Render UI and vector overlays

  /* Begin segmentation drawing variables */
  size_t m_foregroundLabel; //!< Foreground segmentation label
  size_t m_backgroundLabel; //!< Background segmentation label

  bool m_replaceBackgroundWithForeground; /// Paint foreground label only over background label
  bool m_use3dBrush;                      //!< Paint with a 3D brush
  bool m_useIsotropicBrush;               //!< Paint with an isotropic brush
  bool m_useVoxelBrushSize;               //!< Measure brush size in voxel units
  bool m_useRoundBrush;                   //!< Brush is round (true) or rectangular (false)
  bool m_crosshairsMoveWithBrush;         //!< Crosshairs move with the brush
  uint32_t m_brushSizeInVoxels;           //!< Brush size (diameter) in voxels
  float m_brushSizeInMm;                  //!< Brush size (diameter) in millimeters
  /* End segmentation drawing variables */

  /* Begin Graph Cuts weights variables */
  double m_graphCutsWeightsAmplitude; //!< Multiplier in front of exponential
  double
    m_graphCutsWeightsSigma; //!< Standard deviation in exponential, assuming image normalized as [1%, 99%] -> [0, 1]
  GraphNeighborhoodType m_graphCutsNeighborhood; //!< Neighboorhood used for constructing graph
  /* End Graph Cuts weights variables */

  /// Crosshairs move to the position of every new point added to an annotation
  bool m_crosshairsMoveWhileAnnotating;

  /// When the reference image rotates, do the anatomical coordinate axes (LPS, RAI)
  /// and crosshairs rotate, too? When this option is true, the rotation of the
  /// coordinate axes are locked with the reference image.
  bool m_lockAnatomicalCoordinateAxesWithReferenceImage;
};

#endif // APP_SETTINGS_H
