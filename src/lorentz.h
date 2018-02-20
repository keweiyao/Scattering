#ifndef LORENTZ_H
#define LORENTZ_H
#include <iostream>
#include <cmath>
#include "hdf5.h"
#include "hdf5_hl.h"
#include <cstdlib>

const double tiny_v2 = 1e-15;
struct scalar {
	double s;
	friend std::ostream& operator<<(std::ostream& os, const scalar& A){
    	os << A.s;
    	return os;
	  }
	  friend scalar operator+(const scalar& A, const scalar& B){
		return scalar{A.s+B.s};
	  }
	  friend scalar operator-(const scalar& A, const scalar& B){
		return scalar{A.s-B.s};
	  }
	  template<typename T>
	  scalar operator*(T u){
		return scalar{s*u};
	  }
	static size_t size(void){return 1;}
	void set(int i, double value) {s = value;};
	double get(int i) {return s;};
};

struct fourvec {
  double a[4];
  double t(void) const {return a[0];};
  double x(void) const {return a[1];};
  double y(void) const {return a[2];};
  double z(void) const {return a[3];};
  friend std::ostream& operator<<(std::ostream& os, const fourvec& A){
    os << A.t() << " " << A.x() << " " << A.y() << " " << A.z();
    return os;
  }
  friend fourvec operator+(const fourvec& A, const fourvec& B){
    return fourvec{A.t()+B.t(),A.x()+B.x(),A.y()+B.y(),A.z()+B.z()};
  }
  friend fourvec operator-(const fourvec& A, const fourvec& B){
    return fourvec{A.t()-B.t(),A.x()-B.x(),A.y()-B.y(),A.z()-B.z()};
  }
  template<typename T>
  fourvec operator*(T s){
    return fourvec{a[0]*s,a[1]*s,a[2]*s,a[3]*s};
  }
  fourvec boost_to(double vx, double vy, double vz){
  	double v2 = std::max(vx*vx + vy*vy + vz*vz, tiny_v2);
  	double gamma = 1./std::sqrt(1. - v2);
  	double gamma_minus_one = gamma - 1.;
  	double a_dot_v = vx*a[1] + vy*a[2] + vz*a[3];
  	return fourvec{
  		 gamma*(a[0] - a_dot_v),
  		-gamma*vx*a[0] + a[1] + gamma_minus_one*vx*a_dot_v/v2,
  		-gamma*vy*a[0] + a[2] + gamma_minus_one*vy*a_dot_v/v2,
  		-gamma*vz*a[0] + a[3] + gamma_minus_one*vz*a_dot_v/v2
  	};
  }
  fourvec boost_back(double vx, double vy, double vz){
	return boost_to(-vx, -vy, -vz);
  }
  friend double dot(const fourvec& A, const fourvec& B) {
	return A.t()*B.t() - A.x()*B.x() - A.y()*B.y() - A.z()*B.z();
  }
  static size_t size(void){return 4;}
  void set(int i, double value) {a[i] = value;};
  double get(int i) {return a[i];};
};

struct tensor {
  double T[4][4];
  double tt(void) const {return T[0][0];};
  double tx(void) const {return T[0][1];};
  double ty(void) const {return T[0][2];};
  double tz(void) const {return T[0][3];};
  
  double xt(void) const {return T[1][0];};
  double xx(void) const {return T[1][1];};
  double xy(void) const {return T[1][2];};
  double xz(void) const {return T[1][3];};
  
  double yt(void) const {return T[2][0];};
  double yx(void) const {return T[2][1];};
  double yy(void) const {return T[2][2];};
  double yz(void) const {return T[2][3];};
  
  double zt(void) const {return T[3][0];};
  double zx(void) const {return T[3][1];};
  double zy(void) const {return T[3][2];};
  double zz(void) const {return T[3][3];};
  
  friend std::ostream& operator<<(std::ostream& os, const tensor& A){
    for(auto& row : A.T){
    	for(auto& col : row){
    		os << col << " ";
    	}
    	os << std::endl;
    }
    return os;
  }
  friend tensor operator+(const tensor& A, const tensor& B){
    tensor res;
    for(auto i=0; i<4; ++i){
        for(auto j=0; j<4; ++j){
        	res.T[i][j] = A.T[i][j] + B.T[i][j];
        }
    }
    return res;
  }
  friend tensor operator-(const tensor& A, const tensor& B){
    tensor res;
    for(auto i=0; i<4; ++i){
        for(auto j=0; j<4; ++j){
        	res.T[i][j] = A.T[i][j] - B.T[i][j];
        }
    }
    return res;
  }
  template<typename U>
  tensor operator*(U s){
    tensor res{0};
    for(auto i=0; i<4; ++i){
        for(auto j=0; j<4; ++j){
        	res.T[i][j] = T[i][j]*s;
        }
    }
    return res;
  }
  tensor boost_to(double vx, double vy, double vz){
  	double v2 = std::max(vx*vx + vy*vy + vz*vz, tiny_v2);
  	double gamma = 1./std::sqrt(1. - v2);
  	double gm1 = gamma - 1.;
  	double L[4][4];
  	L[0][0] = gamma; L[0][1] = -gamma*vx; L[0][2] = -gamma*vy; L[0][3] = -gamma*vz;
    L[1][1] = 1+gm1*vx*vx/v2; L[1][2] = gm1*vx*vy/v2; L[1][3] = gm1*vx*vz/v2;
    L[2][2] = 1+gm1*vy*vy/v2; L[2][3] = gm1*vy*vz/v2;
  	L[3][3] = 1+gm1*vz*vz/v2;
  	for(auto i=1; i<4; ++i){
  		for(auto j=0; j<i; ++j){
  			L[i][j] = L[j][i];
  		}
  	}
  	tensor res{0};
  	for(auto mu=0; mu<4; ++mu){
  		for(auto nu=0; nu<4; ++nu){
  			for(auto i=0; i<4; ++i){
  				for(auto j=0; j<4; ++j){
  					res.T[mu][nu] += L[mu][i]*L[nu][j]*T[i][j];
  				}
  			}
  		}
  	}
  	return res;
  }
  tensor boost_back(double vx, double vy, double vz){
  	return boost_to(-vx, -vy, -vz);
  }
  double trace(void){
    return T[0][0]-T[1][1]-T[2][2]-T[3][3];
  }
  void set(int i, double value) {T[i/4][i%4] = value;};
  double get(int i) {return T[i/4][i%4];};
  static size_t size(void){return 16;}
};



#endif