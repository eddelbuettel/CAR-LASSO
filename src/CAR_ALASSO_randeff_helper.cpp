// [[Rcpp::depends(RcppArmadillo)]]
#define ARMA_DONT_PRINT_ERRORS
#include <RcppArmadillo.h> 
#include <tgmath.h>
using namespace Rcpp;
using namespace arma;
using namespace std;

// [[Rcpp::depends(RcppProgress)]]
#include <progress.hpp>
#include <progress_bar.hpp>
#include "helper.h"
#include "GIG_helper.h"
#include "CAR_LASSO_helper.h"
#include "CAR_LASSO_randeff_helper.h"


void update_car_randeff_Omega_adp_helper(arma::mat & Omega,
                             const arma::mat & data,
                             const arma::mat & design,
                             const arma::mat & design_r,
                             const arma::mat & nu, 
                             const arma::vec & mu,
                             const arma::mat & beta,
                             const arma::vec & lambda_curr,// different lambda for different entries
                             int k, int p,int n){
  //arma::mat Omega;
  //arma::mat Y_tilde;
  
  //arma::mat Sigma_diag(size(Omega),fill::zeros);
  //Sigma_diag.diag() = 1/sqrt(Omega.diag());

  arma::mat expectation = design * beta + design_r * nu;
  expectation.each_row() += mu.t();
  
  
  arma::mat S = data.t() * data;
  arma::mat U = expectation.t() * expectation;
  
  arma::uvec pertub_vec = linspace<uvec>(0,k-1,k); 
  
  arma::uvec Omega_upper_tri = trimatu_ind(size(Omega),1);
  //arma::uvec Omega_upper_tri_full = trimatu_ind(size(Omega));

  arma::mat lambda_temp = zeros(size(Omega));
  lambda_temp(Omega_upper_tri) = lambda_curr;
  //lambda_temp.diag() *= 0;
  //Rcout << lambda_temp << endl;

  int n_upper_tri = Omega_upper_tri.n_elem;
  
  arma::mat tau_curr(k,k,fill::zeros);
  
  arma::uvec perms_j;
  arma::uvec just_j;
  arma::uvec ind_j(1,fill::zeros);
  arma::vec tauI;
  
  arma::mat S11;
  arma::mat S12;
  double S22;
  
  arma::mat U11;
  arma::mat U12;
  double U22;
  
  arma::mat Omega_11;
  arma::mat inv_Omega_11;
  
  arma::mat omega_12;
  arma::mat Sigma_omega_12(k-1,k-1,fill::zeros);
  arma::mat Omega_omega_12(k-1,k-1,fill::zeros);
  arma::mat chol_Omega_omega_12;
  arma::mat mu_omega_12;
  
  double gamma;
  double lambda_gamma,psi_gamma,chi_gamma;
  
  
  
  tau_curr.zeros();
  // update tau, using old Omega
  for(int j = 0 ; j < n_upper_tri ; ++j){
    tau_curr(Omega_upper_tri(j)) = 
      rinvGau(sqrt(lambda_temp(Omega_upper_tri(j))*lambda_temp(Omega_upper_tri(j))/
            (Omega(Omega_upper_tri(j))*Omega(Omega_upper_tri(j)))),
              lambda_temp(Omega_upper_tri(j))*lambda_temp(Omega_upper_tri(j)));
  }
  
  tau_curr += tau_curr.t(); // use symmertric to update lower tri
  //tau_curr *= Sigma_diag; // a test

  //Rcout << tau_curr << endl;
  for(int j = 0 ; j < k ; ++j){
    perms_j = find(pertub_vec!=j);
    just_j = find(pertub_vec==j);
    tauI = tau_curr(perms_j,just_j); // tau for this column
    
    
    // partitioning:
    S11 = S(perms_j,perms_j);
    S12 = S(perms_j,just_j);
    U11 = U(perms_j,perms_j);
    U12 = U(perms_j,just_j);
    Omega_11 = Omega(perms_j,perms_j);

    inv_Omega_11 = inv(Omega_11);
    //Rcout << "flag" << endl;
    // the current gamma=Omega_22-omega_12^T * Omega_{11}^{-1} * omega_{12}
    gamma = as_scalar( Omega(j,j)-Omega(just_j,perms_j)*inv_Omega_11*Omega(perms_j,just_j));
    
    // update omega_12, which is normal
    //Omega_omega_12 = (S(j,j)+lambda_temp(j,j))*inv_Omega_11+(1/gamma)*inv_Omega_11*U11*inv_Omega_11;
    Omega_omega_12 = (S(j,j))*inv_Omega_11+(1/gamma)*inv_Omega_11*U11*inv_Omega_11;
    Omega_omega_12.diag() += tauI;

    //Sigma_omega_12 = inv(Omega_omega_12);
    
    //mu_omega_12 = (S12-(1/gamma) * inv_Omega_11*U12);
    //mu_omega_12 = - Sigma_omega_12 * mu_omega_12;
    
    //omega_12 = mvnrnd(mu_omega_12, Sigma_omega_12);
    chol_Omega_omega_12 = chol(Omega_omega_12);
    mu_omega_12 = (S12-(1/gamma) * solve(Omega_11,U12));
    mu_omega_12 = - solve(Omega_omega_12,mu_omega_12);
    omega_12 = solve(chol_Omega_omega_12,randn(size(mu_omega_12))) + mu_omega_12;

    
    Omega(perms_j,just_j) = omega_12;
    Omega(just_j,perms_j) = omega_12.t();
    
    // update gamma, follow GIG 
    lambda_gamma = n/2+1;
    //psi_gamma = lambda_temp(j,j) + S(j,j);
    psi_gamma = S(j,j);
    chi_gamma = U(j,j) - 
      2*as_scalar(U12.t()*inv_Omega_11*omega_12)+
      as_scalar(omega_12.t()*inv_Omega_11*U11*inv_Omega_11*omega_12);
    
    gamma = rgig(lambda_gamma, chi_gamma, psi_gamma); // function in GIG_helper.cpp
    
    // update diagonal using determinant
    Omega(j,j) = gamma + as_scalar( omega_12.t()*inv_Omega_11*omega_12);
    
  }
  return;
}

