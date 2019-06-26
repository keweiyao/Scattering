#include <string>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <exception>
#include <random>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

#include "simpleLogger.h"
#include "workflow.h"
#include "random.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

int main(int argc, char* argv[]){
    using OptDesc = po::options_description;
    OptDesc options{};
    options.add_options()
          ("help", "show this help message and exit")
          ("nparticles",
            po::value<int>()->value_name("INT")->default_value(1000,"1000"),
           "number of hard partons")
          ("energy",
            po::value<double>()->value_name("FLOAT")->default_value(10.0,"10.0"),
           "fixed parton energy [GeV]")
          ("temp",
           po::value<double>()->value_name("FLOAT")->default_value(0.3,"0.3"),
           "medium temperature at t=t0 [GeV]")
          ("taui",
           po::value<double>()->value_name("FLOAT")->default_value(0.0,"0.0"),
           "medium starting time [fm/c]")
          ("tauf",
           po::value<double>()->value_name("FLOAT")->default_value(5.0,"5.0"),
           "medium stopping time [fm/c]")
          ("dtau",
           po::value<double>()->value_name("FLOAT")->default_value(0.1,"0.1"),
           "hydro time step dtau [fm/c]")
          ("nu",
           po::value<double>()->value_name("FLOAT")->default_value(0.5,"0.5"),
           "T = T0 * (tau0/tau) ^ (2-1/nu)")
          ("lido-setting,s", 
            po::value<fs::path>()->value_name("PATH")->required(),
           "Lido table setting file")
          ("lido-table,t", 
            po::value<fs::path>()->value_name("PATH")->required(),
           "Lido table path to file")
          ("mu,m", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "medium scale paramtmer")
          ("afix,f", 
            po::value<double>()->value_name("FLOAT")->default_value(-1.0,"-1.0"),
            "fixed alphas value, -1 is running")
          ("k-factor,k", 
            po::value<double>()->value_name("FLOAT")->default_value(0.0,"0.0"),
            "K-factor for the delta-qhat")
          ("t-scale,a", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "rescale the T-dependence")
          ("e-scale,b", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "rescale the p-dependence")
          ("t-power,p", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "T-dependence power")
          ("e-power,q", 
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"),
            "p-dependence power")
          ("gamma,g", 
            po::value<double>()->value_name("FLOAT")->default_value(0.0,"0.0"),
            "kpara / kperp anisotropy parameter")
          ("qcut,c",
            po::value<double>()->value_name("FLOAT")->default_value(1.0,"1.0"), 
            "separation scale Q2 = qcut * mD2")  
          ("mass",
            po::value<double>()->value_name("FLOAT")->default_value(1.3,"1.3"), 
            "quark mass")  
          ("pid",
            po::value<int>()->value_name("INT")->default_value(4,"4"), 
            "particle pid, 4 or 21")  
    ;
    po::variables_map args{};
    try{
        po::store(po::command_line_parser(argc, argv).options(options).run(), args);
        if (args.count("help")){
                std::cout << "usage: " << argv[0] << " [options]\n"
                          << options;
                return 0;
        }    
        // check lido setting
        if (!args.count("lido-setting")){
            throw po::required_option{"<lido-setting>"};
            return 1;
        }
        else{
            if (!fs::exists(args["lido-setting"].as<fs::path>())){
                throw po::error{"<lido-setting> path does not exist"};
                return 1;
            }
        }
        // check lido table
        std::string table_mode;
        if (!args.count("lido-table")){
            throw po::required_option{"<lido-table>"};
            return 1;
        }
        else{
            table_mode = (fs::exists(args["lido-table"].as<fs::path>())) ? 
                         "old" : "new";
        }
        // check pid
        if (!args.count("pid")){
            if ( (std::abs(args["pid"].as<int>()) != 4)
				&& (std::abs(args["pid"].as<int>()) != 21) ){
		        throw po::error{"pid only suppots 4, 21"};
		        return 1;
			}
        }

        // start
        double L = 10*5.026; // box size
        std::vector<particle> plist, new_plist, pOut_list;
		int pid = args["pid"].as<int>();
		double E0 = args["energy"].as<double>();
		double M = args["mass"].as<double>();
        double pabs = std::sqrt(E0*E0-M*M);
        
		for (int i=0; i<args["nparticles"].as<int>(); i++){
		    double phi = Srandom::dist_phi(Srandom::gen);
		    double ctheta = Srandom::dist_costheta(Srandom::gen);
		    double stheta = std::sqrt(1.-ctheta*ctheta);
			fourvec p0{E0, pabs*stheta*std::cos(phi), pabs*stheta*std::sin(phi),pabs*ctheta };
            fourvec x0{0, L*Srandom::init_dis(Srandom::gen),L*Srandom::init_dis(Srandom::gen),L*Srandom::init_dis(Srandom::gen)};
			particle entry;
			entry.p = p0; // 
			entry.p0 = p0; // 
			entry.mass = M; // mass
			entry.pid = pid; // light quark
			entry.x0 = x0; // initial position
			entry.x = x0; // initial position
			entry.weight = 1.;
			entry.Tf = 0.0;
			entry.is_vac = false;
			entry.is_virtual = false;
			entry.vcell.resize(3);
			entry.vcell[0] = 0.; 
			entry.vcell[1] = 0.; 
			entry.vcell[2] = 0.; 
			plist.push_back(entry); 
		}

        /// Lido init
        initialize(table_mode,
                args["lido-setting"].as<fs::path>().string(),
                args["lido-table"].as<fs::path>().string(),
                args["mu"].as<double>(),
                args["afix"].as<double>(),
                args["k-factor"].as<double>(),
                args["t-scale"].as<double>(),
                args["e-scale"].as<double>(),
                args["t-power"].as<double>(),
                args["e-power"].as<double>(),
                args["gamma"].as<double>(),
                args["qcut"].as<double>(),
                0
                );

        /// Assign each quark a transverse position according to TRENTo Nbin output
        /// Freestream particle to the start of hydro
		double ti = args["taui"].as<double>() * 5.026; // convert to GeV^-1
		double tf = args["tauf"].as<double>() * 5.026; // convert to GeV^-1
		double dt = args["dtau"].as<double>() * 5.026; // convert to GeV^-1
		double T0 = args["temp"].as<double>();
		double nu = args["nu"].as<double>();
		int Nsteps = int((tf-ti)/dt)+1;
        for (auto & p : plist){
            p.freestream(ti);
        }

        // initial E
        double Ei = mean_E(plist);

		std::ofstream fff("stat.dat");
        for (int i=0; i<Nsteps; i++){
           
			double t = ti + dt*i;
            double T = T0 * std::pow(ti/t, 2./3.-1./nu/3.);
			if (i%100==0) LOG_INFO << t/5.026 << " [fm/c]\t" << T << " [GeV]" << " #=" << plist.size();

            // One particle evolution
            new_plist.clear();
            for (auto & p : plist){
                // for Open heavy flavor
                if (std::abs(p.pid) < 22)
                    update_particle_momentum_Lido(dt, T, {0., 0., 0.}, p, pOut_list);  

                // for Hidden heavy flavor
                if (p.pid == 553)
                    update_Onium_Disso(dt, T, {0., 0., 0.}, p, pOut_list);

                for (auto & pp : pOut_list) new_plist.push_back(pp);
            }

            // Two particle evolution
            int Npair = 0;
            for (auto & p1 : plist){
                if (p1.pid == 5) {
                    auto xQ = p1.x;
                    for (auto & p2 : plist){
                        if (p2.pid == -5) {
                            auto xQbar = p2.x;
                            double dist2 = 0.;
                            for (int k=1; k<4; k++){ 
                                double dx = xQ.a[k]-xQbar.a[k];
                                dist2 += std::pow(std::min(dx,L-dx), 2);
                            }
                            if (dist2 < std::pow(1.*5.026, 2)) Npair ++;
                            // check if spatially close
                            // apply recombinate prob
                        }
                    }
                }
            }
            if (i%100==0) LOG_INFO << "Npair = " << Npair;

            plist.clear();
            for (auto & p : new_plist){
                // apply perodic boundary condition
                for (int k=1; k<4; k++){ 
                    if (p.x.a[k] > L) p.x.a[k] = p.x.a[k] - L;
                    if (p.x.a[k] < 0) p.x.a[k] = p.x.a[k] + L;
                }
                plist.push_back(p);
            }

            int No=0;
            for (auto & p : plist) {
                if (p.pid == 553) No++;
            }
            fff << t/5.026 << " " << No << std::endl;
        }
		
		// final E
        double Ef = mean_E(plist);
		LOG_INFO << "N-particles\t" << plist.size();
		LOG_INFO << "Initial energy\t" << Ei << " [GeV]";
		LOG_INFO << "Final energy\t" << Ef << " [GeV]";
    }
    catch (const po::required_option& e){
        std::cout << e.what() << "\n";
        std::cout << "usage: " << argv[0] << " [options]\n"
                  << options;
        return 1;
    }
    catch (const std::exception& e) {
       // For all other exceptions just output the error message.
       std::cerr << e.what() << '\n';
       return 1;
    }    
    return 0;
}


