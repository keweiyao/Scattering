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
#include "Medium_Reader.h"
#include "workflow.h"
#include "pythia_wrapper.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

int main(int argc, char* argv[]){
    using OptDesc = po::options_description;
    OptDesc options{};
    options.add_options()
          ("help", "show this help message and exit")
          ("pythia-setting,y",
           po::value<fs::path>()->value_name("PATH")->required(),
           "Pythia setting file")
          ("pythia-events,n",
            po::value<int>()->value_name("INT")->default_value(50000,"50000"),
           "number of Pythia events")
          ("ic,i",
            po::value<fs::path>()->value_name("PATH")->required(),
           "trento initial condition file")
          ("eid,j",
           po::value<int>()->value_name("INT")->default_value(0,"0"),
           "trento event id")
          ("hydro",
           po::value<fs::path>()->value_name("PATH")->required(),
           "hydro file")
          ("lido-setting,s", 
            po::value<fs::path>()->value_name("PATH")->required(),
           "Lido table setting file")
          ("lido-table,t", 
            po::value<fs::path>()->value_name("PATH")->required(),
           "Lido table path to file")   
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
        // check trento event
        if (!args.count("ic")){
            throw po::required_option{"<ic>"};
            return 1;
        }
        else{
            if (!fs::exists(args["ic"].as<fs::path>())) {
                throw po::error{"<ic> path does not exist"};
                return 1;
            }
        }
        // check pythia setting
        if (!args.count("pythia-setting")){
            throw po::required_option{"<pythia-setting>"};
            return 1;
        }
        else{
            if (!fs::exists(args["pythia-setting"].as<fs::path>())) {
                throw po::error{"<pythia-setting> path does not exist"};
                return 1;
            }
        }
        // check hydro setting
        if (!args.count("hydro")){
            throw po::required_option{"<hydro>"};
            return 1;
        }
        else{
            if (!fs::exists(args["hydro"].as<fs::path>())) {
                throw po::error{"<hydro> path does not exist"};
                return 1;
            }
        }

        // start
        std::vector<particle> plist;

        /// HardGen
        HardGen hardgen(
                args["pythia-setting"].as<fs::path>().string(), 
                args["ic"].as<fs::path>().string(),
                args["eid"].as<int>()           
        );
        hardgen.Generate(
                plist, 
                args["pythia-events"].as<int>(),
                2.5,
		true
        );
    
        /// Read Hydro
        Medium<2> med1(args["hydro"].as<fs::path>().string());

        /// Lido init
        initialize(table_mode,
                args["lido-setting"].as<fs::path>().string(),
                args["lido-table"].as<fs::path>().string()
                );

        /// Assign each quark a transverse position according to TRENTo Nbin output
        /// Freestream particle to the start of hydro
	LOG_INFO << "freestream particles to tau = " <<  med1.get_tauH();
        for (auto & p : plist){
            double tau_fs = med1.get_tauH();
	    double tau = std::sqrt(p.x.t()*p.x.t()-p.x.z()*p.x.z());
	    if (tau < tau_fs) {
                p.freestream(compute_realtime_to_propagate(
                               tau_fs-tau, p.x, p.p));
                for (auto & ip : p.radlist) ip.x = p.x;
            }
        }

        // run
        int Ns = 10;
        std::vector<particle> pOut_list;
        double sys_tau = 0;
        while(med1.load_next()){
            double current_hydro_clock = med1.get_tauL();
            sys_tau = current_hydro_clock;
            double hydro_dtau = med1.get_hydro_time_step();
            for (int i=0; i<Ns; ++i){
                LOG_INFO << "Tau = " << sys_tau /5.026 << " fm/c";
                double dtau = hydro_dtau/Ns; // use smaller dt step
                sys_tau += dtau;
                for (auto & p : plist){
                    if (p.x.t()*p.x.t()-p.x.z()*p.x.z() > sys_tau*sys_tau)
                        continue;
                    double T = 0.0, vx = 0.0, vy = 0.0, vz = 0.;
                    med1.interpolate(p.x, T, vx, vy, vz);

                    double vabs = std::sqrt(vx*vx + vy*vy + vz*vy);
                    // regulate v
                    if (vabs > 1.){
                        if (vabs > 1.-1e-6) {
                            double rescale = (1.-1e-6)/vabs;
                            vx *= rescale;
                            vy *= rescale;    
                            vz *= rescale;    
                        }
                    }
                    // x,p-update
                    int fs_size = update_particle_momentum_Lido(
                                  dtau, T, {vx, vy, vz}, p, pOut_list);
                }
            }
        }
        // do any vac radiation that is left
        for (auto & p : plist){
            for (auto & ip : p.radlist){
                 if (ip.is_vac){
                     p.p = p.p - ip.p0;
		     p.p.a[0] = std::sqrt(p.p.pabs2()+p.mass*p.mass);
                 }
            }
        }
        output_oscar(plist, 4, "c-quark-frzout.dat");
        output_oscar(plist, 5, "b-quark-frzout.dat");
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


