#include "predefine.h"
#include <cmath>

char LO[] = "Leading-order";
char GB[] = "Gunion-Bertsch";
char GBHT[] = "Gunion-Bertsch-higher-twist";

bool type1_warned = false;
bool type2_warned = false; 

const double c4d9 = 4./9.;
const double c1d9 = 1./9.;
const double c16pi = 16.*M_PI;
const double c48pi = 48.*M_PI;
const double c16pi2 = 16.*M_PI*M_PI;
const double c64d9pi2 = 64./9.*M_PI*M_PI;
const double c256pi4 = 256.*std::pow(M_PI, 4);

// number of color=3 (3*3-1 = 8 gluons), number of flavor=3, (u,d,s quark)
const int Nc = 3, nf = 3;

// the prefractor for gluon debye mass with Boltzmann statistics
// mD^2 = 8\pi*(Nc+nf)*alpha_s*T^2 ~ 15*alpha_s*T^2
// Note that if using quantum statistics, this will be
// mD^2 = 4\pi/3*(Nc+nf/2)*alpha_s*T^2 ~ 18*alpha_s*T^2
const double pf_g = 8./M_PI*(Nc + nf); // prefractor for gluon self energy^2

// For QCD coupling constant
// alpha_s = alpha_0 = 4\pi/(11Nc/3-2Nf/3)/log(Q^2/LambdaQCD^2)
const double alpha0 = 4.*M_PI/(11./3.*Nc - 2./3.*nf); // alpha_s(Q2 = e*Lambda2)
const double Lambda = 0.2; // [GeV] Lambda QCD = 0.2 GeV
const double Lambda2 = Lambda*Lambda; // [GeV^2] Lambda QCD squared
const double mu2_left = Lambda2*std::exp(1.0); // minimum cut on Q2, where alpha = alpha_0
