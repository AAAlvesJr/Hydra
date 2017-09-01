/*----------------------------------------------------------------------------
 *
 *   Copyright (C) 2016 Antonio Augusto Alves Junior
 *
 *   This file is part of Hydra Data Analysis Framework.
 *
 *   Hydra is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Hydra is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Hydra.  If not, see <http://www.gnu.org/licenses/>.
 *
 *---------------------------------------------------------------------------*/

/*
 * multidimensional_fit.inl
 *
 *  Created on: 01/09/2017
 *      Author: Antonio Augusto Alves Junior
 */

#ifndef MULTIDIMENSIONAL_FIT_INL_
#define MULTIDIMENSIONAL_FIT_INL_


#include <iostream>
#include <assert.h>
#include <time.h>
#include <chrono>

//command line
#include <tclap/CmdLine.h>


//this lib
#include <hydra/device/System.h>
#include <hydra/host/System.h>
#include <hydra/Function.h>
#include <hydra/FunctionWrapper.h>
#include <hydra/Random.h>
#include <hydra/Copy.h>
#include <hydra/Tuple.h>
#include <hydra/Distance.h>
#include <hydra/multiarray.h>
#include <hydra/LogLikelihoodFCN2.h>
#include <hydra/Parameter.h>
#include <hydra/UserParameters.h>
#include <hydra/Pdf.h>
#include <hydra/Copy.h>
#include <hydra/Filter.h>
#include <hydra/GaussKronrodQuadrature.h>

//Minuit2
#include "Minuit2/FunctionMinimum.h"
#include "Minuit2/MnUserParameterState.h"
#include "Minuit2/MnPrint.h"
#include "Minuit2/MnMigrad.h"
#include "Minuit2/MnMinimize.h"
#include "Minuit2/MnMinos.h"
#include "Minuit2/MnContours.h"
#include "Minuit2/CombinedMinimizer.h"
#include "Minuit2/MnPlot.h"
#include "Minuit2/MinosError.h"
#include "Minuit2/ContoursError.h"
#include "Minuit2/VariableMetricMinimizer.h"

/*-------------------------------------
 * Include classes from ROOT to fill
 * and draw histograms and plots.
 *-------------------------------------
 */
#ifdef _ROOT_AVAILABLE_

#include <TROOT.h>
#include <TH1D.h>
#include <TH3D.h>
#include <TApplication.h>
#include <TCanvas.h>

#endif //_ROOT_AVAILABLE_

using namespace ROOT::Minuit2;

int main(int argv, char** argc)
{
	size_t nentries = 0;

	try {

		TCLAP::CmdLine cmd("Command line arguments for ", '=');

		TCLAP::ValueArg<size_t> EArg("n", "number-of-events","Number of events", true, 10e6, "size_t");
		cmd.add(EArg);

		// Parse the argv array.
		cmd.parse(argv, argc);

		// Get the value parsed by each arg.
		nentries = EArg.getValue();

	}
	catch (TCLAP::ArgException &e)  {
		std::cerr << "error: " << e.error() << " for arg " << e.argId()
														<< std::endl;
	}


	//generator
	hydra::Random<thrust::random::default_random_engine>
	Generator( std::chrono::system_clock::now().time_since_epoch().count() );

	//----------------------
    //fit function
	auto GAUSSIAN =  [=] __host__ __device__
			(unsigned int npar, const hydra::Parameter* params,
					unsigned int narg, double* x )
	{

		double g = 1.0;

		double mean[3] = { params[0].GetValue(), params[4].GetValue(), params[4].GetValue()  };
		double sigma[3]= { params[1].GetValue(), params[3].GetValue(), params[5].GetValue()  };

		for(size_t i=0; i<3; i++){

			double m2 = (x[i] - mean[i] )*(x[i] - mean[i] );
			double s2 = sigma[i]*sigma[i];
			g *= exp(-m2/(2.0 * s2 ))/( sqrt(2.0*s2*PI));
		}

		return g;
	};

	//______________________________________________________________

	std::string MeanX("MeanX");   // mean of gaussian in x-direction
	std::string SigmaX("SigmaX"); // sigma of gaussian in x-direction
	hydra::Parameter  meanx_p  = hydra::Parameter::Create()
			.Name(MeanX)
			.Value(0.5)
			.Error(0.0001)
			.Limits(-1.0, 1.0);

	hydra::Parameter  sigmax_p = hydra::Parameter::Create()
			.Name(SigmaX)
			.Value(0.5)
			.Error(0.0001)
			.Limits(0.01, 1.5);
	//______________________________________________________________

	std::string MeanY("MeanY");   // mean of gaussian in y-direction
	std::string SigmaY("SigmaY"); // sigma of gaussian in y-direction
	hydra::Parameter  meany_p  = hydra::Parameter::Create()
			.Name(MeanY)
			.Value(0.5)
			.Error(0.0001)
			.Limits(-1.0, 1.0);

	hydra::Parameter  sigmay_p = hydra::Parameter::Create()
			.Name(SigmaY)
			.Value(0.5)
			.Error(0.0001)
			.Limits(0.01, 1.5);

	//______________________________________________________________

	std::string MeanZ("MeanZ");   // mean of gaussian in z-direction
	std::string SigmaZ("SigmaZ"); // sigma of gaussian in z-direction

	hydra::Parameter  meanz_p  = hydra::Parameter::Create()
	.Name(MeanZ)
	.Value(0.5)
	.Error(0.0001)
	.Limits(-1.0, 1.0);

	hydra::Parameter  sigmaz_p = hydra::Parameter::Create()
	.Name(SigmaZ)
	.Value(0.5)
	.Error(0.0001)
	.Limits(0.01, 1.5);

	//______________________________________________________________

    auto gaussian = hydra::wrap_lambda(GAUSSIAN,
    		meanx_p, sigmax_p, meany_p, sigmay_p, meanz_p, sigmaz_p);

    //-----------------
    // some definitions
    double min   = -5.0;
    double max   =  5.0;

    double meanx  = 0.0;
    double sigmax = 0.5;

    double meany  = 1.0;
    double sigmay = 1.0;

    double meanz  = 2.0;
    double sigmaz = 2.0;


#ifdef _ROOT_AVAILABLE_

	TH3D hist_data_d("hist_data_d", "3D Gaussian - Data - Device",
			100,  min, max,
			100,  min, max,
			100,  min, max );

	TH3D hist_mc_d("hist_mc_d",   "3D Gaussian - MC - Host",
			100, min, max,
			100, min, max,
			100, min, max );

	TH1D hist_datax_d("hist_datax_d", "x projection", 100, min, max);
	TH1D hist_datay_d("hist_datay_d", "y projection", 100, min, max);
	TH1D hist_dataz_d("hist_dataz_d", "z projection", 100, min, max);

	TH1D hist_mcx_d("hist_mcx_d", "x projection", 100, min, max);
	TH1D hist_mcy_d("hist_mcy_d", "y projection", 100, min, max);
	TH1D hist_mcz_d("hist_mcz_d", "z projection", 100, min, max);


#endif //_ROOT_AVAILABLE_

	//device
	//------------------------
	{
		//3D device/host buffer
		hydra::multiarray<3, double, hydra::device::sys_t >  data_d(nentries);
		hydra::multiarray<3, double, hydra::host::sys_t >    data_h(nentries);

		//-------------------------------------------------------
		//gaussian
		Generator.Gauss(meanx, sigmax, data_d.begin(0), data_d.end(0));
		//gaussian
		Generator.Gauss(meany, sigmay, data_d.begin(1), data_d.end(1));
		//gaussian
		Generator.Gauss(meanz, sigmaz, data_d.begin(2), data_d.end(2));


		for(size_t i=0; i<10; i++)
			std::cout << "< Random::Gauss > [" << i << "] :" << data_d[i] << std::endl;

		//numerical integral to normalize the pdf
		std::array<double, 3> MinA{min, min, min};
		std::array<double, 3> MaxA{max, max, max};

		GenzMalikQuadrature<3, hydra::omp::sys_t> Integrator_d(MinA, MaxA, 10);


		//filtering
		auto FILTER = [=]__host__ __device__(unsigned int n, double* x)
		{

			bool decision = true;
			for (auto i=0; i<n; i++)
				decision &=((x[i] > min) && (x[i] < max ))
			return decision ;
		};

		auto filter = hydra::wrap_lambda(FILTER);
		auto range  = hydra::apply_filter(data_d,  filter);

		std::cout<< std::endl<< std::endl;
		for(size_t i=0; i<10; i++)
			std::cout << "< Selected data > [" << i << "] :" << range.begin()[i] << std::endl;


		//make model and fcn
		auto model = hydra::make_pdf(gaussian, Integrator_d );
		auto fcn   = hydra::make_loglikehood_fcn(range.first, range.second, model);

		//-------------------------------------------------------
		//fit
		ROOT::Minuit2::MnPrint::SetLevel(3);
		hydra::Print::SetLevel(hydra::WARNING);
		//minimization strategy
		MnStrategy strategy(2);

		// create Migrad minimizer
		MnMigrad migrad_d(fcn, fcn.GetParameters().GetMnState() ,  strategy);

		std::cout<<fcn.GetParameters().GetMnState()<<std::endl;

		// ... Minimize and profile the time

		auto start_d = std::chrono::high_resolution_clock::now();
		FunctionMinimum minimum_d =  FunctionMinimum(migrad_d(std::numeric_limits<unsigned int>::max(), 5));
		auto end_d = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed_d = end_d - start_d;

		// output
		std::cout<<"minimum: "<<minimum_d<<std::endl;

		//time
		std::cout << "-----------------------------------------"<<std::endl;
		std::cout << "| GPU Time (ms) ="<< elapsed_d.count() <<std::endl;
		std::cout << "-----------------------------------------"<<std::endl;

		//bring data to device
		hydra::copy( data_d.begin() , data_d.end(), data_h.begin() );

		//draw fitted function


#ifdef _ROOT_AVAILABLE_
		for(auto value : data_h)
			hist_gaussian_d.Fill( value);

		//draw fitted function
		hist_fitted_gaussian_d.Sumw2();
		for (size_t i=0 ; i<=100 ; i++) {
			double x = hist_fitted_gaussian_d.GetBinCenter(i);
	        hist_fitted_gaussian_d.SetBinContent(i, fcn.GetPDF()(x) );
		}
		hist_fitted_gaussian_d.Scale(hist_gaussian_d.Integral()/hist_fitted_gaussian_d.Integral() );
#endif //_ROOT_AVAILABLE_

	}//device end



	//host
	//------------------------





#ifdef _ROOT_AVAILABLE_
	TApplication *myapp=new TApplication("myapp",0,0);

	/*
	//draw histograms
	TCanvas canvas_d("canvas_d" ,"Distributions - Device", 500, 500);
	hist_gaussian_d.Draw("hist");
	hist_fitted_gaussian_d.Draw("histsameC");
	hist_fitted_gaussian_d.SetLineColor(2);

	//draw histograms
	TCanvas canvas_h("canvas_h" ,"Distributions - Host", 500, 500);
	hist_gaussian_h.Draw("hist");
	hist_fitted_gaussian_h.Draw("histsameC");
	hist_fitted_gaussian_h.SetLineColor(2);
*/
	myapp->Run();

#endif //_ROOT_AVAILABLE_

	return 0;



}
#endif /* MULTIDIMENSIONAL_FIT_INL_ */