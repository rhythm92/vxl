// This is brl/bbas/imesh/imesh_detection.h
#ifndef imesh_detection_h_
#define imesh_detection_h_

//:
// \file
// \brief Detect features on meshes
// \author Matt Leotta (mleotta@lems.brown.edu)
// \date 5/29/08
//
// \verbatim
//  Modifications
// \endverbatim


#include "imesh_mesh.h"
#include <vcl_vector.h>

//: Return the indices of half edges that are on the mesh boundary
// the results are organized into loops
vcl_vector<vcl_vector<unsigned int> >
imesh_detect_boundary_loops(const imesh_half_edge_set& half_edges);


//: Trace half edges that have been selected into loops
// return true if all half edges form loops
// the loops are returned in \param loops as vectors of half edge indices
bool
imesh_trace_half_edge_loops(const imesh_half_edge_set& half_edges,
                            const vcl_vector<bool>& flags,
                            vcl_vector<vcl_vector<unsigned int> >& loops);

//: Return the indices of contour half edges as seen from direction \param dir
// the results are organized into loops
vcl_vector<vcl_vector<unsigned int> >
imesh_detect_contour_generator(const imesh_mesh& mesh, const vgl_vector_3d<double>& dir);

//: Mark contour half edges as seen from direction \param dir
//: For each contour edge the half edge with the face normal opposing dir is marked
vcl_vector<bool>
imesh_detect_contours(const imesh_mesh& mesh, vgl_vector_3d<double> dir);


//: Return the indices of contour half edges as seen from center of projection \param pt
// the results are organized into loops
vcl_vector<vcl_vector<unsigned int> >
imesh_detect_contour_generator(const imesh_mesh& mesh, const vgl_point_3d<double>& pt);

//: Mark contour half edges as seen from center of projection \param pt
//: For each contour edge the half edge with the face normal opposing dir is marked
vcl_vector<bool>
imesh_detect_contours(const imesh_mesh& mesh, const vgl_point_3d<double>& pt);




#endif // imesh_detection_h_
