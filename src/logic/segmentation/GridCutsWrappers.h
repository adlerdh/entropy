#ifndef GRID_CUTS_WRAPPERS_H
#define GRID_CUTS_WRAPPERS_H

#include <GridCut/GridGraph_3D_26C.h>
#include <GridCut/GridGraph_3D_6C.h>
#include <GridCut/GridGraph_3D_6C_MT.h>

#include <AlphaExpansion/AlphaExpansion_3D_26C.h>
#include <AlphaExpansion/AlphaExpansion_3D_6C.h>
#include <AlphaExpansion/AlphaExpansion_3D_6C_MT.h>

#include <memory>

template<typename type_tcap, typename type_ncap, typename type_flow>
class GridGraph_3D_Base_Wrapper
{
public:
  virtual ~GridGraph_3D_Base_Wrapper() = default;

  virtual int node_id(int x, int y, int z) const = 0;

  virtual void set_terminal_cap(int node_id, type_tcap cap_source, type_tcap cap_sink) = 0;
  virtual void set_neighbor_cap(
    int node_id, int offset_x, int offset_y, int offset_z, type_ncap cap
  ) = 0;

  virtual void set_caps(
    const type_tcap* cap_source,
    const type_tcap* cap_sink,
    const type_ncap* cap_lee,
    const type_ncap* cap_gee,
    const type_ncap* cap_ele,
    const type_ncap* cap_ege,
    const type_ncap* cap_eel,
    const type_ncap* cap_eeg
  ) = 0;

  virtual void compute_maxflow() = 0;

  virtual int get_segment(int node_id) const = 0;
};

template<typename type_tcap, typename type_ncap, typename type_flow>
class GridGraph_3D_6C_Wrapper : public GridGraph_3D_Base_Wrapper<type_tcap, type_ncap, type_flow>
{
public:
  GridGraph_3D_6C_Wrapper(int width, int height, int depth)
    : m_grid(
      std::make_unique<GridGraph_3D_6C<type_tcap, type_ncap, type_flow> >(width, height, depth)
    )
  {
  }

  ~GridGraph_3D_6C_Wrapper() override = default;

  int node_id(int x, int y, int z) const override { return m_grid->node_id(x, y, z); }

  void set_terminal_cap(int node_id, type_tcap cap_source, type_tcap cap_sink) override
  {
    m_grid->set_terminal_cap(node_id, cap_source, cap_sink);
  }

  void set_neighbor_cap(int node_id, int offset_x, int offset_y, int offset_z, type_ncap cap) override
  {
    m_grid->set_neighbor_cap(node_id, offset_x, offset_y, offset_z, cap);
  }

  void set_caps(
    const type_tcap* cap_source,
    const type_tcap* cap_sink,
    const type_ncap* cap_lee,
    const type_ncap* cap_gee,
    const type_ncap* cap_ele,
    const type_ncap* cap_ege,
    const type_ncap* cap_eel,
    const type_ncap* cap_eeg
  ) override
  {
    m_grid->set_caps(cap_source, cap_sink, cap_lee, cap_gee, cap_ele, cap_ege, cap_eel, cap_eeg);
  }

  void compute_maxflow() override { m_grid->compute_maxflow(); }
  int get_segment(int node_id) const override { return m_grid->get_segment(node_id); }

private:
  std::unique_ptr<GridGraph_3D_6C<type_tcap, type_ncap, type_flow> > m_grid;
};

template<typename type_tcap, typename type_ncap, typename type_flow>
class GridGraph_3D_6C_MT_Wrapper : public GridGraph_3D_Base_Wrapper<type_tcap, type_ncap, type_flow>
{
public:
  GridGraph_3D_6C_MT_Wrapper(int width, int height, int depth, int num_threads, int block_size)
    : m_grid(std::make_unique<GridGraph_3D_6C_MT<type_tcap, type_ncap, type_flow> >(
      width, height, depth, num_threads, block_size
    ))
  {
  }

  ~GridGraph_3D_6C_MT_Wrapper() override = default;

  int node_id(int x, int y, int z) const override { return m_grid->node_id(x, y, z); }

  void set_terminal_cap(int, type_tcap, type_tcap) override {}
  void set_neighbor_cap(int, int, int, int, type_ncap) override {}

  void set_caps(
    const type_tcap* cap_source,
    const type_tcap* cap_sink,
    const type_ncap* cap_lee,
    const type_ncap* cap_gee,
    const type_ncap* cap_ele,
    const type_ncap* cap_ege,
    const type_ncap* cap_eel,
    const type_ncap* cap_eeg
  ) override
  {
    m_grid->set_caps(cap_source, cap_sink, cap_lee, cap_gee, cap_ele, cap_ege, cap_eel, cap_eeg);
  }

  void compute_maxflow() override { m_grid->compute_maxflow(); }
  int get_segment(int node_id) const override { return m_grid->get_segment(node_id); }

private:
  std::unique_ptr<GridGraph_3D_6C_MT<type_tcap, type_ncap, type_flow> > m_grid;
};

template<typename type_tcap, typename type_ncap, typename type_flow>
class GridGraph_3D_26C_Wrapper : public GridGraph_3D_Base_Wrapper<type_tcap, type_ncap, type_flow>
{
public:
  GridGraph_3D_26C_Wrapper(int width, int height, int depth)
    : m_grid(
      std::make_unique<GridGraph_3D_26C<type_tcap, type_ncap, type_flow> >(width, height, depth)
    )
  {
  }

  ~GridGraph_3D_26C_Wrapper() override = default;

  int node_id(int x, int y, int z) const override { return m_grid->node_id(x, y, z); }

  void set_terminal_cap(int node_id, type_tcap cap_source, type_tcap cap_sink) override
  {
    m_grid->set_terminal_cap(node_id, cap_source, cap_sink);
  }

  void set_neighbor_cap(int node_id, int offset_x, int offset_y, int offset_z, type_ncap cap) override
  {
    m_grid->set_neighbor_cap(node_id, offset_x, offset_y, offset_z, cap);
  }

  void
  set_caps(const type_tcap*, const type_tcap*, const type_ncap*, const type_ncap*, const type_ncap*, const type_ncap*, const type_ncap*, const type_ncap*)
    override
  {
  }

  void compute_maxflow() override { m_grid->compute_maxflow(); }
  int get_segment(int node_id) const override { return m_grid->get_segment(node_id); }

private:
  std::unique_ptr<GridGraph_3D_26C<type_tcap, type_ncap, type_flow> > m_grid;
};

template<typename type_label, typename type_cost, typename type_energy>
class AlphaExpansion_3D_Base_Wrapper
{
public:
  virtual ~AlphaExpansion_3D_Base_Wrapper() = default;
  virtual void perform() = 0;
  virtual void perform_random() = 0;
  virtual type_label* get_labeling() = 0;
};

template<typename type_label, typename type_cost, typename type_energy>
class AlphaExpansion_3D_6C_Wrapper
  : public AlphaExpansion_3D_Base_Wrapper<type_label, type_cost, type_energy>
{
public:
  AlphaExpansion_3D_6C_Wrapper(
    int width, int height, int depth, int nLabels, type_cost* data, type_cost** smooth
  )
    : m_expansion(std::make_unique<AlphaExpansion_3D_6C<type_label, type_cost, type_energy> >(
      width, height, depth, nLabels, data, smooth
    ))
  {
  }

  AlphaExpansion_3D_6C_Wrapper(
    int width,
    int height,
    int depth,
    int nLabels,
    type_cost* data,
    typename AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::SmoothCostFn smoothFn
  )
    : m_expansion(std::make_unique<AlphaExpansion_3D_6C<type_label, type_cost, type_energy> >(
      width, height, depth, nLabels, data, smoothFn
    ))
  {
  }

  ~AlphaExpansion_3D_6C_Wrapper() override = default;

  void perform() override { m_expansion->perform(); }
  void perform_random() override { m_expansion->perform_random(); }
  type_label* get_labeling() override { return m_expansion->get_labeling(); }

private:
  std::unique_ptr<AlphaExpansion_3D_6C<type_label, type_cost, type_energy> > m_expansion;
};

template<typename type_label, typename type_cost, typename type_energy>
class AlphaExpansion_3D_6C_MT_Wrapper
  : public AlphaExpansion_3D_Base_Wrapper<type_label, type_cost, type_energy>
{
public:
  AlphaExpansion_3D_6C_MT_Wrapper(
    int width,
    int height,
    int depth,
    int nLabels,
    type_cost* data,
    typename AlphaExpansion_3D_6C<type_label, type_cost, type_energy>::SmoothCostFn smoothFn,
    int num_threads,
    int block_size
  )
    : m_expansion(std::make_unique<AlphaExpansion_3D_6C_MT<type_label, type_cost, type_energy> >(
      width, height, depth, nLabels, data, smoothFn, num_threads, block_size
    ))
  {
  }

  ~AlphaExpansion_3D_6C_MT_Wrapper() override = default;

  void perform() override { m_expansion->perform(); }
  void perform_random() override { m_expansion->perform_random(); }
  type_label* get_labeling() override { return m_expansion->get_labeling(); }

private:
  std::unique_ptr<AlphaExpansion_3D_6C_MT<type_label, type_cost, type_energy> > m_expansion;
};

template<typename type_label, typename type_cost, typename type_energy>
class AlphaExpansion_3D_26C_Wrapper
  : public AlphaExpansion_3D_Base_Wrapper<type_label, type_cost, type_energy>
{
public:
  AlphaExpansion_3D_26C_Wrapper(
    int width, int height, int depth, int nLabels, type_cost* data, type_cost** smooth
  )
    : m_expansion(std::make_unique<AlphaExpansion_3D_26C<type_label, type_cost, type_energy> >(
      width, height, depth, nLabels, data, smooth
    ))
  {
  }

  AlphaExpansion_3D_26C_Wrapper(
    int width,
    int height,
    int depth,
    int nLabels,
    type_cost* data,
    typename AlphaExpansion_3D_26C<type_label, type_cost, type_energy>::SmoothCostFn smoothFn
  )
    : m_expansion(std::make_unique<AlphaExpansion_3D_26C<type_label, type_cost, type_energy> >(
      width, height, depth, nLabels, data, smoothFn
    ))
  {
  }

  ~AlphaExpansion_3D_26C_Wrapper() override = default;

  void perform() override { m_expansion->perform(); }
  void perform_random() override { m_expansion->perform_random(); }
  type_label* get_labeling() override { return m_expansion->get_labeling(); }

private:
  std::unique_ptr<AlphaExpansion_3D_26C<type_label, type_cost, type_energy> > m_expansion;
};

#endif // GRID_CUTS_WRAPPERS_H
