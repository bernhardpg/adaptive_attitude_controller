#include "controller/attitude_controller.h"

namespace controller {
  AdaptiveController::AdaptiveController()
  {
    init();
  }

  void AdaptiveController::init()
  {
		// *******
    // Model parameters
		// *******
		Eigen::Matrix3d est_errors;
		est_errors << 0.02, 0,  0,
									0, 0.015, 0,
									0, 0,  0.03;

    J_nominal_ << 0.07, 0, 0,
									0, 0.08, 0,
									0, 0, 0.12; // From .urdf file
		// Augment inertia matrix to not be perfectly identified
		J_nominal_ += est_errors;

		// *******
		// Trajectory generator params
		// *******
    time_step_ = pow(10, -3); // Taken from simulation

    cmd_w_0_ = 30.0; // Bandwidth
    cmd_damping_ = 1.0; // Damping

		// *******
    // Controller params
		// *******
    k_q_ = 1.0;
    k_w_ = 1.0;

		// *******
		// Adaptive controller
		// *******
		// TODO start with better estimates?
		Lambda_hat_.diagonal() << 0,0,0;
		Theta_hat_.diagonal() << 0,0,0;
		Phi << 0,0,0;
		tau_dist_hat_ << 0,0,0;

		// *******
    // Initialize variables
		// *******
    q_ = Eigen::Quaterniond::Identity();
    w_ = Eigen::Vector3d::Zero();
    q_e_ = Eigen::Quaterniond::Identity();
    w_bc_ = Eigen::Vector3d::Zero();

    q_c_ = Eigen::Quaterniond::Identity();
    w_c_ = Eigen::Vector3d::Zero();
    w_c_dot_ = Eigen::Vector3d::Zero();
    w_c_body_frame = Eigen::Vector3d::Zero();
    w_c_dot_body_frame = Eigen::Vector3d::Zero();

    q_r_ = EulerToQuat(0,0,0);
  }

  void AdaptiveController::controllerCallback(Eigen::Quaterniond q, Eigen::Vector3d w, double t)
  {
		t_ = t;
    q_ = Eigen::Quaterniond(q);
    w_ = Eigen::Vector3d(w);

    // Controll loop
    generateCommandSignal();
    calculateErrors();
    computeInput();
  }

	void AdaptiveController::setRefSignal(Eigen::Quaterniond q_ref)
	{
		q_r_ = q_ref;
	}

	Eigen::Vector3d AdaptiveController::getInputTorques()
	{
		return input_.tau;
	}

	Eigen::Quaterniond AdaptiveController::getCmdSignal()
	{
		return q_c_;
	}

  void AdaptiveController::generateCommandSignal()
  {

    // Difference between reference and command frame
    Eigen::Quaterniond q_rc = q_r_.conjugate() * q_c_;

    // Create quaternion from vector to make math work
    Eigen::Quaterniond w_c_quat;
    w_c_quat.w() = 0;
    w_c_quat.vec() = 0.5 * w_c_;

    // Define derivatives
    Eigen::Quaterniond q_c_dot = q_c_ * w_c_quat;
    w_c_dot_ = - 2 * pow(cmd_w_0_, 2) * quat_log_v(quat_plus_map(q_rc))
      - 2 * cmd_damping_ * cmd_w_0_ * w_c_;
    // Note: w_c_dot_ = u_c in the paper

    // Integrate using forward Euler:
    q_c_.w() = q_c_.w() + time_step_ * q_c_dot.w();
    q_c_.vec() = q_c_.vec() + time_step_ * q_c_dot.vec();
    w_c_ = w_c_ + time_step_ * w_c_dot_;

    // Calculate body frame values
    w_c_body_frame = q_e_.conjugate()._transformVector(w_c_);
    w_c_dot_body_frame = q_e_.conjugate()._transformVector(w_c_dot_);
  }

  void AdaptiveController::calculateErrors()
  {
    // Calulate attitude error
    q_e_ = q_c_.conjugate() * q_;
    // Calculate angular velocity error
    w_bc_ = w_ - q_e_.conjugate()._transformVector(w_c_);
    //std::cout << w_bc_ << std::endl << std::endl;
  }

  void AdaptiveController::computeInput()
  {
    // Calculates inputs for the NED frame!
    // Baseline controller
    Eigen::Vector3d cancellation_terms = cross_map(w_) * J_nominal_ * w_
      + J_nominal_ * (w_c_dot_body_frame
          + cross_map(w_) * w_c_body_frame);

    Eigen::Vector3d feedforward_terms = J_nominal_ * (- k_q_ * quat_log_v(quat_plus_map(q_e_)) - k_w_ * w_bc_);

    input_.tau = cancellation_terms + feedforward_terms;
  }

  double AdaptiveController::saturate(double v, double min, double max)
  {
    v = v > max ? max : (
        v < min ? min : v
        );

    return v;
  }
} // namespace controller
