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
#include <sstream>
#include <unistd.h>

#include "simpleLogger.h"
#include "Medium_Reader.h"
#include "workflow.h"
#include "pythia_jet_gen.h"
#include "Hadronize.h"
#include "jet_finding.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

void output_jet(std::string fname, std::vector<particle> plist){
    std::ofstream f(fname);
    for (auto & p : plist) f << p.pid << " " << p.p << " " << p.x0 << " " << p.x << std::endl;
    f.close();
}

int main(int argc, char* argv[]){
    using OptDesc = po::options_description;
    OptDesc options{};
    options.add_options()
          ("help", "show this help message and exit")         
          ("hydro",
           po::value<fs::path>()->value_name("PATH")->required(),
           "hydro file")
    ;
    po::variables_map args{};
    try{
        po::store(po::command_line_parser(argc, argv).options(options).run(), args);
        if (args.count("help")){
                std::cout << "usage: " << argv[0] << " [options]\n"
                          << options;
                return 0;
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

        /// use process id to define filename
        int processid = getpid();
        std::stringstream outputfilename2;

        std::vector<current> clist;
        /// Initialzie a hydro reader
        Medium<3> med1(args["hydro"].as<fs::path>().string());

        double xs=0, ys=0, etas=0; 
        fourvec total{0,0,0,0};
        while(med1.load_next()){
            double current_hydro_clock = med1.get_tauL();
            double hydro_dtau = med1.get_hydro_time_step();
           for(int it=0; it<1; it++){
            double t = current_hydro_clock + it*hydro_dtau;
           
            bool make_source =  (.2*5.076<=t)&& (t <= 4.21*5.076);
            LOG_INFO << make_source << t;
            xs = t;
            
            if(make_source){
                double T = 0.0, vx = 0.0, vy = 0.0, vz = 0.0;
                fourvec x{t*std::cosh(etas), xs, ys, t*std::sinh(etas)}; 
                LOG_INFO << "here1";
                med1.interpolate(x, T, vx, vy, vz);
                double vzgrid = vz;
                LOG_INFO << "here2" << vx << " " << vy;
                current J; 
                vx = 0*vx/std::sqrt(1-vzgrid*vzgrid);
                vy = 0*vy/std::sqrt(1-vzgrid*vzgrid);
                fourvec ploss{1,1,0,0};
                ploss = ploss*(hydro_dtau/0.4/5.076);
                total = total + ploss;
                ploss = ploss.boost_to(0, 0, vzgrid).boost_to(vx, vy, 0);
                J.p = ploss;
                
                J.x = x;
                J.tau = x.tau();
                J.rap = x.rap();
                J.v[0] = vx; J.v[1] = vy; J.v[2] = 0;
                J.cs = std::sqrt(.333);
                //if (T>0.37) J.cs = std::sqrt(.3);
                //else if (T>0.21)J.cs=std::sqrt(0.25+(T-.21)*.05/(.37-.21));
                //else if (T>0.15)J.cs=std::sqrt(0.15+(T-.15)*.1/(.21-.15));
                double gammaR = 1./sqrt(1.-vx*vx-vy*vy);
                J.a00 = gammaR; 
                J.a01 = gammaR*vx;
                J.a02 = gammaR*vy;
                J.a11 = 1.+(gammaR-1.)*vx*vx;
                J.a12 = (gammaR-1.)*vx*vy;
                J.a22 = 1.+(gammaR-1.)*vy*vy;
                clist.push_back(J);  
            }
           }
        }
        LOG_INFO << "here3" << total;
        std::stringstream outputfilename1;
        outputfilename1 << "gT.dat";
        TestSource(clist, outputfilename1.str());
LOG_INFO << "here4";
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



