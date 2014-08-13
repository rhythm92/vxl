#include <volm/conf/volm_conf_query.h>
//:
// \file
#include <vcl_cassert.h>
#include <vcl_cmath.h>
#include <vcl_algorithm.h>
#include <bsol/bsol_algs.h>
#include <bpgl/bpgl_camera_utils.h>
#include <vul/vul_file.h>
#include <vil/vil_load.h>
#include <vil/vil_save.h>
#include <vsph/vsph_spherical_coord.h>
#include <vsph/vsph_sph_point_3d.h>
#include <vgl/vgl_polygon_scan_iterator.h>

// default constructor
volm_conf_query::volm_conf_query()
{
  ni_ = 0;  nj_ = 0;  ncam_ = 0;  nobj_ = 0;  nref_ = 0;
  dm_ = 0;
  cameras_.clear();  camera_strings_.clear();
  ref_obj_name_.clear();  conf_objects_.clear();
}

// create query from labeled depth map scene
volm_conf_query::volm_conf_query(volm_camera_space_sptr cam_space, depth_map_scene_sptr depth_scene)
{
  dm_ = depth_scene;
  ni_ = dm_->ni();  nj_ = dm_->nj();
  altitude_ = cam_space->altitude();
  // create reference objects from depth scene
  bool success = this->parse_ref_object(dm_);
  assert(success && "volm_conf_query: parse reference object list from depth scene failed");
  nref_ = (unsigned)this->ref_obj_name_.size();
  vcl_cout << nref_ << " reference configurational objects are loaded: ";
  for (unsigned i = 0; i < nref_; i++)
    vcl_cout << ref_obj_name_[i] << ' ';
  vcl_cout << '\n';
  // create cameras
  success = this->create_perspective_cameras(cam_space);
  assert(success && "volm_conf_query: construct perspective cameras from camera space failed");
  ncam_ = (unsigned)this->cameras_.size();
  vcl_cout << ncam_ << " cameras are created: " << vcl_endl;
  for (unsigned i = 0; i < ncam_; i++)
    vcl_cout << i << ": " << camera_strings_[i] << vcl_endl;
  // construct configurational object from 3d polygons
  success  = this->create_conf_object();
  assert(success && "volm_conf_query: construct configurational objects failed");
}

bool volm_conf_query::parse_ref_object(depth_map_scene_sptr dm)
{
  ref_obj_name_.clear();
  if (dm == 0)
    return false;
  // note that sky object should never be reference object
  if (dm->ground_plane().size())
    for (unsigned i = 0; i < dm->ground_plane().size(); i++)
      if ( dm->ground_plane()[i]->is_ref() )
        ref_obj_name_.push_back(dm->ground_plane()[i]->name());
  if (dm->scene_regions().size())
    for (unsigned i = 0; i < dm->scene_regions().size(); i++)
      if ( dm->scene_regions()[i]->is_ref() )
        ref_obj_name_.push_back(dm->scene_regions()[i]->name());
  if (ref_obj_name_.empty())
    return false;
  return true;
}

bool volm_conf_query::create_perspective_cameras(volm_camera_space_sptr cam_space)
{
  // iterate over valid cameras in the camera space
  // Note that ground plane construct has been applied on camera space
  cameras_.clear();  camera_strings_.clear();  camera_angles_.clear();
  vcl_vector<unsigned> const& valid_cams = cam_space->valid_indices();
  for (unsigned i = 0; i < valid_cams.size(); i++) {
    vpgl_perspective_camera<double> cam = cam_space->camera(valid_cams[i]);
    vcl_string cam_str = cam_space->get_string(valid_cams[i]);
    cameras_.push_back(cam);
    camera_strings_.push_back(cam_str);
    camera_angles_.push_back(cam_space->camera_angles(valid_cams[i]));
  }
  return true;
}


bool volm_conf_query::create_conf_object()
{
  conf_objects_.clear();
  // loop over each calibrated camera to construct the list of configurational objects
  for (unsigned cam_id =0;  cam_id < ncam_;  cam_id++)
  {
    vcl_cout << "\t creating configuration objects for camera: " << camera_strings_[cam_id] << vcl_endl;
    vpgl_perspective_camera<double> pcam = cameras_[cam_id];
    // obtain camera center
    vgl_homg_point_3d<double> cam_center = pcam.camera_center();
    // obtain the horizon line
    vgl_line_2d<double> h_line = bpgl_camera_utils::horizon(pcam);
    // create a map of volm_conf_object
    vcl_map<vcl_string, volm_conf_object_sptr> conf_object;
    vcl_map<vcl_string, vcl_pair<unsigned, unsigned> > conf_pixels;
    // only consider non-planar objects
    vcl_vector<depth_map_region_sptr> regions = dm_->scene_regions();
    vcl_cout << "\t camera center: " << cam_center << ", horizontal line: " << h_line << vcl_endl;
    for (unsigned r_idx = 0; r_idx < regions.size(); r_idx++)
    {
      //vcl_cout << "\t\t projecting " << regions[r_idx]->name() << "..." << vcl_flush << vcl_endl;
      vgl_polygon<double> poly = bsol_algs::vgl_from_poly(regions[r_idx]->region_2d());
      float theta = -1.0f, dist = -1.0f;
      unsigned i, j;
      // project all ground vertices on the polygon to 3-d world points if the vertex is under the horizon
      this->project(pcam, cam_center, h_line, poly, dist, theta, i, j);
      //vcl_cout << "\t\t min_dist: " << dist << ", phi: " << theta << ", pixel: " << i << "x" << j << vcl_flush << vcl_endl;
      if (theta < 0 && dist < 0)
        continue;
      // create a configurational object for it
      unsigned char land_id = regions[r_idx]->land_id();
      volm_conf_object_sptr conf_obj = new volm_conf_object(theta, dist, land_id);
      //vcl_cout << "conf_obj: ";  conf_obj->print(vcl_cout);
      conf_object.insert(vcl_pair<vcl_string, volm_conf_object_sptr>(regions[r_idx]->name(), conf_obj));
      vcl_pair<unsigned, unsigned> tmp_pair(i,j);
      conf_pixels.insert(vcl_pair<vcl_string, vcl_pair<unsigned, unsigned> >(regions[r_idx]->name(), tmp_pair));
    }
    // update the configurational object for current camera
    conf_objects_.push_back(conf_object);
    conf_objects_pixels_.push_back(conf_pixels);
  }

  return true;
}

void volm_conf_query::project(vpgl_perspective_camera<double> const& cam,
                              vgl_homg_point_3d<double> const& cam_center,
                              vgl_line_2d<double> const& horizon,
                              vgl_polygon<double> const& poly,
                              float& min_dist, float& phi, unsigned& i, unsigned& j)
{
  min_dist = -1.0f;  phi = -1.0f;  i = 0;  j=0;
  // only consider the first sheet
  unsigned n_vertices = poly[0].size();
  vcl_cout << "\t\tnumber of vertices: " << n_vertices << vcl_endl;
  vcl_map<float, float> pt_pairs;
  vcl_map<float, vcl_pair<unsigned, unsigned> > pt_pixels;
  for (unsigned v_idx = 0; v_idx < n_vertices; v_idx++) {
    double x = poly[0][v_idx].x();
    double y = poly[0][v_idx].y();
    double yl = line_coord(horizon, x);
    if (y < yl)  // given img point is above the horizon
      continue;
    // obtain the back project ray to calculate the distance
    vgl_ray_3d<double> ray = cam.backproject(x, y);
    // obtain the angular relative to camera x axis
    vgl_point_3d<double> cp(ray.direction().x(), ray.direction().y(), ray.direction().z());
    vsph_spherical_coord sph_coord;
    vsph_sph_point_3d sp;
    sph_coord.spherical_coord(cp, sp);
    // calculate the distance
    float dist = vcl_tan(vnl_math::pi - sp.theta_)*altitude_;
    pt_pairs.insert(vcl_pair<float, float>(dist,sp.phi_));
#if 0
    vcl_cout << "\t\tpixel: " << x << "x" << y << " is under horizon, has ray " << cp 
             << " and spherical coords: " << sp << " dist: " << dist << ", theta: " << sp.phi_ << vcl_flush << vcl_endl;
#endif
    pt_pixels.insert(vcl_pair<float, vcl_pair<unsigned, unsigned> >(dist, vcl_pair<unsigned, unsigned>((unsigned)x,(unsigned)y)));
  }
  if (pt_pairs.empty())  // no vertices projects to ground
    return;
  min_dist = pt_pairs.begin()->first;
  phi = pt_pairs[min_dist];
  
  i = pt_pixels[min_dist].first;
  j = pt_pixels[min_dist].second;
  //vcl_cout << "\t\tmin_dist: " << min_dist << ", phi: " << phi << ", pixel: " << i << "x" << j << vcl_flush << vcl_endl;
  return;
}

double volm_conf_query::line_coord(vgl_line_2d<double> const& line, double const& x)
{
  if (line.b() == 0)
    return 0.0;
  else
    return -line.a()/line.b()*x - line.c()/line.b();
}


bool volm_conf_query::visualize_ref_objs(vcl_string const& in_file, vcl_string const& out_folder)
{
  if (!vul_file::exists(in_file))
    return false;
  vil_image_view<vil_rgb<vxl_byte> > src_img = vil_load(in_file.c_str());
  vcl_vector<depth_map_region_sptr> regions = dm_->scene_regions();

  for (unsigned cam_id = 0; cam_id < ncam_; cam_id++) {
    vil_image_view<vil_rgb<vxl_byte> > img = src_img;
    // plot horizontal line
    vgl_line_2d<double> h_line = bpgl_camera_utils::horizon(cameras_[cam_id]);
    vcl_vector<vgl_point_2d<double> > h_line_pixels;
    for (unsigned x = 0; x < ni_; x++) {
      double y = (vcl_floor)(line_coord(h_line, x));
      h_line_pixels.push_back(vgl_point_2d<double>((double)x, y));
    }
    this->plot_line_into_image(img, h_line_pixels, 0, 0, 0, 6);
    // plot the non-ground depth map scenes first
    for (unsigned i = 0; i < regions.size(); i++) {
      vgl_polygon<double> poly = bsol_algs::vgl_from_poly(regions[i]->region_2d());
      poly[0].push_back(poly[0][0]);
      unsigned char r,g,b;
      double width;
      if (vcl_find(ref_obj_name_.begin(), ref_obj_name_.end(), regions[i]->name()) == ref_obj_name_.end()) {
        r = volm_osm_category_io::volm_land_table[regions[i]->land_id()].color_.r;
        g = volm_osm_category_io::volm_land_table[regions[i]->land_id()].color_.g;
        b = volm_osm_category_io::volm_land_table[regions[i]->land_id()].color_.b;
        width = 5.0;
      } else {
        r = 255; g = 255; b = 255; width = 7.0;
      }
      this->plot_line_into_image(img, poly[0],r,g,b,width);
    }
    // plot the configurational object
    for (vcl_map<vcl_string, vcl_pair<unsigned, unsigned> >::iterator mit = conf_objects_pixels_[cam_id].begin(); mit != conf_objects_pixels_[cam_id].end(); ++mit ) {
      unsigned char land_id = conf_objects_[cam_id][mit->first]->land();
      unsigned char r,g,b;
      double width;
      if (vcl_find(ref_obj_name_.begin(), ref_obj_name_.end(), mit->first) == ref_obj_name_.end()) {
        r = volm_osm_category_io::volm_land_table[land_id].color_.r;
        g = volm_osm_category_io::volm_land_table[land_id].color_.g;
        b = volm_osm_category_io::volm_land_table[land_id].color_.b;
        width = 20.0;
      }else {
        r = 255; g = 255; b = 255; width = 30.0;
      }
      this->plot_dot_into_image(img, vgl_point_2d<double>((double)mit->second.first, (double)mit->second.second),r,g,b,width);
    }
    vcl_string filename = vul_file::strip_extension(vul_file::strip_directory(in_file));
    vcl_string out_file = out_folder + "/" + filename + "_" + camera_strings_[cam_id] + ".tif";
    vil_save(img, out_file.c_str());
  }
  return true;

}

bool volm_conf_query::generate_top_views(vcl_string const& out_folder, vcl_string const& filename_pre)
{
  // ensure a maximum image size
  unsigned half_ni = 0;  unsigned half_nj = 0;
  for (unsigned cam_id = 0; cam_id < ncam_; cam_id++)
  {
    vcl_map<vcl_string, volm_conf_object_sptr> conf_obj = conf_objects_[cam_id];
    for (vcl_map<vcl_string, volm_conf_object_sptr>::iterator mit = conf_obj.begin();  mit != conf_obj.end();  ++mit) {
      float x = mit->second->dist() * vcl_cos(mit->second->theta());
      float y = mit->second->dist() * vcl_sin(mit->second->theta());
      if (half_ni < vcl_ceil(x))  half_ni = vcl_ceil(x);
      if (half_nj < vcl_ceil(y))  half_nj = vcl_ceil(y);
      vcl_cout << "obj " << mit->first << " has distance " << mit->second->dist() << " and angle " << mit->second->theta()
               << ", pixel " << vcl_ceil(x) << "x" << vcl_ceil(y)
               << ", img size" << 2*half_ni << "x" << 2*half_nj << vcl_endl;
    }
  }
  unsigned ni,nj;
  ni = 2*half_ni;
  nj = 2*half_nj;
  vcl_cout << "top views has image size: " << ni << "x" << nj << vcl_endl;
  for (unsigned cam_id = 0; cam_id < ncam_; cam_id++)
  {
    vcl_string cam_string = camera_strings_[cam_id];
    vil_image_view<vil_rgb<vxl_byte> > img(ni, nj);
    img.fill(vil_rgb<vxl_byte>(127,127,127));
    // perform the coordinate transformation (putting camera center to the image center)
    float xo = half_ni, yo = half_nj;
    // plot camera center
    this->plot_dot_into_image(img, vgl_point_2d<double>(xo,yo), 0, 0, 0, 5.0);
    
    vcl_map<vcl_string, volm_conf_object_sptr> conf_obj = conf_objects_[cam_id];
    for (vcl_map<vcl_string, volm_conf_object_sptr>::iterator mit = conf_obj.begin();  mit != conf_obj.end();  ++mit) {
      float xc = mit->second->dist() * vcl_cos(mit->second->theta());
      float yc = mit->second->dist() * vcl_sin(mit->second->theta());
      unsigned char r, g, b;  double width;
      unsigned char land_id = mit->second->land();
      if (vcl_find(ref_obj_name_.begin(), ref_obj_name_.end(), mit->first) == ref_obj_name_.end()) {
        r = volm_osm_category_io::volm_land_table[land_id].color_.r;
        g = volm_osm_category_io::volm_land_table[land_id].color_.g;
        b = volm_osm_category_io::volm_land_table[land_id].color_.b;
        width = 10.0;
      }else {
        r = 255; g = 255; b = 255; width = 25.0;
      }
      vcl_cout << "(" << xc << "," << yc << "), (" << xo << "," << yo << ") --> (" << xc+xo << "," << yo-yc << ")" << vcl_endl;
      this->plot_dot_into_image(img, vgl_point_2d<double>(xc+xo,yo-yc), r, g, b, width);
    }
    vcl_string out_file = out_folder + "/" + filename_pre + "_" + cam_string + ".tif";
    vcl_cout << "save image to " << out_file << vcl_endl;
    vil_save(img, out_file.c_str());
  }
  return true;
}

void volm_conf_query::plot_line_into_image(vil_image_view<vil_rgb<vxl_byte> >& image,
                                           vcl_vector<vgl_point_2d<double> > const& line,
                                           unsigned char const& r, unsigned char const& g, unsigned char const& b,
                                           double const& width)
{
  vgl_polygon<double> img_poly;
  volm_io_tools::expend_line(line, width, img_poly);
  vgl_polygon_scan_iterator<double> it(img_poly, true);
  for (it.reset(); it.next();  ) {
    int y = it.scany();
    for (int x = it.startx(); x <= it.endx(); ++x) {
      if ( x >= 0 && y >= 0 && x < (int)image.ni() && y < (int)image.nj()) {
        image(x, y) = vil_rgb<vxl_byte>(r, g, b);
      }
    }
  }
}

void volm_conf_query::plot_dot_into_image(vil_image_view<vil_rgb<vxl_byte> >& image,
                                          vgl_point_2d<double> const& pt,
                                          unsigned char const& r, unsigned char const& g, unsigned char const& b,
                                          double const& radius)
{
  vgl_polygon<float> img_poly;
  img_poly.new_sheet();
  vcl_set<float> angles;
  for (unsigned i = 0; i <= 36; i++)
    angles.insert(i*10.0f);
  angles.insert(45.0f);  angles.insert(135.0f);  angles.insert(225.0f);  angles.insert(315.0f);
  for (vcl_set<float>::iterator sit = angles.begin();  sit != angles.end();  ++sit) {
    float x = pt.x() + radius*vcl_cos(*sit*vnl_math::pi_over_180);
    float y = pt.y() + radius*vcl_sin(*sit*vnl_math::pi_over_180);
    img_poly.push_back(x, y);
  }
  vgl_polygon_scan_iterator<float> it(img_poly, true);
  for (it.reset(); it.next();  ) {
    int y = it.scany();
    for (int x = it.startx(); x <= it.endx(); ++x) {
      if ( x>=0 && y>=0 && x<(int)image.ni() && y<(int)image.nj())
        image(x,y) = vil_rgb<vxl_byte>(r, g, b);
    }
  }
}