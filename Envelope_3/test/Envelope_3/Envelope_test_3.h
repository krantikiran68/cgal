// Copyright (c) 2005  Tel-Aviv University (Israel).
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
// $Source: /CVSROOT/CGAL/Packages/Envelope_3/include/CGAL/Envelope_test_3.h,v $
// $Revision$ $Date$
// $Name:  $
// SPDX-License-Identifier: GPL-3.0+
//
// Author(s)     : Michal Meyerovitch     <gorgymic@post.tau.ac.il>

#ifndef CGAL_ENVELOPE_TEST_3_H
#define CGAL_ENVELOPE_TEST_3_H

#include <CGAL/Envelope_3/Envelope_base.h>
#include "Envelope_test_overlay_functor.h"
#include <CGAL/Envelope_3/Envelope_overlay_2.h>

#include <CGAL/Arr_walk_along_line_point_location.h>
#include <CGAL/Object.h>
#include <CGAL/enum.h>

#include <iostream>
#include <cassert>
#include <list>
#include <set>
#include <vector>
#include <map>

//#define CGAL_DEBUG_ENVELOPE_TEST_3

// this is a very trivial and not efficient algorithm for computing the lower envelope
// of general surfaces in 3d, used for testing.
// The algorithm projects the surfaces on the plane, and projects all the intersections
// between surfaces, to get an arrangement that is a partition of the real envelope.
// Then it computes for each part in the arragement the surfaces on the envelope over it
// by comparing them all.

namespace CGAL {

template <class EnvelopeTraits_3, class MinimizationDiagram_2>
class Envelope_test_3
{
public:
  typedef EnvelopeTraits_3                                           Traits;
  typedef typename Traits::Surface_3                                 Surface_3;
  typedef typename Traits::Xy_monotone_surface_3                     Xy_monotone_surface_3;

  typedef MinimizationDiagram_2                                      Minimization_diagram_2;
  typedef typename Minimization_diagram_2::Point_2                   Point_2;
  typedef typename Minimization_diagram_2::X_monotone_curve_2        X_monotone_curve_2;
  typedef typename Traits::Curve_2                                   Curve_2;

protected:

  typedef Envelope_test_overlay_functor<Minimization_diagram_2>       Overlay_functor;
  typedef Envelope_overlay_2<Minimization_diagram_2, Overlay_functor> Overlay_2;
  
  typedef Arr_walk_along_line_point_location<Minimization_diagram_2> Md_point_location;

  typedef typename Minimization_diagram_2::Halfedge_const_iterator   Halfedge_const_iterator;
  typedef typename Minimization_diagram_2::Halfedge_const_handle     Halfedge_const_handle;
  typedef typename Minimization_diagram_2::Halfedge_handle           Halfedge_handle;
  typedef typename Minimization_diagram_2::Halfedge_iterator         Halfedge_iterator;
  typedef typename Minimization_diagram_2::Vertex_const_handle       Vertex_const_handle;
  typedef typename Minimization_diagram_2::Vertex_handle             Vertex_handle;
  typedef typename Minimization_diagram_2::Vertex_iterator           Vertex_iterator;
  typedef typename Minimization_diagram_2::Face_handle               Face_handle;
  typedef typename Minimization_diagram_2::Face_const_iterator       Face_const_iterator;
  typedef typename Minimization_diagram_2::Face_const_handle         Face_const_handle;
  typedef typename Minimization_diagram_2::Face_iterator             Face_iterator;
  typedef typename Minimization_diagram_2::Ccb_halfedge_circulator   Ccb_halfedge_circulator;
  typedef typename Minimization_diagram_2::Inner_ccb_iterator        Hole_iterator;
  typedef typename Minimization_diagram_2::Dcel::Face_data_iterator  Face_data_iterator;

  typedef std::pair<X_monotone_curve_2, 
    typename EnvelopeTraits_3::Multiplicity>                         Intersection_curve;

public:
  // c'tor
  Envelope_test_3()
  {    
  }

  // virtual destructor.
  virtual ~Envelope_test_3(){}
  
  template <class SurfaceIterator>
  void construct_lu_envelope(SurfaceIterator begin, 
                             SurfaceIterator end, 
                             Minimization_diagram_2 &result)
  {
    if (begin == end)
    {
      return; // result is empty
    }

    std::vector<Xy_monotone_surface_3> surfaces;
    SurfaceIterator si = begin;
    for(; si != end; ++si)
    {
      surfaces.push_back(*si);
    }

    Md_point_location pl(result);

    int number_of_surfaces = surfaces.size();
    int j;
    std::list<X_monotone_curve_2> curves_col;
    std::list<Point_2> points_col;
    typename std::list<Curve_2>::iterator boundary_it;
    for(int i=0; i<number_of_surfaces; ++i)
    {
      Xy_monotone_surface_3 &cur_surface = surfaces[i];
      // first insert all the projected curves of the boundary of the current surface
      // collect the curve in this list, and use sweepline at the end
      std::list<Object>                            boundary_list;
      typedef std::pair<X_monotone_curve_2, Oriented_side> Boundary_xcurve;

      typedef std::list<Object>::const_iterator Boundary_iterator;
      traits.construct_projected_boundary_2_object()(cur_surface, std::back_inserter(boundary_list));
      for(Boundary_iterator boundary_it = boundary_list.begin();
          boundary_it != boundary_list.end();
          ++boundary_it)
      {
        const Object& obj = *boundary_it;
        Boundary_xcurve boundary_cv;
        assert(assign(boundary_cv, obj));
        assign(boundary_cv, obj);
        curves_col.push_back(boundary_cv.first);
      }
      
      // second, intersect it with all surfaces before it
      Object cur_obj;
      for(j=0; j<i; ++j)
      {
        Xy_monotone_surface_3& prev_surface = surfaces[j];

        std::vector<Object> inter_objs;
        traits.construct_projected_intersections_2_object()(cur_surface, prev_surface, std::back_inserter(inter_objs));

        // we collect all intersections and use sweep to insert them
        Point_2 point;
        Intersection_curve curve;
        for(unsigned int k=0; k<inter_objs.size(); ++k)
        {
          cur_obj = inter_objs[k];
          assert(!cur_obj.is_empty());
          if (CGAL::assign(point, cur_obj))
          {
            #ifdef CGAL_DEBUG_ENVELOPE_TEST_3
              std::cout << "intersection between surfaces is a point: " << point << std::endl;
            #endif
            //insert_vertex(result, point, pl);
            points_col.push_back(point);
          }
          else if (CGAL::assign(curve, cur_obj))
          {
            curves_col.push_back(curve.first);
            /*#ifdef CGAL_DEBUG_ENVELOPE_TEST_3
              std::cout << "intersection between surfaces is a curve: " << curve.first << std::endl;
            #endif
            std::list<Object> objs;
            traits.make_x_monotone_2_object()(curve.first, std::back_inserter(objs));
            std::list<Object>::iterator itr;
            for(itr = objs.begin(); itr != objs.end(); ++itr)
            {
              X_monotone_curve_2 curr_cv;
              assert(assign(curr_cv, *itr));
              assign(curr_cv, *itr);
              curves_col.push_back(curr_cv);
            }*/
            //insert(result, curve.first, pl);
          }
          else
          {
            assert_msg(false, "wrong intersection type");
          }
        }
      }
    }

    // insert the curves
    insert(result, curves_col.begin(), curves_col.end());
    // insert the points
    typename std::list<Point_2>::iterator pit = points_col.begin();
    for(; pit != points_col.end(); ++pit)
      insert_point(result, *pit, pl);
      
    m_result = &result;

    // now, foreach vertex, edge and face, we should determine which surfaces are minimal over it.

    // update vertices' data
    Vertex_iterator vi = result.vertices_begin();
    for(; vi != result.vertices_end(); ++vi)
    {
      Vertex_handle vh = vi;
      // first we find the surfaces that are defined over the vertex
      std::list<Xy_monotone_surface_3> defined_surfaces;
      typename Traits::Is_defined_over is_defined_over = traits.is_defined_over_object();      
      for(int i=0; i<number_of_surfaces; ++i)
        if (is_defined_over(vh->point(), surfaces[i]))
          defined_surfaces.push_back(surfaces[i]);

      // now compare them over the vertex
      set_minimum_over_vertex(vh, defined_surfaces.begin(), defined_surfaces.end());
    }
    
    // update edges' data
    Halfedge_iterator hi = result.halfedges_begin();
    for(; hi != result.halfedges_end(); ++hi, ++hi)
    {
      Halfedge_handle hh = hi;
      // first we find the surfaces that are defined over the egde
      std::list<Xy_monotone_surface_3> defined_surfaces;
      for(int i=0; i<number_of_surfaces; ++i)
        if (is_surface_defined_over_edge(hh, surfaces[i]))
          defined_surfaces.push_back(surfaces[i]);

      // now compare them over the edge
      set_minimum_over_edge(hh, defined_surfaces.begin(), defined_surfaces.end());
    }
    
    // update faces' data

    // init current face for caching of computation
    current_face = Face_handle();

    Face_iterator fi;
    for (fi = result.faces_begin(); fi != result.faces_end(); ++fi)
    {
      #ifdef CGAL_DEBUG_ENVELOPE_TEST_3
        std::cout << "deal with face" << std::endl;
      #endif
      
      Face_handle fh = fi;
      // first we find the surfaces that are defined over the face
      std::list<Xy_monotone_surface_3> defined_surfaces;
      for (int i=0; i<number_of_surfaces; ++i)
        if (is_surface_defined_over_face(fh, surfaces[i]))
          defined_surfaces.push_back(surfaces[i]);
      
      // now compare them over the face
      set_minimum_over_face(fh, defined_surfaces.begin(),
                            defined_surfaces.end());
    }
  }

  /*!
   * compare the 2 envelopes by overlaying them, and then comparing the
   * surfaces over the faces of the result map.
   *
   * \todo The overlay compares the data using assertions. This should be
   * replaced, but since we want to terminate the overlay once we
   * determine that the 2 diagrams differ, we cannot simply remove the
   * assertions. One option is to generate an exeption and catch it.
   */
  bool compare_diagrams(Minimization_diagram_2 &test_env,
                        Minimization_diagram_2 &env)
  {
    Minimization_diagram_2 overlay_map;
    overlay(test_env, env, overlay_map);
    return true;
  }

protected:

  // fill the vertex with the surface on the envelope
  // all the surfaces are known to be defined over the vertex' point
  template <class SurfaceIterator>
  void set_minimum_over_vertex(const Vertex_handle& v,
                               SurfaceIterator begin, SurfaceIterator end)
  {
    if (begin == end)
      v->set_no_data();
    else
    {
      SurfaceIterator si = begin;
      // we set the first surface as the minimum, and then compare all the others
      v->set_data(*si);
      ++si;
      for(; si != end; ++si)
      {
        Comparison_result cr = traits.compare_z_at_xy_3_object()(v->point(), v->get_data(), *si);
        if (cr == EQUAL)
          v->add_data(*si);
        else if (cr == LARGER)
          v->set_data(*si); // this erases all surfaces from vertex's list
        // else - new surface has no affect on the envelope
      }
    }
  }

  // fill the edge with the surface on the envelope
  // all the surfaces are known to be defined over the edge's curve
  template <class SurfaceIterator>
  void set_minimum_over_edge(const Halfedge_handle& h, SurfaceIterator begin, SurfaceIterator end)
  {
    if (begin == end)
      h->set_no_data();
    else
    {
      if (h != current_edge)
        compute_point_in_current_edge(h);
      
      SurfaceIterator si = begin;
      // we set the first surface as the minimum, and then compare all the others
      h->set_data(*si);
      ++si;
      for(; si != end; ++si)
      {
        Comparison_result cr = traits.compare_z_at_xy_3_object()(current_point_inside_edge,
                                                                              h->get_data(), *si);
        if (cr == EQUAL)
          h->add_data(*si);
        else if (cr == LARGER)
          h->set_data(*si); // this erases all surfaces from halfedge's list
        // else - new surface has no affect on the envelope
      }
      // set twin's data
      h->twin()->set_data(h->begin_data(), h->end_data());
    }
  }  
  // fill the face with the surface on the envelope
  // the surfaces are known to not intersect inside the face
  // (but might intersect on its edges)
  template <class SurfaceIterator>
  void set_minimum_over_face(const Face_handle& face, SurfaceIterator begin, SurfaceIterator end)
  {
    if (face->is_unbounded() || begin == end)
    {
      // a special case - no surface over the unbounded face, and when there are no surfaces at all
      face->set_no_data();
    }
    else
    {
      SurfaceIterator si = begin;
      // we set the first surface as the minimum, and then compare all the others
      face->set_data(*si);
      ++si;
      for(; si != end; ++si)
      {
        Comparison_result cr = compare_surfaces_over_face(face, face->get_data(), *si);
        if (cr == EQUAL)
          face->add_data(*si);
        else if (cr == LARGER)
          face->set_data(*si); // this erases all surfaces from face's list
        // else - new surface has no affect on the envelope
      }
    }  
  }

  // compare surfaces over face
  // return SMALLER if the first surface is closer to the envelope
  //        LARGER if the second surface is closer to the envelope
  //        EQUAL otherwise
  // this is version 2 which uses a calculated point inside the face
  Comparison_result compare_surfaces_over_face(const Face_handle& face,
                                               const Xy_monotone_surface_3 &surf1,
                                               const Xy_monotone_surface_3& surf2)
  {
    assert(!face->is_unbounded());
    Comparison_result cur_res;
    if (face != current_face)
    compute_point_in_current_face(face);

    cur_res = traits.compare_z_at_xy_3_object()(current_point,surf1,surf2);
    
    #ifdef CGAL_DEBUG_ENVELOPE_TEST_3
      std::cout << "for comparison inside face, current result = " << cur_res << std::endl;
    #endif

    return cur_res;
  }

  // check if the surface is defines over the edge
  bool is_surface_defined_over_edge(const Halfedge_handle& h, Xy_monotone_surface_3 &surf)
  {
    // check it over a point inside the edge's curve
    if (h != current_edge)
      compute_point_in_current_edge(h);

    bool result = traits.is_defined_over_object()(current_point_inside_edge, surf);
    return result;
  }
  
  // check if the surface is defines over the face
  // this is version checks the point inside the face
  bool is_surface_defined_over_face(const Face_handle& face,
                                    Xy_monotone_surface_3 &surf)
  {
    // we always have bounded surfaces
    if (face->is_unbounded())
      return false;
      
    if (face != current_face)
      compute_point_in_current_face(face);

    bool result = traits.is_defined_over_object()(current_point,surf);

    return result;    
  }

  // compute a point inside the face of the arranegement
  Point_2 compute_point_inside_face(Minimization_diagram_2 &env, Face_handle face)
  {
    assert(!face->is_unbounded());
    
    #ifdef CGAL_DEBUG_ENVELOPE_TEST_3
      std::cout << "in compute point inside face" << std::endl;
    #endif

    // 1. find an edge on the outer ccb of the face that is not vertical
    Ccb_halfedge_circulator hec = face->outer_ccb();
    Ccb_halfedge_circulator hec_begin = hec;
    bool found = false;
    do {
      if (!traits.is_vertical_2_object()(hec->curve()))
      {
        found = true;
        continue;
      }
      hec++;
    } while(hec != hec_begin && !found);
    assert(found);

    Halfedge_handle found_hh = hec;
    
    // 2. find a point on this edge's curve that is not one of its vertices
    //    (we use the middle of the curve)
    Point_2 shoot_source = traits.construct_middle_point(found_hh->curve());

    // 3. ray shoot up or down, into the face
    //    and find the intersection point of the ray. the segment between
    //    the point from which we shoot, and the intersection point lies
    //    inside the face. we take its middle point as a point inside the face
    bool shoot_up = true;
// TODO_NEW_DESIGN - check this
//    if (traits.compare_x(found_hh->source()->point(), found_hh->target()->point()) == LARGER)
//      shoot_up = false;
    if (traits.equal_2_object()(found_hh->source()->point(),
                              traits.construct_max_vertex_2_object()(found_hh->curve())))
      shoot_up = false;

    Md_point_location pl(env);
    Object shoot_obj;
    Halfedge_const_handle shoot_hh;
    Vertex_const_handle shoot_vh;

    Point_2 shoot_target;
    
    if (shoot_up)
      shoot_obj = pl.ray_shoot_up(shoot_source);
    else
      shoot_obj = pl.ray_shoot_down(shoot_source);

    if (assign(shoot_hh, shoot_obj))
    {
      shoot_target = traits.vertical_ray_shoot_2(shoot_source, shoot_hh->curve());
    }
    else if (assign(shoot_vh, shoot_obj))
    {
      shoot_target = (env.non_const_handle(shoot_vh))->point();
    }
    else
      CGAL_error(); // it cannot be the unbounded face
    
    Point_2 res_point = traits.construct_middle_point(shoot_source, shoot_target);

    #ifdef CGAL_DEBUG_ENVELOPE_TEST_3
      std::cout << "finished computing point in face" << std::endl;

      // just for checking, locate res_point in env to find face
      Object test_pl_obj = pl.locate(res_point);
      Face_const_handle test_fh;
      assert(assign(test_fh, test_pl_obj));
      assert(test_fh == face);
    #endif


    return res_point;
  }

  
  // compute a point inside the face saved in current_face
  // and put the result into current_point
  void compute_point_in_current_face(Face_handle face)
  {
    current_face = face;
    current_point = compute_point_inside_face(*m_result, current_face);
  }

  // compute a point inside the edge saved in current_edge
  // and put the result into current_point_inside_edge
  void compute_point_in_current_edge(Halfedge_handle h)
  {
    current_edge = h;
    current_point_inside_edge = traits.construct_middle_point(h->curve());
  }

protected:
  Overlay_2              overlay;
  Traits                 traits;
  Minimization_diagram_2 *m_result;

  Face_handle        current_face;
  Point_2            current_point;

  Halfedge_handle    current_edge;
  Point_2            current_point_inside_edge;
};

} //namespace CGAL

#endif 
