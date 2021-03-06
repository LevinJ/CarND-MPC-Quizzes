#include "MPC.h"
#include <math.h>
#include <cppad/cppad.hpp>
#include <cppad/ipopt/solve.hpp>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "matplotlibcpp.h"

namespace plt = matplotlibcpp;

using CppAD::AD;

// We set the number of timesteps to 25
// and the timestep evaluation frequency or evaluation
// period to 0.05.
size_t N = 25;
double dt = 0.05;

// This value assumes the model presented in the classroom is used.
//
// It was obtained by measuring the radius formed by running the vehicle in the
// simulator around in a circle with a constant steering angle and velocity on a
// flat terrain.
//
// Lf was tuned until the the radius formed by the simulating the model
// presented in the classroom matched the previous radius.
//
// This is the length from front to CoG that has a similar radius.
const double Lf = 2.67;

// Both the reference cross track and orientation errors are 0.
// The reference velocity is set to 40 mph.
double ref_cte = 0;
double ref_epsi = 0;
double ref_v = 40;

// The solver takes all the state variables and actuator
// variables in a singular vector. Thus, we should to establish
// when one variable starts and another ends to make our lifes easier.
size_t x_start = 0;
size_t y_start = x_start + N;
size_t psi_start = y_start + N;
size_t v_start = psi_start + N;
size_t cte_start = v_start + N;
size_t epsi_start = cte_start + N;
size_t delta_start = epsi_start + N;
size_t a_start = delta_start + N - 1;

class FG_eval {
public:
	Eigen::VectorXd coeffs;
	// Coefficients of the fitted polynomial.
	FG_eval(Eigen::VectorXd coeffs) { this->coeffs = coeffs; }

	typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
	// `fg` is a vector containing the cost and constraints.
	// `vars` is a vector containing the variable values (state & actuators).
	void operator()(ADvector& fg, const ADvector& vars) {
		// The cost is stored is the first element of `fg`.
		// Any additions to the cost should be added to `fg[0]`.
		fg[0] = 0;

		// The part of the cost based on the reference state.
		for (int i = 0; i < N; i++) {
			fg[0] += CppAD::pow(vars[cte_start + i] - ref_cte, 2);
			fg[0] += CppAD::pow(vars[epsi_start + i] - ref_epsi, 2);
			fg[0] += CppAD::pow(vars[v_start + i] - ref_v, 2);
		}

		// Minimize the use of actuators.
		for (int i = 0; i < N - 1; i++) {
			fg[0] += CppAD::pow(vars[delta_start + i], 2);
			fg[0] += CppAD::pow(vars[a_start + i], 2);
		}

		// Minimize the value gap between sequential actuations.
		for (int i = 0; i < N - 2; i++) {
			fg[0] +=  CppAD::pow(vars[delta_start + i + 1] - vars[delta_start + i], 2);
			fg[0] += CppAD::pow(vars[a_start + i + 1] - vars[a_start + i], 2);
		}

		//
		// Setup Constraints
		//
		// NOTE: In this section you'll setup the model constraints.

		// Initial constraints
		//
		// We add 1 to each of the starting indices due to cost being located at
		// index 0 of `fg`.
		// This bumps up the position of all the other values.
		fg[1 + x_start] = vars[x_start];
		fg[1 + y_start] = vars[y_start];
		fg[1 + psi_start] = vars[psi_start];
		fg[1 + v_start] = vars[v_start];
		fg[1 + cte_start] = vars[cte_start];
		fg[1 + epsi_start] = vars[epsi_start];

		// The rest of the constraints
		for (int i = 0; i < N - 1; i++) {
			// The state at time t+1 .
			AD<double> x1 = vars[x_start + i + 1];
			AD<double> y1 = vars[y_start + i + 1];
			AD<double> psi1 = vars[psi_start + i + 1];
			AD<double> v1 = vars[v_start + i + 1];
			AD<double> cte1 = vars[cte_start + i + 1];
			AD<double> epsi1 = vars[epsi_start + i + 1];

			// The state at time t.
			AD<double> x0 = vars[x_start + i];
			AD<double> y0 = vars[y_start + i];
			AD<double> psi0 = vars[psi_start + i];
			AD<double> v0 = vars[v_start + i];
			AD<double> cte0 = vars[cte_start + i];
			AD<double> epsi0 = vars[epsi_start + i];

			// Only consider the actuation at time t.
			AD<double> delta0 = vars[delta_start + i];
			AD<double> a0 = vars[a_start + i];

			AD<double> f0 = coeffs[0] + coeffs[1] * x0 + coeffs[2] *CppAD::pow(x0,2) + coeffs[3] *CppAD::pow(x0,3);
			AD<double> psides0 = CppAD::atan(coeffs[1] + 2 * coeffs[2] * x0 + 3 * coeffs[3]*CppAD::pow(x0,2));

			// Here's `x` to get you started.
			// The idea here is to constraint this value to be 0.
			//
			// Recall the equations for the model:
			// x_[t+1] = x[t] + v[t] * cos(psi[t]) * dt
			// y_[t+1] = y[t] + v[t] * sin(psi[t]) * dt
			// psi_[t+1] = psi[t] + v[t] / Lf * delta[t] * dt
			// v_[t+1] = v[t] + a[t] * dt
			// cte[t+1] = f(x[t]) - y[t] + v[t] * sin(epsi[t]) * dt
			// epsi[t+1] = psi[t] - psides[t] + v[t] * delta[t] / Lf * dt
			fg[2 + x_start + i] = x1 - (x0 + v0 * CppAD::cos(psi0) * dt);
			fg[2 + y_start + i] = y1 - (y0 + v0 * CppAD::sin(psi0) * dt);
			fg[2 + psi_start + i] = psi1 - (psi0 + v0 * delta0 / Lf * dt);
			fg[2 + v_start + i] = v1 - (v0 + a0 * dt);
			fg[2 + cte_start + i] =
					cte1 - ((f0 - y0) + (v0 * CppAD::sin(epsi0) * dt));
			fg[2 + epsi_start + i] =
					epsi1 - ((psi0 - psides0) + v0 * delta0 / Lf * dt);
		}
	}
};

//
// MPC class definition
//

MPC::MPC() {}
MPC::~MPC() {}

vector<double> MPC::Solve(Eigen::VectorXd x0, Eigen::VectorXd coeffs,vector<double>& mpc_x, vector<double>& mpc_y) {
	size_t i;
	typedef CPPAD_TESTVECTOR(double) Dvector;

	double x = x0[0];
	double y = x0[1];
	double psi = x0[2];
	double v = x0[3];
	double cte = x0[4];
	double epsi = x0[5];

	// number of independent variables
	// N timesteps == N - 1 actuations
	size_t n_vars = N * 6 + (N - 1) * 2;
	// Number of constraints
	size_t n_constraints = N * 6;

	// Initial value of the independent variables.
	// Should be 0 except for the initial values.
	Dvector vars(n_vars);
	for (int i = 0; i < n_vars; i++) {
		vars[i] = 0.0;
	}
	// Set the initial variable values
	vars[x_start] = x;
	vars[y_start] = y;
	vars[psi_start] = psi;
	vars[v_start] = v;
	vars[cte_start] = cte;
	vars[epsi_start] = epsi;

	// Lower and upper limits for x
	Dvector vars_lowerbound(n_vars);
	Dvector vars_upperbound(n_vars);

	// Set all non-actuators upper and lowerlimits
	// to the max negative and positive values.
	for (int i = 0; i < delta_start; i++) {
		vars_lowerbound[i] = -1.0e19;
		vars_upperbound[i] = 1.0e19;
	}

	// The upper and lower limits of delta are set to -25 and 25
	// degrees (values in radians).
	// NOTE: Feel free to change this to something else.
	for (int i = delta_start; i < a_start; i++) {
		vars_lowerbound[i] = -0.436332;
		vars_upperbound[i] = 0.436332;
	}

	// Acceleration/decceleration upper and lower limits.
	// NOTE: Feel free to change this to something else.
	for (int i = a_start; i < n_vars; i++) {
		vars_lowerbound[i] = -1.0;
		vars_upperbound[i] = 1.0;
	}

	// Lower and upper limits for constraints
	// All of these should be 0 except the initial
	// state indices.
	Dvector constraints_lowerbound(n_constraints);
	Dvector constraints_upperbound(n_constraints);
	for (int i = 0; i < n_constraints; i++) {
		constraints_lowerbound[i] = 0;
		constraints_upperbound[i] = 0;
	}
	constraints_lowerbound[x_start] = x;
	constraints_lowerbound[y_start] = y;
	constraints_lowerbound[psi_start] = psi;
	constraints_lowerbound[v_start] = v;
	constraints_lowerbound[cte_start] = cte;
	constraints_lowerbound[epsi_start] = epsi;

	constraints_upperbound[x_start] = x;
	constraints_upperbound[y_start] = y;
	constraints_upperbound[psi_start] = psi;
	constraints_upperbound[v_start] = v;
	constraints_upperbound[cte_start] = cte;
	constraints_upperbound[epsi_start] = epsi;

	// Object that computes objective and constraints
	FG_eval fg_eval(coeffs);

	// options
	std::string options;
	options += "Integer print_level  0\n";
	options += "Sparse  true        forward\n";
	options += "Sparse  true        reverse\n";
	options += "Numeric max_cpu_time          0.5\n";

	// place to return solution
	CppAD::ipopt::solve_result<Dvector> solution;

	// solve the problem
	CppAD::ipopt::solve<Dvector, FG_eval>(
			options, vars, vars_lowerbound, vars_upperbound, constraints_lowerbound,
			constraints_upperbound, fg_eval, solution);

	//
	// Check some of the solution values
	//
	bool ok = true;
	ok &= solution.status == CppAD::ipopt::solve_result<Dvector>::success;

	auto cost = solution.obj_value;
	std::cout << "Cost " << cost << std::endl;
	for (int i = 0; i < N; i++){
		mpc_x.push_back(solution.x[x_start + i]);
		mpc_y.push_back(solution.x[y_start + i]);
	}
	if(!ok){
		std::cout << "Fatal error: failed to find optimal parameters!!!" << std::endl;
		exit(EXIT_FAILURE);
	}
	return {solution.x[x_start + 1],   solution.x[y_start + 1],
		solution.x[psi_start + 1], solution.x[v_start + 1],
		solution.x[cte_start + 1], solution.x[epsi_start + 1],
		solution.x[delta_start],   solution.x[a_start],
		cost};
}

//
// Helper functions to fit and evaluate polynomials.
//

// Evaluate a polynomial.
double polyeval(Eigen::VectorXd coeffs, double x) {
	double result = 0.0;
	for (int i = 0; i < coeffs.size(); i++) {
		result += coeffs[i] * pow(x, i);
	}
	return result;
}

// Fit a polynomial.
// Adapted from
// https://github.com/JuliaMath/Polynomials.jl/blob/master/src/Polynomials.jl#L676-L716
Eigen::VectorXd polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals,
		int order) {
	assert(xvals.size() == yvals.size());
	assert(order >= 1 && order <= xvals.size() - 1);
	Eigen::MatrixXd A(xvals.size(), order + 1);

	for (int i = 0; i < xvals.size(); i++) {
		A(i, 0) = 1.0;
	}

	for (int j = 0; j < xvals.size(); j++) {
		for (int i = 0; i < order; i++) {
			A(j, i + 1) = A(j, i) * xvals(j);
		}
	}

	auto Q = A.householderQr();
	auto result = Q.solve(yvals);
	return result;
}

void transform_map_coord(vector<double>& xvals,vector<double>& yvals, double vehicle_x, double vehicle_y, double vehicle_theta) {

	vector<double> transformed_x;
	vector<double> transformed_y;
	int total_size = xvals.size();

	for (int i = 0; i < total_size; i++) {

		double new_x;
		double new_y;

		double cos_theta = cos(vehicle_theta - M_PI / 2);
		double sin_theta = sin(vehicle_theta - M_PI / 2);
		new_x = -(xvals[i] - vehicle_x) * sin_theta + (yvals[i] - vehicle_y) * cos_theta;
		new_y = -(xvals[i] - vehicle_x) * cos_theta - (yvals[i] - vehicle_y) * sin_theta;

		transformed_x.push_back(new_x);
		transformed_y.push_back(new_y);
	}
	xvals= transformed_x;
	yvals = transformed_y;

	return;
}


int main() {
	MPC mpc;
	int iters = 60;

	vector<double> next_x={-32.16173,-43.49173,-61.09,-78.29172,-93.05002,-107.7717};
	vector<double> next_y={113.361,105.941,92.88499,78.73102,65.34102,50.57938};



	// NOTE: free feel to play around with these
	double x = -40.62;
	double y = 108.73;
	double psi = 3.733651;
	double v = 10;

	//transform to vehicle coordinate
	transform_map_coord(next_x,next_y, x, y, psi);
	x = 0;
	y = 0;
	psi = 0;


	Eigen::VectorXd ptsx;
	Eigen::VectorXd ptsy;
	int next_xy_size = next_x.size();
	ptsx.resize(next_xy_size);
	ptsy.resize(next_xy_size);
	for (int i=0; i< next_xy_size; i++){
		ptsx[i] = next_x[i];
		ptsy[i] = next_y[i];
	}

	// The polynomial is fitted to a straight line so a polynomial with
	// order 1 is sufficient.
	auto coeffs = polyfit(ptsx, ptsy, 3);

	//
	// The cross track error is calculated by evaluating at polynomial at x, f(x)
	// and subtracting y.
	double cte = polyeval(coeffs, 0) - y;
	// Due to the sign starting at 0, the orientation error is -f'(x).
	// derivative of coeffs[0] + coeffs[1] * x -> coeffs[1]
	double epsi = -atan(coeffs[1] + (2 * coeffs[2] * x) + (3 * coeffs[3]* (x*x)));

	Eigen::VectorXd state(6);
	state << x, y, psi, v, cte, epsi;

	std::vector<double> x_vals = {state[0]};
	std::vector<double> y_vals = {state[1]};
	std::vector<double> psi_vals = {state[2]};
	std::vector<double> v_vals = {state[3]};
	std::vector<double> cte_vals = {state[4]};
	std::vector<double> epsi_vals = {state[5]};
	std::vector<double> delta_vals = {};
	std::vector<double> a_vals = {};
	std::vector<double> cost_vals = {};

	cout <<"initial status: " << state <<endl;
	for (size_t i = 0; i < iters; i++) {
		std::cout << "Iteration " << i << std::endl;
		vector<double> mpc_x;
		vector<double> mpc_y;

		auto vars = mpc.Solve(state, coeffs,mpc_x,mpc_y);


		x_vals.push_back(vars[0]);
		y_vals.push_back(vars[1]);
		psi_vals.push_back(vars[2]);
		v_vals.push_back(vars[3]);
		cte_vals.push_back(vars[4]);
		epsi_vals.push_back(vars[5]);

		delta_vals.push_back(vars[6]);
		a_vals.push_back(vars[7]);
		cost_vals.push_back(vars[8]);

		state << vars[0], vars[1], vars[2], vars[3], vars[4], vars[5];
		std::cout << state << std::endl;

		if(i == (iters-1) || (i%10 == 0)){
			plt::plot(next_x, next_y);
			plt::plot(mpc_x, mpc_y, "ro");
			plt::show();

		}

		std::cout << "Iteration " << i << " end"<<std::endl;



	}

	// Plot values
	// NOTE: feel free to play around with this.
	// It's useful for debugging!
	plt::subplot(5, 1, 1);
	plt::title("CTE");
	plt::plot(cte_vals);

	plt::subplot(5, 1, 2);
	plt::title("epsi");
	plt::plot(epsi_vals);

	plt::subplot(5, 1, 3);
	plt::title("cost");
	plt::plot(cost_vals);

	plt::subplot(5, 1, 4);
	plt::title("Delta (Radians)");
	plt::plot(delta_vals);

	plt::subplot(5, 1, 5);
	plt::title("Velocity");
	plt::plot(v_vals);

	plt::show();
}
