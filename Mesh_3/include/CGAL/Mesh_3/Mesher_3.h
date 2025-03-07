// Copyright (c) 2009-2014 INRIA Sophia-Antipolis (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0+
//
//
// Author(s)     : Laurent Rineau, Stephane Tayeb, Clement Jamin
//
//******************************************************************************
// File Description :
// Implements a mesher_3 with two mesher levels : one for facets and one for
// tets.
//******************************************************************************

#ifndef CGAL_MESH_3_MESHER_3_H
#define CGAL_MESH_3_MESHER_3_H

#include <CGAL/license/Mesh_3.h>


#include <CGAL/Mesh_3/config.h>

#include <CGAL/Mesh_error_code.h>

#include <CGAL/Mesh_3/Dump_c3t3.h>

#include<CGAL/Mesh_3/Refine_facets_3.h>
#include<CGAL/Mesh_3/Refine_facets_manifold_base.h>
#include<CGAL/Mesh_3/Refine_cells_3.h>
#include <CGAL/Mesh_3/Refine_tets_visitor.h>
#include <CGAL/Mesher_level_visitors.h>
#include <CGAL/Kernel_traits.h>

#ifdef CGAL_MESH_3_USE_OLD_SURFACE_RESTRICTED_DELAUNAY_UPDATE
#include <CGAL/Surface_mesher/Surface_mesher_visitor.h>
#endif

#include <CGAL/Mesh_3/Concurrent_mesher_config.h>
#include <CGAL/Real_timer.h>

#ifdef CGAL_MESH_3_PROFILING
  #include <CGAL/Mesh_3/Profiling_tools.h>
#endif

#ifdef CGAL_LINKED_WITH_TBB
#  if TBB_IMPLEMENT_CPP0X
#   include <tbb/compat/thread>
#  else
#   include <thread>
#  endif
#endif

#include <boost/format.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <string>

namespace CGAL {
namespace Mesh_3 {

/************************************************
// Class Mesher_3_base
// Two versions: sequential / parallel
************************************************/

// Sequential
template <typename Tr, typename Concurrency_tag>
class Mesher_3_base
{
protected:
  typedef typename Tr::Lock_data_structure Lock_data_structure;

  Mesher_3_base(const Bbox_3 &, int) {}

  Lock_data_structure *get_lock_data_structure() { return 0; }
  WorksharingDataStructureType *get_worksharing_data_structure() { return 0; }
  void set_bbox(const Bbox_3 &) {}
};

#ifdef CGAL_LINKED_WITH_TBB
// Parallel
template <typename Tr>
class Mesher_3_base<Tr, Parallel_tag>
{
protected:
  typedef typename Tr::Lock_data_structure Lock_data_structure;

  Mesher_3_base(const Bbox_3 &bbox, int num_grid_cells_per_axis)
  : m_lock_ds(bbox, num_grid_cells_per_axis),
    m_worksharing_ds(bbox)
  {}

  Lock_data_structure *get_lock_data_structure()
  {
    return &m_lock_ds;
  }
  WorksharingDataStructureType *get_worksharing_data_structure()
  {
    return &m_worksharing_ds;
  }

  void set_bbox(const Bbox_3 &bbox)
  {
    m_lock_ds.set_bbox(bbox);
    m_worksharing_ds.set_bbox(bbox);
  }

  /// Lock data structure
  Lock_data_structure m_lock_ds;
  /// Worksharing data structure
  WorksharingDataStructureType m_worksharing_ds;
};
#endif // CGAL_LINKED_WITH_TBB


/************************************************
 *
 * Mesher_3 class
 *
 ************************************************/

template<class C3T3, class MeshCriteria, class MeshDomain>
class Mesher_3
: public Mesher_3_base<
    typename C3T3::Triangulation,
#ifdef CGAL_DEBUG_FORCE_SEQUENTIAL_MESH_REFINEMENT
    Sequential_tag
#else
    typename C3T3::Concurrency_tag
#endif
  >
{
public:
#ifdef CGAL_DEBUG_FORCE_SEQUENTIAL_MESH_REFINEMENT
  typedef Sequential_tag                            Concurrency_tag;
#else
  typedef typename C3T3::Concurrency_tag            Concurrency_tag;
#endif
  typedef typename C3T3::Triangulation              Triangulation;
  typedef typename Triangulation::Point             Point;
  typedef typename Triangulation::Bare_point        Bare_point;
  typedef typename Kernel_traits<Point>::Kernel     Kernel;
  typedef typename Kernel::Vector_3                 Vector;
  typedef typename MeshDomain::Index                Index;

  // Self
  typedef Mesher_3<C3T3, MeshCriteria, MeshDomain>      Self;
#ifdef CGAL_DEBUG_FORCE_SEQUENTIAL_MESH_REFINEMENT
  typedef Mesher_3_base<Triangulation, Sequential_tag> Base;
#else
  typedef Mesher_3_base<Triangulation, typename C3T3::Concurrency_tag> Base;
#endif

  using Base::get_lock_data_structure;

  //-------------------------------------------------------
  // Mesher_levels
  //-------------------------------------------------------
  // typedef Refine_facets_manifold_base<Rf_base>      Rf_manifold_base;

  /// Facets mesher level
  typedef Refine_facets_3<
      Triangulation,
      typename MeshCriteria::Facet_criteria,
      MeshDomain,
      C3T3,
      Null_mesher_level,
      Concurrency_tag,
      Refine_facets_manifold_base
    >                                               Facets_level;

  /// Cells mesher level
  typedef Refine_cells_3<
      Triangulation,
      typename MeshCriteria::Cell_criteria,
      MeshDomain,
      C3T3,
      Facets_level,
      Concurrency_tag>                              Cells_level;

  //-------------------------------------------------------
  // Visitors
  //-------------------------------------------------------
  /// Facets visitor : to update cells when refining surface
  typedef tets::Refine_facets_visitor<
      Triangulation,
      Cells_level,
      Null_mesh_visitor>                            Facets_visitor;

#ifndef CGAL_MESH_3_USE_OLD_SURFACE_RESTRICTED_DELAUNAY_UPDATE
  /// Cells visitor : it just need to know previous level
  typedef Null_mesh_visitor_level<Facets_visitor>   Cells_visitor;
#else
  /// Cells visitor : to update surface (restore restricted Delaunay)
  /// when refining cells
  typedef Surface_mesher::Visitor<
      Triangulation,
      Facets_level,
      Facets_visitor>                               Cells_visitor;
#endif

  /// Constructor
  Mesher_3(C3T3&               c3t3,
           const MeshDomain&   domain,
           const MeshCriteria& criteria,
           int mesh_topology = 0,
           std::size_t maximal_number_of_vertices = 0,
           Mesh_error_code* error_code = 0);

  /// Destructor
  ~Mesher_3() 
  {
    // The lock data structure is going to be destroyed
    r_c3t3_.triangulation().set_lock_data_structure(NULL);
  }

  /// Launch mesh refinement
  double refine_mesh(std::string dump_after_refine_surface_prefix = "");

  /// Debug
  std::string debug_info() const;
  std::string debug_info_header() const;

  // Step-by-step methods
  void initialize();
  void fix_c3t3();
  void display_number_of_bad_elements();
  void one_step();
  bool is_algorithm_done();

#ifdef CGAL_MESH_3_MESHER_STATUS_ACTIVATED
  struct Mesher_status
  {
    std::size_t vertices, facet_queue, cells_queue;

    Mesher_status(std::size_t v, std::size_t f, std::size_t c)
     : vertices(v), facet_queue(f), cells_queue(c) {}
  };

  Mesher_status status() const;
#endif

private:
  /// The oracle
  const MeshDomain& r_oracle_;

  /// Meshers
  Null_mesher_level null_mesher_;
  Facets_level facets_mesher_;
  Cells_level cells_mesher_;

  /// Visitors
  Null_mesh_visitor null_visitor_;
  Facets_visitor facets_visitor_;
  Cells_visitor cells_visitor_;

  /// The container of the resulting mesh
  C3T3& r_c3t3_;

  enum Refinement_stage {
    NOT_INITIALIZED,
    REFINE_FACETS,
    REFINE_FACETS_AND_EDGES,
    REFINE_FACETS_AND_EDGES_AND_VERTICES,
    REFINE_ALL
  };

  Refinement_stage refinement_stage;

  /// Maximal number of vertices
  std::size_t maximal_number_of_vertices_;

  /// Pointer to the error code
  Mesh_error_code* error_code_;

private:
  // Disabled copy constructor
  Mesher_3(const Self& src);
  // Disabled assignment operator
  Self& operator=(const Self& src);

};  // end class Mesher_3



template<class C3T3, class MC, class MD>
Mesher_3<C3T3,MC,MD>::Mesher_3(C3T3& c3t3,
                               const MD& domain,
                               const MC& criteria,
                               int mesh_topology,
                               std::size_t maximal_number_of_vertices,
                               Mesh_error_code* error_code)
: Base(c3t3.bbox(),
       Concurrent_mesher_config::get().locking_grid_num_cells_per_axis)
, r_oracle_(domain)
, null_mesher_()
, facets_mesher_(c3t3.triangulation(),
                 criteria.facet_criteria_object(),
                 domain,
                 null_mesher_,
                 c3t3,
                 mesh_topology,
                 maximal_number_of_vertices)
, cells_mesher_(c3t3.triangulation(),
                criteria.cell_criteria_object(),
                domain,
                facets_mesher_,
                c3t3,
                maximal_number_of_vertices)
, null_visitor_()
, facets_visitor_(&cells_mesher_, &null_visitor_)
#ifndef CGAL_MESH_3_USE_OLD_SURFACE_RESTRICTED_DELAUNAY_UPDATE
, cells_visitor_(facets_visitor_)
#else
, cells_visitor_(&facets_mesher_, &facets_visitor_)
#endif
, r_c3t3_(c3t3)
, maximal_number_of_vertices_(maximal_number_of_vertices)
, error_code_(error_code)
{
  facets_mesher_.set_lock_ds(this->get_lock_data_structure());
  facets_mesher_.set_worksharing_ds(this->get_worksharing_data_structure());
  cells_mesher_.set_lock_ds(this->get_lock_data_structure());
  cells_mesher_.set_worksharing_ds(this->get_worksharing_data_structure());
}



template<class C3T3, class MC, class MD>
double
Mesher_3<C3T3,MC,MD>::
refine_mesh(std::string dump_after_refine_surface_prefix)
{
  CGAL::Real_timer timer;
  timer.start();
  double elapsed_time = 0.;

  // First surface mesh could modify c3t3 without notifying cells_mesher
  // So we have to ensure that no old cell will be left in c3t3
  // Second, the c3t3 object could have been corrupted since the last call
  // to `refine_mesh`, for example by inserting new vertices in the
  // triangulation.
  r_c3t3_.clear_cells_and_facets_from_c3t3();

  const Triangulation& r_tr = r_c3t3_.triangulation();

#ifndef CGAL_MESH_3_VERBOSE
  // Scan surface and refine it
  initialize();

#ifdef CGAL_MESH_3_PROFILING
  std::cerr << "Refining facets..." << std::endl;
  WallClockTimer t;
#endif
  facets_mesher_.refine(facets_visitor_);
  facets_mesher_.scan_edges();
  refinement_stage = REFINE_FACETS_AND_EDGES;
  facets_mesher_.refine(facets_visitor_);
  facets_mesher_.scan_vertices();
  refinement_stage = REFINE_FACETS_AND_EDGES_AND_VERTICES;
  facets_mesher_.refine(facets_visitor_);
#ifdef CGAL_MESH_3_PROFILING
  double facet_ref_time = t.elapsed();
  std::cerr << "==== Facet refinement: " << facet_ref_time << " seconds ===="
            << std::endl << std::endl;
# ifdef CGAL_MESH_3_EXPORT_PERFORMANCE_DATA
    // If it's parallel but the refinement is forced to sequential, we don't
    // output the value
#   ifndef CGAL_DEBUG_FORCE_SEQUENTIAL_MESH_REFINEMENT
  CGAL_MESH_3_SET_PERFORMANCE_DATA("Facets_time", facet_ref_time);
#   endif
# endif
#endif

#if defined(CHECK_AND_DISPLAY_THE_NUMBER_OF_BAD_ELEMENTS_IN_THE_END)
  std::cerr << std::endl
    << "===============================================================" << std::endl
    << "=== CHECK_AND_DISPLAY_THE_NUMBER_OF_BAD_ELEMENTS_IN_THE_END ===" << std::endl;
  display_number_of_bad_elements();
  std::cerr
    << "==============================================================="
    << std::endl << std::endl;
#endif

  // Then activate facet to surface visitor (surface could be
  // refined again if it is encroached)
  facets_visitor_.activate();

  dump_c3t3(r_c3t3_, dump_after_refine_surface_prefix);

  if(maximal_number_of_vertices_ == 0 ||
     r_tr.number_of_vertices() < maximal_number_of_vertices_)
  {
    // Then scan volume and refine it
    cells_mesher_.scan_triangulation();
    refinement_stage = REFINE_ALL;
#ifdef CGAL_MESH_3_PROFILING
    std::cerr << "Refining cells..." << std::endl;
    t.reset();
#endif
    cells_mesher_.refine(cells_visitor_);
#ifdef CGAL_MESH_3_PROFILING
    double cell_ref_time = t.elapsed();
    std::cerr << "==== Cell refinement: " << cell_ref_time << " seconds ===="
              << std::endl << std::endl;
# ifdef CGAL_MESH_3_EXPORT_PERFORMANCE_DATA
    // If it's parallel but the refinement is forced to sequential, we don't
    // output the value
#   ifndef CGAL_DEBUG_FORCE_SEQUENTIAL_MESH_REFINEMENT
  CGAL_MESH_3_SET_PERFORMANCE_DATA("Cells_refin_time", cell_ref_time);
#   endif
# endif
#endif

#if defined(CGAL_MESH_3_VERBOSE) || defined(CGAL_MESH_3_PROFILING)
    std::cerr
      << "Vertices: " << r_c3t3_.triangulation().number_of_vertices() << std::endl
      << "Facets  : " << r_c3t3_.number_of_facets_in_complex() << std::endl
      << "Tets    : " << r_c3t3_.number_of_cells_in_complex() << std::endl;
#endif
  } // end test of `maximal_number_of_vertices`
#else // ifdef CGAL_MESH_3_VERBOSE
  std::cerr << "Start surface scan...";
  initialize();
  std::cerr << "end scan. [Bad facets:" << facets_mesher_.size() << "]";
  std::cerr << std::endl << std::endl;
  elapsed_time += timer.time();
  timer.stop(); timer.reset(); timer.start();

  int nbsteps = 0;

  std::cerr << "Refining Surface...\n";
  std::cerr << "Legend of the following line: "
            << "(#vertices,#steps," << cells_mesher_.debug_info_header()
            << ")\n";

  std::cerr << "(" << r_tr.number_of_vertices() << ","
            << nbsteps << "," << cells_mesher_.debug_info() << ")";

  while ( ! facets_mesher_.is_algorithm_done() &&
          ( maximal_number_of_vertices_ == 0 ||
            maximal_number_of_vertices_ >= r_tr.number_of_vertices()) )
  {
    facets_mesher_.one_step(facets_visitor_);
    std::cerr
    << boost::format("\r             \r"
                     "(%1%,%2%,%3%) (%|4$.1f| vertices/s)")
    % r_tr.number_of_vertices()
    % nbsteps % cells_mesher_.debug_info()
    % (nbsteps / timer.time());
    ++nbsteps;
  }
  std::cerr << std::endl;
  std::cerr << "Total refining surface time: " << timer.time() << "s" << std::endl;
  std::cerr << std::endl;

  elapsed_time += timer.time();
  timer.stop(); timer.reset(); timer.start();
  nbsteps = 0;

  facets_visitor_.activate();
  dump_c3t3(r_c3t3_, dump_after_refine_surface_prefix);
  std::cerr << "Start volume scan...";
  cells_mesher_.scan_triangulation();
  refinement_stage = REFINE_ALL;
  std::cerr << "end scan. [Bad tets:" << cells_mesher_.size() << "]";
  std::cerr << std::endl << std::endl;
  elapsed_time += timer.time();
  timer.stop(); timer.reset(); timer.start();

  std::cerr << "Refining...\n";
  std::cerr << "Legend of the following line: "
            << "(#vertices,#steps," << cells_mesher_.debug_info_header()
            << ")\n";
  std::cerr << "(" << r_tr.number_of_vertices() << ","
            << nbsteps << "," << cells_mesher_.debug_info() << ")";

  while ( ! cells_mesher_.is_algorithm_done()  &&
          ( maximal_number_of_vertices_ == 0 ||
            maximal_number_of_vertices_ >= r_tr.number_of_vertices()) )
  {
    cells_mesher_.one_step(cells_visitor_);
    std::cerr
        << boost::format("\r             \r"
                     "(%1%,%2%,%3%) (%|4$.1f| vertices/s)")
        % r_tr.number_of_vertices()
        % nbsteps % cells_mesher_.debug_info()
        % (nbsteps / timer.time());
    ++nbsteps;
  }
  std::cerr << std::endl;

  std::cerr << "Total refining volume time: " << timer.time() << "s" << std::endl;
  std::cerr << "Total refining time: " << timer.time()+elapsed_time << "s" << std::endl;
  std::cerr << std::endl;
#endif

  if(maximal_number_of_vertices_ != 0 &&
     r_tr.number_of_vertices() >= maximal_number_of_vertices_ &&
     error_code_ != 0)
  {
    *error_code_ = CGAL_MESH_3_MAXIMAL_NUMBER_OF_VERTICES_REACHED;
  }

  timer.stop();
  elapsed_time += timer.time();

#if defined(CHECK_AND_DISPLAY_THE_NUMBER_OF_BAD_ELEMENTS_IN_THE_END) \
 || defined(SHOW_REMAINING_BAD_ELEMENT_IN_RED)
  std::cerr << std::endl
    << "===============================================================" << std::endl
    << "=== CHECK_AND_DISPLAY_THE_NUMBER_OF_BAD_ELEMENTS_IN_THE_END ===" << std::endl;
  display_number_of_bad_elements();
  std::cerr
    << "==============================================================="
    << std::endl << std::endl;
#endif

  return elapsed_time;
}


template<class C3T3, class MC, class MD>
void
Mesher_3<C3T3,MC,MD>::
initialize()
{
#ifdef CGAL_MESH_3_PROFILING
  std::cerr << "Initializing... ";
  WallClockTimer t;
#endif
  //=====================================
  // Bounding box estimation
  //=====================================
#if defined(CGAL_LINKED_WITH_TBB) || \
    defined(CGAL_SEQUENTIAL_MESH_3_ADD_OUTSIDE_POINTS_ON_A_FAR_SPHERE)

#ifndef CGAL_SEQUENTIAL_MESH_3_ADD_OUTSIDE_POINTS_ON_A_FAR_SPHERE
  if(boost::is_convertible<Concurrency_tag, Parallel_tag>::value)
#endif // If that macro is defined, then estimated_bbox must be initialized
  {
    Base::set_bbox(r_oracle_.bbox());
  }
#endif // CGAL_LINKED_WITH_TBB||"sequential use far sphere"

  //========================================
  // Initialization: parallel or sequential
  //========================================

#ifdef CGAL_LINKED_WITH_TBB
  // Parallel
  if (boost::is_convertible<Concurrency_tag, Parallel_tag>::value)
  {
    // we're not multi-thread, yet
    r_c3t3_.triangulation().set_lock_data_structure(0);

# ifndef CGAL_PARALLEL_MESH_3_DO_NOT_ADD_OUTSIDE_POINTS_ON_A_FAR_SPHERE

    if (r_c3t3_.number_of_far_points() == 0 && r_c3t3_.number_of_facets() == 0)
    {
      const Bbox_3 &bbox = r_oracle_.bbox();

      // Compute radius for far sphere
      const double xdelta = bbox.xmax()-bbox.xmin();
      const double ydelta = bbox.ymax()-bbox.ymin();
      const double zdelta = bbox.zmax()-bbox.zmin();
      const double radius = 5. * std::sqrt(xdelta*xdelta +
                                           ydelta*ydelta +
                                           zdelta*zdelta);
      const Vector center(
        bbox.xmin() + 0.5*xdelta,
        bbox.ymin() + 0.5*ydelta,
        bbox.zmin() + 0.5*zdelta);
#  ifdef CGAL_CONCURRENT_MESH_3_VERBOSE
      std::cerr << "Adding points on a far sphere (radius = " << radius <<")...";
#  endif
      Random_points_on_sphere_3<Bare_point> random_point(radius);
      const int NUM_PSEUDO_INFINITE_VERTICES = static_cast<int>(
        float(tbb::task_scheduler_init::default_num_threads())
        * Concurrent_mesher_config::get().num_pseudo_infinite_vertices_per_core);
      for (int i = 0 ; i < NUM_PSEUDO_INFINITE_VERTICES ; ++i, ++random_point)
        r_c3t3_.add_far_point(r_c3t3_.triangulation().geom_traits().construct_weighted_point_3_object()
                              (r_c3t3_.triangulation().geom_traits().construct_translated_point_3_object()(*random_point, center)));
    
#  ifdef CGAL_CONCURRENT_MESH_3_VERBOSE
      std::cerr << "done." << std::endl;
#  endif
    }
# endif // CGAL_PARALLEL_MESH_3_DO_NOT_ADD_OUTSIDE_POINTS_ON_A_FAR_SPHERE

#ifdef CGAL_MESH_3_PROFILING
    double init_time = t.elapsed();
    std::cerr << "done in " << init_time << " seconds." << std::endl;
#endif

    // Scan triangulation
    facets_mesher_.scan_triangulation();
    refinement_stage = REFINE_FACETS;

    // From now on, we're multi-thread
    r_c3t3_.triangulation().set_lock_data_structure(get_lock_data_structure());
  }
  // Sequential
  else
#endif // CGAL_LINKED_WITH_TBB
  {
#ifdef CGAL_SEQUENTIAL_MESH_3_ADD_OUTSIDE_POINTS_ON_A_FAR_SPHERE
    if (r_c3t3_.number_of_far_points() == 0 && r_c3t3_.number_of_facets() == 0)
    {
      const Bbox_3 &bbox = r_oracle_.bbox();

      // Compute radius for far sphere
      const double xdelta = bbox.xmax()-bbox.xmin();
      const double ydelta = bbox.ymax()-bbox.ymin();
      const double zdelta = bbox.zmax()-bbox.zmin();
      const double radius = 5. * std::sqrt(xdelta*xdelta +
                                           ydelta*ydelta +
                                           zdelta*zdelta);
      const Vector center(
        bbox.xmin() + 0.5*xdelta,
        bbox.ymin() + 0.5*ydelta,
        bbox.zmin() + 0.5*zdelta);
# ifdef CGAL_MESH_3_VERBOSE
      std::cerr << "Adding points on a far sphere (radius = " << radius << ")...";
# endif
      Random_points_on_sphere_3<Bare_point> random_point(radius);
      const int NUM_PSEUDO_INFINITE_VERTICES = 12*2;
      for (int i = 0 ; i < NUM_PSEUDO_INFINITE_VERTICES ; ++i, ++random_point)
        r_c3t3_.add_far_point(r_c3t3_.triangulation().geom_traits().construct_weighted_point_3_object()
                              (r_c3t3_.triangulation().geom_traits().construct_translated_point_3_object()(*random_point, center)));
# ifdef CGAL_MESH_3_VERBOSE
      std::cerr << "done." << std::endl;
# endif
    }
#endif // CGAL_SEQUENTIAL_MESH_3_ADD_OUTSIDE_POINTS_ON_A_FAR_SPHERE

#ifdef CGAL_MESH_3_PROFILING
    double init_time = t.elapsed();
    std::cerr << "done in " << init_time << " seconds." << std::endl;
#endif

    // Scan triangulation
    facets_mesher_.scan_triangulation();
    refinement_stage = REFINE_FACETS;
  }
}


template<class C3T3, class MC, class MD>
void
Mesher_3<C3T3,MC,MD>::
fix_c3t3()
{
  if ( ! facets_visitor_.is_active() )
  {
    cells_mesher_.scan_triangulation();
  }
}


template<class C3T3, class MC, class MD>
void
Mesher_3<C3T3,MC,MD>::
display_number_of_bad_elements()
{
  int nf = facets_mesher_.number_of_bad_elements();
  int nc = cells_mesher_.number_of_bad_elements();
  std::cerr << "Bad facets: " << nf << " - Bad cells: " << nc << std::endl;
}

template<class C3T3, class MC, class MD>
void
Mesher_3<C3T3,MC,MD>::
one_step()
{
  if ( ! facets_visitor_.is_active() )
  {
    facets_mesher_.one_step(facets_visitor_);

    if ( facets_mesher_.is_algorithm_done() )
    {
      switch(refinement_stage) {
      case REFINE_FACETS:
        facets_mesher_.scan_edges();
        refinement_stage = REFINE_FACETS_AND_EDGES;
        if (!facets_mesher_.is_algorithm_done()) break;
        CGAL_FALLTHROUGH;
      case REFINE_FACETS_AND_EDGES:
        facets_mesher_.scan_vertices();
        refinement_stage = REFINE_FACETS_AND_EDGES_AND_VERTICES;
        if (!facets_mesher_.is_algorithm_done()) break;
        CGAL_FALLTHROUGH;
      default:
        facets_visitor_.activate();
        cells_mesher_.scan_triangulation();
        refinement_stage = REFINE_ALL;
      }
    }
  }
  else
  {
    CGAL_assertion(refinement_stage == REFINE_ALL);
    cells_mesher_.one_step(cells_visitor_);
  }
}

template<class C3T3, class MC, class MD>
bool
Mesher_3<C3T3,MC,MD>::
is_algorithm_done()
{
  return cells_mesher_.is_algorithm_done();
}


#ifdef CGAL_MESH_3_MESHER_STATUS_ACTIVATED
template<class C3T3, class MC, class MD>
typename Mesher_3<C3T3,MC,MD>::Mesher_status
Mesher_3<C3T3,MC,MD>::
status() const
{
  return Mesher_status(r_c3t3_.triangulation().number_of_vertices(),
                       facets_mesher_.queue_size(),
                       cells_mesher_.queue_size());
}
#endif

template<class C3T3, class MC, class MD>
inline
std::string
Mesher_3<C3T3,MC,MD>::debug_info() const
{
  return cells_mesher_.debug_info();
}

template<class C3T3, class MC, class MD>
inline
std::string
Mesher_3<C3T3,MC,MD>::debug_info_header() const
{
  return cells_mesher_.debug_info_header();
}

}  // end namespace Mesh_3

}  // end namespace CGAL


#endif // CGAL_MESH_3_MESHER_3_H
