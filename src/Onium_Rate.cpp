#include "Onium_Rate.h"
#include "integrator.h"
#include "sampler.h"
#include "minimizer.h"
#include "random.h"
#include "approx_functions.h"
#include "matrix_elements.h"
#include "Langevin.h"
#include "Onium_Disso_dR.h"
#include "Onium_Reco_dR.h"
#include "Onium_predefine.h"
#include <iostream>
#include <cmath>
#include <gsl/gsl_math.h>


// --------------------------- Quarkonium 2->2 disso rate: QQbar[nl] + g --> Q + Qbar
template<>
OniumDissoRate22<2, double(*)(double, void *)>::
    OniumDissoRate22(std::string Name, std::string configfile, int n, int l, double(*f)(double, void *)):
StochasticBase<2>(Name+"/rate", configfile),
_f(f)
{
   // read configfile
   boost::property_tree::ptree config;
   std::ifstream input(configfile);
   read_xml(input, config);

   std::vector<std::string> strs;
   boost::split(strs, Name, boost::is_any_of("/"));
   std::string model_name = strs[0];
   std::string process_name = strs[1];
   auto tree = config.get_child(model_name + "." + process_name);
   _mass = tree.get<double>("mass");
   _n = n;
   _l = l;
   _Enl = get_onium_Enl_Coulomb(_mass, _n, _l);
   _aB = get_aB(_mass);
   _active = (tree.get<std::string>("<xmlattr>.status")=="active")?true:false;
}

// Calculate R = int_E[nl]^inf dR/dq * dq
template <>
scalar OniumDissoRate22<2, double(*)(double, void*)>::
calculate_scalar(std::vector<double> parameters){
    double v = parameters[0];
    double T = parameters[1];
    
    // dR/dq to be intergated
    auto dRdq = [v, T, this](double q){
        double params[5] = {v, T, this->_mass, this->_Enl, this->_aB};
        double result = this->_f(q, params);
        return result;
    };
    double qmin = _Enl;
    double qmax = 100*_Enl;
    double error;
    double res = quad_1d(dRdq, {qmin, qmax}, error);
    return scalar{res};
}
// Find the max of dR/dq
template <>
scalar OniumDissoRate22<2, double(*)(double, void*)>::
find_max(std::vector<double> parameters){
    double v = parameters[0];
    double T = parameters[1];
    auto dRdq = [v, T, this](double q){
        double params[5] = {v, T, this->_mass, this->_Enl, this->_aB};
        double result = this->_f(q, params);
        return -result;
    };
    double qmin = _Enl;
    double qmax = 100*_Enl;
    double res = -minimize_1d(dRdq, {qmin, qmax}, 1e-3, 100, 100);
    return scalar{res*1.5};
}

// Sample Final states using dR/dq
template<>
void OniumDissoRate22<2, double(*)(double, void *)>::
sample(std::vector<double> parameters, std::vector< fourvec > & FS){
    double v = parameters[0];
    double T = parameters[1];
    auto dRdq = [v, T, this](double q){
        double params[5] = {v, T, this->_mass, this->_Enl, this->_aB};
        double result = this->_f(q, params);
        return result;
    };
    double qmin = _Enl;
    double qmax = 100*_Enl;
    // sample gluon energy
    double q = sample_1d(dRdq, {qmin, qmax},StochasticBase<2>::GetFmax(parameters).s);
    // sample relative momentum of Q and Qbar
    double p_rel = std::sqrt((q-_Enl)*_mass);
    // sample costheta of q
    double r = Srandom::rejection(Srandom::gen);
    double cos = disso_gluon_costheta(q, v, T, r);
    // sample phi of q
    double phi = Srandom::dist_phi(Srandom::gen);
    // sample costheta and phi of p_rel
    double cos_rel = Srandom::dist_costheta(Srandom::gen);
    double phi_rel = Srandom::dist_phi(Srandom::gen);
    
    std::vector<double> momentum_gluon(3);
    std::vector<double> momentum_rel(3);
    std::vector<double> pQpQbar_final(6);
    momentum_gluon = polar_to_cartisian1(q, cos, phi);
    momentum_rel = polar_to_cartisian1(p_rel, cos_rel, phi_rel);
    pQpQbar_final = add_real_gluon(momentum_gluon, momentum_rel);
    double E_Q = momentum_to_energy(_mass, pQpQbar_final[0], pQpQbar_final[1], pQpQbar_final[2]);
    double E_Qbar = momentum_to_energy(_mass, pQpQbar_final[3], pQpQbar_final[4], pQpQbar_final[5]);
    FS.clear();
    FS.resize(2);
    // In Rest frame
    FS[0] = fourvec{E_Q, pQpQbar_final[0], pQpQbar_final[1], pQpQbar_final[2]};
    FS[1] = fourvec{E_Qbar, pQpQbar_final[3], pQpQbar_final[4], pQpQbar_final[5]};
    // boost to hydrocell frame
    FS[0] = FS[0].boost_back(0,0,v);
    FS[1] = FS[1].boost_back(0,0,v);
}


template <size_t N, typename F>
fourvec OniumDissoRate22<N, F>::calculate_fourvec(std::vector<double> parameters){
	return fourvec::unity();
}
template <size_t N, typename F>
tensor OniumDissoRate22<N, F>::calculate_tensor(std::vector<double> parameters){
	return tensor::unity();
}


// ------------- Quarkonium 2->2 reco rate: Q + Qbar --> QQbar[nl] + g
template<>
OniumRecoRate22<3, double(*)(double *, std::size_t, void *)>::
OniumRecoRate22(std::string Name, std::string configfile, int n, int l, double(*f)(double *, std::size_t, void *)):
StochasticBase<3>(Name+"/rate", configfile),
_f(f)
{
    // read configfile
    boost::property_tree::ptree config;
    std::ifstream input(configfile);
    read_xml(input, config);
    
    std::vector<std::string> strs;
    boost::split(strs, Name, boost::is_any_of("/"));
    std::string model_name = strs[0];
    std::string process_name = strs[1];
    auto tree = config.get_child(model_name + "." + process_name);
    _mass = tree.get<double>("mass");
    _n = n;
    _l = l;
    _Enl = get_onium_Enl_Coulomb(_mass, _n, _l);
    _aB = get_aB(_mass);
    _active = (tree.get<std::string>("<xmlattr>.status")=="active")?true:false;
}

// 2->2 reco rate
template <>
scalar OniumRecoRate22<3, double(*)(double *, std::size_t, void *)>::
find_max(std::vector<double> parameters){
    return scalar::unity();
}


// 2->2 reco rate
template <>
scalar OniumRecoRate22<3, double(*)(double *, std::size_t, void *)>::
calculate_scalar(std::vector<double> parameters){
    double x[3];
    x[0] = parameters[0]; // v
    x[1] = parameters[1]; // T
    x[2] = std::exp(parameters[2]); // exp(ln_p_rel)
    double params[3] = {this->_mass, this->_Enl, this->_aB};
    double result = prefactor_reco_gluon * this->_f(x, 3, params);
    return scalar{result};
}


// sample reco_real_gluon
template<>
void OniumRecoRate22<3, double(*)(double *, std::size_t, void *)>::
sample(std::vector<double> parameters, std::vector< fourvec > & FS){
    double v = parameters[0];
    double T = parameters[1];
    double p = std::exp(parameters[2]);      // relative momentum in QQbar rest frame
    
    double q = p*p/_mass + _Enl; // emitted gluon energy
    double cos = Sample_reco_gluon_costheta(v, T, q); // emitted gluon direction
    double phi = Srandom::dist_phi(Srandom::gen);
    
    std::vector<double> momentum_gluon(3);
    std::vector<double> momentum_onium(3);
    momentum_gluon = polar_to_cartisian1(q, cos, phi);
    momentum_onium = subtract_real_gluon(momentum_gluon);
    double E_onium = momentum_to_energy(2.*_mass-_Enl, momentum_onium[0], momentum_onium[1], momentum_onium[2]);
    FS.clear();
    FS.resize(1);
    // In Rest frame of QQbar
    FS[0] = fourvec{E_onium, momentum_onium[0], momentum_onium[1], momentum_onium[2]};
    // boost to hydrocell frame
    FS[0] = FS[0].boost_back(0,0,v);
}

template <size_t N, typename F>
fourvec OniumRecoRate22<N, F>::calculate_fourvec(std::vector<double> parameters){
    return fourvec::unity();
}
template <size_t N, typename F>
tensor OniumRecoRate22<N, F>::calculate_tensor(std::vector<double> parameters){
    return tensor::unity();
}

/*
// ------------------------------- Quarkonium 2->3 disso rate: QQbar[nl] + q --> Q + Qbar + q
template<>
OniumDissoRate23q<2, double(*)(double *, std::size_t, void *)>::
OniumDissoRate23q(std::string Name, std::string configfile, int n, int l, double(*)(double *, std::size_t, void *)):
StochasticBase<2>(Name+"/rate", configfile),
_f(f)
{
    // read configfile
    boost::property_tree::ptree config;
    std::ifstream input(configfile);
    read_xml(input, config);
    
    std::vector<std::string> strs;
    boost::split(strs, Name, boost::is_any_of("/"));
    std::string model_name = strs[0];
    std::string process_name = strs[1];
    auto tree = config.get_child(model_name + "." + process_name);
    _mass = tree.get<double>("mass");
    _n = n;
    _l = l;
    _Enl = get_onium_Enl_Coulomb(_mass, _n, _l);
    _aB = get_aB(_mass);
    _prel_up = get_prel_up(_aB, _n);
    _active = (tree.get<std::string>("<xmlattr>.status")=="active")?true:false;
}

// Calculate R = int dp1 dp2 dR/dp1/dp2
template <>
scalar OniumDissoRate23q<2, double(*)(double *, std::size_t, void *)>::
calculate_scalar(std::vector<double> parameters){
    double v = parameters[0];
    double T = parameters[1];
    
    // dR/dq to be intergated
    auto dRdp1dp2 = [v, T, this](double * x){
        double params[5] = {v, T, this->_mass, this->_Enl, this->_aB};
        double result = this->_f(x, 5, params);
        return result;
    };
    double p1up = 15.*T/std::sqrt(1.-v);

    double xl[5] = { _Enl, -1., 0., -1., 0. };
    double xu[5] = { p1up, 1., _prel_up, 1., TwoPi };
    double error;
    double res = prefactor_disso_ineq*vegas(dRdp1dp2, 5, xl, xu, error, 5000);
    return scalar{res};
}

// Sample final states using dR/dp1/dp2
template<>
void OniumDissoRate23q<2, double(*)(double *, std::size_t, void *)>::
sample(std::vector<double> parameters, std::vector< fourvec > & FS){
    double v = parameters[0];
    double T = parameters[1];
    
    
    
    
    
    
    auto dRdq = [v, T, this](double q){
        double params[5] = {v, T, this->_mass, this->_Enl, this->_aB};
        double result = this->_f(q, params);
        return result;
    };
    double qmin = _Enl;
    double qmax = 100*_Enl;
    // sample gluon energy
    double q = sample_1d(dRdq, {qmin, qmax},StochasticBase<2>::GetFmax(parameters).s);
    // sample relative momentum of Q and Qbar
    double p_rel = std::sqrt((q-_Enl)*_mass);
    // sample costheta of q
    double r = Srandom::rejection(Srandom::gen);
    double cos = disso_gluon_costheta(q, v, T, r);
    // sample phi of q
    double phi = Srandom::dist_phi(Srandom::gen);
    // sample costheta and phi of p_rel
    double cos_rel = Srandom::dist_costheta(Srandom::gen);
    double phi_rel = Srandom::dist_phi(Srandom::gen);
    
    std::vector<double> momentum_gluon(3);
    std::vector<double> momentum_rel(3);
    std::vector<double> pQpQbar_final(6);
    momentum_gluon = polar_to_cartisian1(q, cos, phi);
    momentum_rel = polar_to_cartisian1(p_rel, cos_rel, phi_rel);
    pQpQbar_final = add_real_gluon(momentum_gluon, momentum_rel);
    double E_Q = momentum_to_energy(_mass, pQpQbar_final[0], pQpQbar_final[1], pQpQbar_final[2]);
    double E_Qbar = momentum_to_energy(_mass, pQpQbar_final[3], pQpQbar_final[4], pQpQbar_final[5]);
    FS.clear();
    FS.resize(2);
    // In Rest frame
    FS[0] = fourvec{E_Q, pQpQbar_final[0], pQpQbar_final[1], pQpQbar_final[2]};
    FS[1] = fourvec{E_Qbar, pQpQbar_final[3], pQpQbar_final[4], pQpQbar_final[5]};
    // boost to hydrocell frame
    FS[0] = FS[0].boost_back(0,0,v);
    FS[1] = FS[1].boost_back(0,0,v);
}

template <size_t N, typename F>
fourvec OniumDissoRate23q<N, F>::calculate_fourvec(std::vector<double> parameters){
    return fourvec::unity();
}
template <size_t N, typename F>
tensor OniumDissoRate23q<N, F>::calculate_tensor(std::vector<double> parameters){
    return tensor::unity();
}
*/

template class OniumDissoRate22<2,double(*)(double, void*)>;
template class OniumRecoRate22<3, double(*)(double *, std::size_t, void*)>;
//template class OniumDissoRate23q<2, double(*)(double *, std::size_t, void *)>;
