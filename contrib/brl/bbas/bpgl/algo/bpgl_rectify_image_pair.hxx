#include <fstream>

#include "vgl/vgl_box_3d.h"
#include "vil/vil_load.h"
#include "vil/vil_convert.h"
#include "vil/vil_bilin_interp.h"
#include "vil/vil_bicub_interp.h"
#include "vnl/vnl_random.h"
#include "vnl/vnl_det.h"
#include "vnl/vnl_math.h"
#include "vnl/vnl_inverse.h"

#include <vpgl/algo/vpgl_camera_convert.h>
#include <vpgl/algo/vpgl_equi_rectification.h>
#include "bpgl_rectify_image_pair.h"
#include <vpgl/vpgl_fundamental_matrix.h>
#include <vpgl/vpgl_affine_fundamental_matrix.h>

template <class CAMT>
void bpgl_rectify_image_pair<CAMT>::set_valid_types(){
  valid_cam_types_.push_back("vpgl_affine_camera");
  valid_cam_types_.push_back("vpgl_perspective_camera");
}
template <class CAMT>
bool bpgl_rectify_image_pair<CAMT>::valid_type(std::string const& type){
  for(size_t i = 0; i<valid_cam_types_.size(); ++i)
    if(type == valid_cam_types_[i])
      return true;
  return false;
}
template <class CAMT>
bool bpgl_rectify_image_pair<CAMT>::
set_images_and_cams(vil_image_view_base_sptr const& view0, CAMT const& cam0,
                    vil_image_view_base_sptr const& view1, CAMT const& cam1)
{
  // convert images to float
  vil_image_view_base_sptr fview0 = vil_convert_cast(float(), view0);
  if (!fview0) {
    std::cerr << "can't convert view 0 to float" << std::endl;
    return false;
  }

  vil_image_view_base_sptr fview1 = vil_convert_cast(float(), view1);
  if (!fview1) {
    std::cerr << "can't convert view 1 to float" << std::endl;
    return false;
  }

  // save to instance
  fview0_ = *fview0;
  cam0_  = cam0;
  fview1_ = *fview1;
  cam1_  = cam1;
  return true;
}

template <class CAMT>
bool bpgl_rectify_image_pair<CAMT>::
set_images_and_cams(vil_image_resource_sptr const& resc0, CAMT const& cam0,
                    vil_image_resource_sptr const& resc1, CAMT const& cam1)
{
  // get image views
  auto view0 = resc0->get_view();
  if (!view0) {
    std::cerr << "null view from resource 0" << std::endl;
    return false;
  }

  auto view1 = resc1->get_view();
  if (!view1) {
    std::cerr << "null view from resource 1" << std::endl;
    return false;
  }

  // save to instance
  return this->set_images_and_cams(view0, cam0, view1, cam1);
}

template <class CAMT>
bool bpgl_rectify_image_pair<CAMT>::
load_camera(std::string const& cam_path, CAMT& cam)
{
  std::string ctype = cam.type_name();
  if(!this->valid_type(ctype)){
    std::cerr << "can't process camera of type " << ctype << std::endl;
    return false;
  }
  std::ifstream istr(cam_path);
  if (!istr) {
    std::cout << "Can't open camera path " << cam_path << std::endl;
    return false;
  }
  istr >> cam;
  return true;
}


template <class CAMT>
void bpgl_rectify_image_pair<CAMT>::
compute_warp_dimensions_offsets()
{
  double dni0 = fview0_.ni()-1, dnj0 = fview0_.nj()-1;
  double dni1 = fview1_.ni()-1, dnj1 = fview1_.nj()-1;
  std::vector<vnl_vector_fixed<double, 3> > corners_0(4), corners_1(4);
  std::vector<vnl_vector_fixed<double, 3> > Hcorners_0(4), Hcorners_1(4);
  corners_0[0] = vnl_vector_fixed<double, 3>(0,0,1);
  corners_0[1] = vnl_vector_fixed<double, 3>(dni0,0,1);
  corners_0[2] = vnl_vector_fixed<double, 3>(dni0,dnj0,1);
  corners_0[3] = vnl_vector_fixed<double, 3>(0,dnj0,1);

  corners_1[0] = vnl_vector_fixed<double, 3>(0,0,1);
  corners_1[1] = vnl_vector_fixed<double, 3>(dni1,0,1);
  corners_1[2] = vnl_vector_fixed<double, 3>(dni1,dnj1,1);
  corners_1[3] = vnl_vector_fixed<double, 3>(0,dnj1,1);
  for (size_t c = 0; c<4; ++c) {
    Hcorners_0[c] =  H0_*corners_0[c];
    Hcorners_0[c] /= Hcorners_0[c][2];
    Hcorners_1[c] =  H1_*corners_1[c];
    Hcorners_1[c] /= Hcorners_1[c][2];
  }
  double mini0 = std::numeric_limits<double>::max(), maxi0 = -mini0;
  double minj0 = std::numeric_limits<double>::max(), maxj0 = -minj0;
  double mini1 = std::numeric_limits<double>::max(), maxi1 = -mini1;
  double minj1 = std::numeric_limits<double>::max(), maxj1 = -minj1;
  for (size_t c = 0; c<4; ++c) {
    if(Hcorners_0[c][0] < mini0)
      mini0 = Hcorners_0[c][0];
    if(Hcorners_1[c][0] < mini1)
      mini1 = Hcorners_1[c][0];
    if(Hcorners_0[c][0] > maxi0)
      maxi0 = Hcorners_0[c][0];
    if(Hcorners_1[c][0] > maxi1)
      maxi1 = Hcorners_1[c][0];
    if(Hcorners_0[c][1] < minj0)
      minj0 = Hcorners_0[c][1];
    if(Hcorners_1[c][1] < minj1)
      minj1 = Hcorners_1[c][1];
    if(Hcorners_0[c][1] > maxj0)
      maxj0 = Hcorners_0[c][1];
    if(Hcorners_1[c][1] > maxj1)
      maxj1 = Hcorners_1[c][1];
  }
  double w0 = (maxi0-mini0), h0 = (maxj0-minj0);
  double w1 = (maxi1-mini1), h1 = (maxj1-minj1);
  double w = w0, h = h0;
  du_off_ = mini0; dv_off_ = minj0;
  if (w1<w) {
    w = w1;
    du_off_ = mini1;
  }
  if (h1<h) {
    h = h1;
    dv_off_ = minj1;
  }
  //double scaled_w = w*params_.upsample_scale_, scaled_h = h*params_.upsample_scale_;
  // replaced by scale_to_input_size()
  out_ni_ = static_cast<size_t>(w+0.5) +1;
  out_nj_ = static_cast<size_t>(h+0.5) +1;
}
template <class CAMT>
double bpgl_rectify_image_pair<CAMT>::scale_to_input_size(){
  this->compute_warp_dimensions_offsets();
  double ni0 = fview0_.ni(), nj0 = fview0_.nj();
  double ni1=  fview1_.ni(), nj1 = fview1_.nj();
  double w0 = std::max(ni0, ni1);
  double h0 = std::max(nj0, nj1);
  double sw = w0/out_ni_;
  double sh = h0/out_nj_;
  // 1.2 is geometric mean of 1 and sqrt(2)(45deg rotation)
  // as a compromise scale for a rotated image
  double s = 1.2*params_.upsample_scale_*std::max(sw, sh);
  out_ni_ *= s;
  out_nj_ *= s;
  return s;
}
template <class CAMT>
bool bpgl_rectify_image_pair<CAMT>::
compute_rectification(vgl_box_3d<double>const& scene_box, size_t n_points, double average_z)
{
  double min_x = scene_box.min_x();
  double min_y = scene_box.min_y();
  double width = scene_box.width(), height = scene_box.height();
  double ni0 = fview0_.ni(), nj0 = fview0_.nj();
  double ni1=  fview1_.ni(), nj1 = fview1_.nj();
  vnl_random rng;
  double z0 = 0.5*(scene_box.min_z() + scene_box.max_z());
  if (vnl_math::isfinite(average_z))
    z0 = average_z;
  std::vector< vnl_vector_fixed<double, 3> > img_pts0, img_pts1;
  double inside_pts = 0.0;
  for (unsigned i = 0; i < n_points; i++) {
    double x = rng.drand64()*width + min_x;  // sample in local coords
    double y = rng.drand64()*height + min_y;
    double u0, v0, u1, v1;

    cam0_.project(x,y,z0,u0,v0);
    bool proj_0_good = (u0 >= 0 && u0 < ni0 && v0 >= 0 && v0 < nj0);

    cam1_.project(x, y, z0, u1, v1);
    bool proj_1_good = (u1 >= 0 && u1 < ni1 && v1 >= 0 && v1 < nj1);

    // store points regardless of if they project into each image
    img_pts0.emplace_back(u0,v0,1);
    img_pts1.emplace_back(u1,v1,1);

    // but accumulate the points that intersect each image
    if (proj_0_good && proj_1_good)
      inside_pts += 1.0;
  }
  // if too few points have both images in common - problem
  double overlap_fraction = double(inside_pts) / double(n_points);
  if (overlap_fraction < params_.min_overlap_fraction_) {
    std::cout << "Fatal - an insufficient fraction of zero disparity plane points project into both images "
              << overlap_fraction << " < " << params_.min_overlap_fraction_<< std::endl;
    return false;
  }
  // sanity check
  bool epi_constraint = true;
  vnl_matrix_fixed<double, 3, 3> Fm;
  if(cam0_.type_name() == "vpgl_perspective_camera"){
    vnl_matrix_fixed<double, 3, 4> m0 = cam0_.get_matrix(), m1 = cam1_.get_matrix();
    vpgl_proj_camera<double> pc0(m0), pc1(m1);
    vpgl_fundamental_matrix<double> Fp(pc0, pc1);
    Fm = Fp.get_matrix();
  }else if(cam0_.type_name() == "vpgl_affine_camera"){
    vnl_matrix_fixed<double, 3, 4> m0 = cam0_.get_matrix(), m1 = cam1_.get_matrix();
    vpgl_affine_camera<double> A0(m0), A1(m1);
    vpgl_affine_fundamental_matrix<double> Fa(A0, A1);
    Fm = Fa.get_matrix();
  }else{
    std::cerr << "Can't rectify image camera of type " << cam0_.type_name() << std::endl;
    return false;
  }
  for (size_t k = 0; k < img_pts0.size(); ++k) {
    vnl_vector_fixed<double, 3> pr = img_pts0[0], line_l, pl = img_pts1[0];
    line_l = Fm * pr;
    //normalize line
    line_l /= sqrt(line_l[0] * line_l[0] + line_l[1] * line_l[1]);
    double dp = dot_product(line_l, pl);
    epi_constraint = epi_constraint && fabs(dp) < 1.0e-6;
  }
  if (!epi_constraint) {
    std::cout << "epipolar constraint doesn't hold for F matrix and sampled points" << std::endl;
    return false;
  }

  if(cam0_.type_name() == "vpgl_affine_camera"){
    vnl_matrix_fixed<double, 3, 4> m0 = cam0_.get_matrix(), m1 = cam1_.get_matrix();
    vpgl_affine_camera<double> A0(m0), A1(m1);
    if (!vpgl_equi_rectification::rectify_pair(A0, A1, img_pts0, img_pts1, H0_, H1_)) {
      std::cout << "vpgl equi rectification failed" << std::endl;
      return false;
    }
  }else if(cam0_.type_name() == "vpgl_perspective_camera"){
    if (!vpgl_equi_rectification::rectify_pair(cam0_, cam1_, img_pts0, img_pts1, H0_, H1_)) {
      std::cout << "vpgl equi rectification failed" << std::endl;
      return false;
    }
  }
  double singular_tol = 1.0e-6;
  if ((fabs(vnl_det(H0_)) < singular_tol) ||
      (fabs(vnl_det(H1_)) < singular_tol)) {
    std::cout << "vpgl rectification produced singular homography(s)" << std::endl;
    return false;
  }

  // second sanity check
  bool equal_y = true;
  for (size_t k = 0; k < img_pts0.size()&&equal_y; ++k) {
    vnl_vector_fixed<double, 3> p0 = img_pts0[k], hp0, p1 = img_pts1[k], hp1;
    hp0 = H0_*p0;  hp1 = H1_*p1;
    double y0 = hp0[1]/hp0[2], y1 = hp1[1]/hp1[2];
    double dy = fabs(y1-y0);
    if (dy>0.001)
      equal_y = false;
  }
  if (!equal_y) {
    std::cout << "homographies do not map to equal row positions" << std::endl;
    return false;
  }

  // third sanity check
  double x_dif_sum = 0.0;
  for (size_t k = 0; k < img_pts0.size(); ++k) {
    vnl_vector_fixed<double, 3> p0 = img_pts0[k], hp0, p1 = img_pts1[k], hp1;
    hp0 = H0_*p0;  hp1 = H1_*p1;
    double x0 = hp0[0]/hp0[2], x1 = hp1[0]/hp1[2];
    double dx = x1-x0;
    x_dif_sum += dx;
  }
  x_dif_sum /= img_pts0.size();
  bool x_shift_min = fabs(x_dif_sum)<0.1;
  if (!x_shift_min) {
    std::cout << "homographies do not minimize column shift" << std::endl;
    return false;
  }
  double sf = this->scale_to_input_size();
  vnl_matrix_fixed<double, 3, 3> tr,sc;
  tr.set_identity();
  tr[0][2] = -du_off_; tr[1][2] = -dv_off_;
  H0_ = tr*H0_;
  H1_ = tr*H1_;
  sc.set_identity();
  sc[0][0] = sf; sc[1][1] = sc[0][0];
  H0_ = sc*H0_;
  H1_ = sc*H1_;
  vnl_matrix_fixed<double, 3, 4> M0 = cam0_.get_matrix(), M1 = cam1_.get_matrix();
  M0 = H0_*M0;  M1 = H1_*M1;
  rect_cam0_.set_matrix(M0);
  rect_cam1_.set_matrix(M1);
  return true;
}

// provide the possibility of NAN as an invalid pixel value. Useful for subsequent
// processing to avoid the invalid warp regions in the rectified image.
template <class CAMT>
void bpgl_rectify_image_pair<CAMT>::
warp_image(vil_image_view<float> fview,
           vnl_matrix_fixed<double, 3, 3> const& H,
           vil_image_view<float>& fwarp,
           size_t out_ni, size_t out_nj)
{
  size_t ni = fview.ni(), nj = fview.nj();
  double dni = static_cast<double>(ni);
  double dnj = static_cast<double>(nj);
  fwarp.set_size(out_ni, out_nj);
  fwarp.fill(params_.invalid_pixel_val_);
  vnl_matrix_fixed<double, 3, 3> Hinv = vnl_inverse(H);
  vnl_vector_fixed<double, 3> opix(0.0, 0.0, 1), hopix;
  hopix = Hinv * opix;
  vnl_vector_fixed<double, 3> mpix(out_ni, out_nj, 1), hmpix;
  hmpix = Hinv * mpix;

  for (size_t j =0; j<out_nj; ++j) {
    for (size_t i =0; i<out_ni; ++i) {
      double du = static_cast<double>(i);
      double dv = static_cast<double>(j);
      vnl_vector_fixed<double, 3> pix(du, dv, 1), hpix;
      hpix = Hinv*pix;
      double du_h = hpix[0]/hpix[2], dv_h = hpix[1]/hpix[2];
      if (du_h < 0.0 || du_h >= dni || dv_h < 0.0 ||dv_h >= dnj)
        continue;
      //float fval = vil_bilin_interp_safe_extend(fview, du_h, dv_h);
      float fval = vil_bicub_interp_safe_extend(fview, du_h, dv_h);
      fwarp(i, j) = fval;
    }
  }
}

template <class CAMT>
void bpgl_rectify_image_pair<CAMT>::warp_pair()
{
  this->warp_image(fview0_, H0_, rect_fview0_, out_ni_, out_nj_);
  this->warp_image(fview1_, H1_, rect_fview1_, out_ni_, out_nj_);
}
  // Code for easy instantiation.
#undef BPGL_RECTIFY_IMAGE_PAIR_INSTANTIATE
#define BPGL_RECTIFY_IMAGE_PAIR_INSTANTIATE(CAMT) \
template class bpgl_rectify_image_pair<CAMT>
