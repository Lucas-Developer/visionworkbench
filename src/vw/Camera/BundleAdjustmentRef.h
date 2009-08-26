// __BEGIN_LICENSE__
// Copyright (C) 2006-2009 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

/// \file BundleAdjustmentRef.h
/// 
/// Reference implementation of bundle adjustment. Very slow!

#ifndef __VW_CAMERA_BUNDLE_ADJUSTMENT_REF_H__
#define __VW_CAMERA_BUNDLE_ADJUSTMENT_REF_H__

#include <vw/Camera/BundleAdjustmentBase.h>

namespace vw {
namespace camera {
  
  template <class BundleAdjustModelT, class RobustCostT>
  class BundleAdjustmentRef : public BundleAdjustmentBase<BundleAdjustModelT,RobustCostT> {

  public:

    BundleAdjustmentRef( BundleAdjustModelT & model,
                         RobustCostT const& robust_cost_func,
                         bool use_camera_constraint=true,
                         bool use_gcp_constraint=true ) :
    BundleAdjustmentBase<BundleAdjustModelT,RobustCostT>( model, robust_cost_func,
                                                          use_camera_constraint,
                                                          use_gcp_constraint ) {}
    
    // PROBABLY DELETE?
    //-----------------------------------------------------------
    // I'm not sure what this does
    void debug_impl(Matrix<double> &J, Matrix<double> &sigma, Vector<double> &epsilon) {

      // Here are some useful variable declarations that make the code
      // below more readable.
      unsigned num_cam_params = BundleAdjustModelT::camera_params_n;
      unsigned num_pt_params = BundleAdjustModelT::point_params_n;
      unsigned num_points = this->m_model.num_points();
      unsigned num_model_parameters = this->m_model.num_cameras()*num_cam_params + this->m_model.num_points()*num_pt_params;

      unsigned num_cameras = this->m_model.num_cameras();
      unsigned num_ground_control_points = this->m_control_net->num_ground_control_points();
      unsigned num_observations = 2*this->m_model.num_pixel_observations();
      if (this->m_use_camera_constraint)
        num_observations += num_cameras*num_cam_params;
      if (this->m_use_gcp_constraint)
        num_observations += num_ground_control_points*num_pt_params;

      // --- SETUP STEP ----
      // Add rows to J and epsilon for the imaged pixel observations
      int idx = 0;
      for (unsigned i = 0; i < this->m_control_net->size(); 
           ++i) {       // Iterate over control points
        for (unsigned m = 0; m < (*(this->m_control_net))[i].size(); 
             ++m) {  // Iterate over control measures
          int camera_idx = (*(this->m_control_net))[i][m].image_id();

          Matrix<double> J_a = this->m_model.A_jacobian(i,camera_idx, 
                                                        this->m_model.A_parameters(camera_idx), 
                                                        this->m_model.B_parameters(i));
          Matrix<double> J_b = this->m_model.B_jacobian(i,camera_idx, 
                                                        this->m_model.A_parameters(camera_idx), 
                                                        this->m_model.B_parameters(i));

          // Populate the Jacobian Matrix
          submatrix(J, 2*idx, num_cam_params*camera_idx, 2, num_cam_params) = J_a;
          submatrix(J, 2*idx, num_cam_params*num_cameras + i*num_pt_params, 2, num_pt_params) = J_b;

          // Apply robust cost function weighting and populate the error vector
          Vector2 unweighted_error = (*(this->m_control_net))[i][m].dominant() - this->m_model(i, camera_idx, 
                                      this->m_model.A_parameters(camera_idx), 
                                      this->m_model.B_parameters(i));
          double mag = norm_2(unweighted_error);
          double weight = sqrt(this->m_robust_cost_func(mag)) / mag;
          subvector(epsilon,2*idx,2) = unweighted_error * weight;

          // Fill in the entries of the sigma matrix with the uncertainty of the observations.
          Matrix2x2 inverse_cov;
          Vector2 pixel_sigma = (*this->m_control_net)[i][m].sigma();
          inverse_cov(0,0) = 1/(pixel_sigma(0)*pixel_sigma(0));
          inverse_cov(1,1) = 1/(pixel_sigma(1)*pixel_sigma(1));
          submatrix(sigma, 2*idx, 2*idx, 2, 2) = inverse_cov;

          ++idx;
        }
      }
      
      // Add rows to J and epsilon for a priori camera parameters...
      if (this->m_use_camera_constraint)
        for (unsigned j=0; j < num_cameras; ++j) {
          Matrix<double> id(num_cam_params, num_cam_params);
          id.set_identity();
          submatrix(J,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    j*num_cam_params,
                    num_cam_params,
                    num_cam_params) = id;
          Vector<double> unweighted_error = this->m_model.A_initial(j)-this->m_model.A_parameters(j);
          subvector(epsilon,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    num_cam_params) = unweighted_error;
          submatrix(sigma,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    num_cam_params, num_cam_params) = this->m_model.A_inverse_covariance(j);
        }

      // ... and the position of the 3D points to J and epsilon ...
      if (this->m_use_gcp_constraint) {
        idx = 0;
        for (unsigned i=0; i < this->m_model.num_points(); ++i) {
          if ((*(this->m_control_net))[i].type() == ControlPoint::GroundControlPoint) {
            Matrix<double> id(num_pt_params,num_pt_params);
            id.set_identity();
            submatrix(J,2*this->m_model.num_pixel_observations() + num_cameras*num_cam_params + 
                      idx*num_pt_params,
                      num_cameras*num_cam_params + idx*num_pt_params,
                      num_pt_params,
                      num_pt_params) = id;
            Vector<double> unweighted_error = this->m_model.B_initial(i)-this->m_model.B_parameters(i);
            subvector(epsilon,
                      2*this->m_model.num_pixel_observations() + num_cameras*num_cam_params + 
                      idx*num_pt_params,
                      num_pt_params) = unweighted_error;
            submatrix(sigma,
                      2*this->m_model.num_pixel_observations() + num_cameras*num_cam_params + 
                      idx*num_pt_params,
                      2*this->m_model.num_pixel_observations() + num_cameras*num_cam_params + 
                      idx*num_pt_params,
                      num_pt_params, num_pt_params) = this->m_model.B_inverse_covariance(i);
            ++idx;
          }
        }
      }
    }

    // UPDATE IMPLEMENTATION
    //---------------------------------------------------------------
    // This is a simple, non-sparse, unoptimized implementation of LM
    // bundle adjustment.  It is primarily used for validation and
    // debugging.
    //
    // Each entry in the outer vector corresponds to a distinct 3D
    // point.  The inner vector contains a list of image IDs and
    // pixel coordinates where that point was imaged.
    virtual double update(double &abs_tol, double &rel_tol) { 
      ++this->m_iterations;

      // Here are some useful variable declarations that make the code
      // below more readable.
      unsigned num_cam_params = BundleAdjustModelT::camera_params_n;
      unsigned num_pt_params = BundleAdjustModelT::point_params_n;
      unsigned num_points = this->m_model.num_points();
      unsigned num_model_parameters = this->m_model.num_cameras()*num_cam_params + 
        this->m_model.num_points()*num_pt_params;

      unsigned num_cameras = this->m_model.num_cameras();
      unsigned num_ground_control_points = this->m_control_net->num_ground_control_points();
      unsigned num_observations = 2*this->m_model.num_pixel_observations();
      if (this->m_use_camera_constraint)
        num_observations += num_cameras*num_cam_params;
      if (this->m_use_gcp_constraint)
        num_observations += num_ground_control_points*num_pt_params;

      // The core LM matrices and vectors
      Matrix<double> J(num_observations, num_model_parameters);   // Jacobian Matrix
      Vector<double> epsilon(num_observations);                   // Error vector
      Matrix<double> sigma(num_observations, num_observations);   // Sigma (uncertainty) matrix

      // --- SETUP STEP ----
      // Add rows to J and epsilon for the imaged pixel observations
      int idx = 0;
      for (unsigned i = 0; i < this->m_control_net->size(); ++i) {       // Iterate over control points
        for (unsigned m = 0; m < (*(this->m_control_net))[i].size(); 
             ++m) {  // Iterate over control measures
          int camera_idx = (*(this->m_control_net))[i][m].image_id();

          Matrix<double> J_a = this->m_model.A_jacobian(i,camera_idx, 
                                                        this->m_model.A_parameters(camera_idx), 
                                                        this->m_model.B_parameters(i));
          Matrix<double> J_b = this->m_model.B_jacobian(i,camera_idx, 
                                                        this->m_model.A_parameters(camera_idx), 
                                                        this->m_model.B_parameters(i));

          // Populate the Jacobian Matrix
          submatrix(J, 2*idx, num_cam_params*camera_idx, 2, num_cam_params) = J_a;
          submatrix(J, 2*idx, num_cam_params*num_cameras + i*num_pt_params, 2, num_pt_params) = J_b;

          // Apply robust cost function weighting and populate the error vector
          Vector2 unweighted_error = (*(this->m_control_net))[i][m].dominant() - 
            this->m_model(i, camera_idx, 
                          this->m_model.A_parameters(camera_idx), 
                          this->m_model.B_parameters(i));
          double mag = norm_2(unweighted_error);
          double weight = sqrt(this->m_robust_cost_func(mag)) / mag;
          subvector(epsilon,2*idx,2) = unweighted_error * weight;

          // Fill in the entries of the sigma matrix with the uncertainty of the observations.
          Matrix2x2 inverse_cov;
          Vector2 pixel_sigma = (*(this->m_control_net))[i][m].sigma();
          inverse_cov(0,0) = 1/(pixel_sigma(0)*pixel_sigma(0));
          inverse_cov(1,1) = 1/(pixel_sigma(1)*pixel_sigma(1));
          submatrix(sigma, 2*idx, 2*idx, 2, 2) = inverse_cov;

          ++idx;
        }
      }
      
      // Add rows to J and epsilon for a priori camera parameters...
      if (this->m_use_camera_constraint)
        for (unsigned j=0; j < num_cameras; ++j) {
          Matrix<double> id(num_cam_params, num_cam_params);
          id.set_identity();
          submatrix(J,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    j*num_cam_params,
                    num_cam_params,
                    num_cam_params) = id;
          Vector<double> unweighted_error = this->m_model.A_initial(j) - 
            this->m_model.A_parameters(j);
          subvector(epsilon,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    num_cam_params) = unweighted_error;
          submatrix(sigma,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    num_cam_params, num_cam_params) = this->m_model.A_inverse_covariance(j);
        }

      // ... and the position of the 3D points to J and epsilon ...
      if (this->m_use_gcp_constraint) {
        idx = 0;
        for (unsigned i=0; i < this->m_model.num_points(); ++i) {
          if ((*this->m_control_net)[i].type() == ControlPoint::GroundControlPoint) {
            Matrix<double> id(num_pt_params,num_pt_params);
            id.set_identity();
            submatrix(J,2*this->m_model.num_pixel_observations() + 
                      num_cameras*num_cam_params + idx*num_pt_params,
                      num_cameras*num_cam_params + idx*num_pt_params,
                      num_pt_params,
                      num_pt_params) = id;
            Vector<double> unweighted_error = this->m_model.B_initial(i)-this->m_model.B_parameters(i);
            subvector(epsilon,
                      2*this->m_model.num_pixel_observations() + 
                      num_cameras*num_cam_params + idx*num_pt_params,
                      num_pt_params) = unweighted_error;
            submatrix(sigma,
                      2*this->m_model.num_pixel_observations() + 
                      num_cameras*num_cam_params + idx*num_pt_params,
                      2*this->m_model.num_pixel_observations() + 
                      num_cameras*num_cam_params + idx*num_pt_params,
                      num_pt_params, num_pt_params) = this->m_model.B_inverse_covariance(i);
            ++idx;
          }
        }
      }

      // --- UPDATE STEP ----

      // Build up the right side of the normal equation...
      Vector<double> del_J = -1.0 * (transpose(J) * sigma * epsilon);

      // ... and the left side.  (Remembering to rescale the diagonal
      // entries of the approximated hessian by lambda)
      Matrix<double> hessian = transpose(J) * sigma * J;

      // initialize m_lambda on first iteration, ignore if user has
      // changed it.
      double max = 0.0;
      if (this->m_iterations == 1 && this->m_lambda == 1e-3){
        for (unsigned i = 0; i < hessian.rows(); ++i){
          if (fabs(hessian(i,i)) > max)
            max = fabs(hessian(i,i));
        }
        this->m_lambda = 1e-10 * max;
      }

      for ( unsigned i=0; i < hessian.rows(); ++i )
        hessian(i,i) +=  this->m_lambda;

      //Cholesky decomposition. Returns Cholesky matrix in lower left hand corner.
      Vector<double> delta = del_J;

      // Here we want to make sure that if we apply Schur methods as
      // on p. 604, we can get the same answer as in the general delta.
      unsigned num_cam_entries = num_cam_params * num_cameras;
      unsigned num_pt_entries = num_pt_params * num_points;

      Matrix<double> U = submatrix(hessian, 0, 0, num_cam_entries, num_cam_entries);
      Matrix<double> W = submatrix(hessian, 0, num_cam_entries,  num_cam_entries, num_pt_entries);
      Matrix<double> Vinv = submatrix(hessian, num_cam_entries, num_cam_entries, 
                                      num_pt_entries, num_pt_entries); 
      chol_inverse(Vinv);
      Matrix<double> Y = W * transpose(Vinv) * Vinv;
      Vector<double> e = subvector(delta, 0, num_cam_entries) - 
        W * transpose(Vinv) * Vinv * subvector(delta, num_cam_entries, num_pt_entries); 
      Matrix<double> S = U - Y * transpose(W);
      solve(e, S); // using cholesky
      solve(delta, hessian);
      
      double nsq_x = 0;
      for (unsigned j=0; j<this->m_model.num_cameras(); ++j){
        Vector<double> vec = this->m_model.A_parameters(j);
        nsq_x += norm_2(vec);
      }
      for (unsigned i=0; i<this->m_model.num_points(); ++i){
        Vector<double> vec =  this->m_model.B_parameters(i);
        nsq_x += norm_2(vec);
      }


      // Solve for update

      // --- EVALUATE UPDATE STEP ---
      Vector<double> new_epsilon(num_observations);                  // Error vector

      idx = 0;
      for (unsigned i = 0; i < this->m_control_net->size(); ++i) {       
        for (unsigned m = 0; m < (*this->m_control_net)[i].size(); ++m) {
          int camera_idx = (*this->m_control_net)[i][m].image_id();

          Vector<double> cam_delta = subvector(delta, num_cam_params*camera_idx, num_cam_params);
          Vector<double> pt_delta = subvector(delta, num_cam_params*num_cameras + num_pt_params*i, 
                                              num_pt_params);

          // Apply robust cost function weighting and populate the error vector
          Vector2 unweighted_error = (*this->m_control_net)[i][m].dominant() - 
            this->m_model(i, camera_idx,
                          this->m_model.A_parameters(camera_idx)-cam_delta, 
                          this->m_model.B_parameters(i)-pt_delta);
          double mag = norm_2(unweighted_error);
          double weight = sqrt(this->m_robust_cost_func(mag)) / mag;
          subvector(new_epsilon,2*idx,2) = unweighted_error * weight;

          ++idx;
        }
      }

      // Add rows to J and epsilon for a priori position/pose constraints...
      if (this->m_use_camera_constraint)
        for (unsigned j=0; j < num_cameras; ++j) {
          Vector<double> cam_delta = subvector(delta, num_cam_params*j, num_cam_params);
          subvector(new_epsilon,
                    2*this->m_model.num_pixel_observations() + j*num_cam_params,
                    num_cam_params) = this->m_model.A_initial(j)-
            (this->m_model.A_parameters(j)-cam_delta);
        }

      // ... and the position of the 3D points to J and epsilon ...
      if (this->m_use_gcp_constraint) {
        idx = 0;
        for (unsigned i=0; i < this->m_model.num_points(); ++i) {
          if ((*this->m_control_net)[i].type() == ControlPoint::GroundControlPoint) {
            Vector<double> pt_delta = subvector(delta, num_cam_params*num_cameras + num_pt_params*i, 
                                                num_pt_params);
            subvector(new_epsilon,
                      2*this->m_model.num_pixel_observations() + num_cameras*num_cam_params + 
                      idx*num_pt_params,
                      num_pt_params) = this->m_model.B_initial(i)-
              (this->m_model.B_parameters(i)-pt_delta);
            ++idx;
          }
        }
      }

      //Fletcher modification
      double Splus = .5*transpose(new_epsilon) * sigma * new_epsilon; //Compute new objective
      double SS = .5*transpose(epsilon) * sigma * epsilon;            //Compute old objective
      double dS = .5 * transpose(delta) *(this->m_lambda * delta + del_J);

      // WARNING: will want to replace dS later

      double R = (SS - Splus)/dS; // Compute ratio
      unsigned ret = 0;
      
      if (R > 0){
        ret = 1; 
        for (unsigned j=0; j<this->m_model.num_cameras(); ++j) 
          this->m_model.set_A_parameters(j, this->m_model.A_parameters(j) - 
                                         subvector(delta, num_cam_params*j, num_cam_params));
        for (unsigned i=0; i<this->m_model.num_points(); ++i)
          this->m_model.set_B_parameters(i, this->m_model.B_parameters(i) - 
                                         subvector(delta, num_cam_params*num_cameras + num_pt_params*i, 
                                                   num_pt_params));
     
        // Summarize the stats from this step in the iteration
        double overall_norm = sqrt(.5 * transpose(new_epsilon) * sigma * new_epsilon);
        
        double overall_delta = sqrt(.5 * transpose(epsilon) * sigma * epsilon) - 
          sqrt(.5 * transpose(new_epsilon) * sigma * new_epsilon) ; 
        
        abs_tol = overall_norm;
        rel_tol = overall_delta;

        if (this->m_control==0){
          double temp = 1 - pow((2*R - 1),3); 
          if (temp < 1.0/3.0)
            temp = 1.0/3.0;
      
          this->m_lambda *= temp;
          this->m_nu = 2;
        } else if (this->m_control == 1)
          this->m_lambda /= 10;
        
        return overall_delta;
      
      } else { // R <= 0
        
        if (this->m_control == 0){
          this->m_lambda *= this->m_nu; 
          this->m_nu*=2; 
        } else if (this->m_control == 1)
          this->m_lambda *= 10;

        // double overall_delta = sqrt(.5 * transpose(epsilon) * sigma * epsilon) - sqrt(.5 * transpose(new_epsilon) * sigma * new_epsilon);
 
        return ScalarTypeLimits<double>::highest();
      }
    }

  };

}}

#endif//__VW_CAMERA_BUNDLE_ADJUSTMENT_REF_H__