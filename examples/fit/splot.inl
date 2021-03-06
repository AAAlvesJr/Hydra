/*----------------------------------------------------------------------------
 *
 *   Copyright (C) 2016 - 2017 Antonio Augusto Alves Junior
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
 * splot.inl
 *
 *  Created on: 17/09/2017
 *      Author: Antonio Augusto Alves Junior
 */

#ifndef SPLOT_INL_
#define SPLOT_INL_


#include <iostream>
#include <assert.h>
#include <time.h>
#include <chrono>

//command line
#include <tclap/CmdLine.h>

//this lib
#include <hydra/device/System.h>
#include <hydra/host/System.h>
#include <hydra/omp/System.h>
#include <hydra/Function.h>
#include <hydra/FunctionWrapper.h>
#include <hydra/Random.h>
#include <hydra/LogLikelihoodFCN.h>
#include <hydra/Parameter.h>
#include <hydra/UserParameters.h>
#include <hydra/Pdf.h>
#include <hydra/AddPdf.h>
#include <hydra/Copy.h>
#include <hydra/Filter.h>
#include <hydra/GaussKronrodQuadrature.h>
#include <hydra/SPlot.h>
#include <hydra/DenseHistogram.h>
#include <hydra/SparseHistogram.h>

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

	//-----------------
    // some definitions
    double min   =  0.0;
    double max   =  10.0;

	//generator
	hydra::Random<> Generator( std::chrono::system_clock::now().time_since_epoch().count() );

	//----------------------
    //fit model

	//gaussian
	auto GAUSSIAN =  [=] __host__ __device__
			(unsigned int npar, const hydra::Parameter* params,unsigned int narg, double* x )
	{
		double m2 = (x[0] -  params[0])*(x[0] - params[0] );
		double s2 = params[1]*params[1];
		double g = exp(-m2/(2.0 * s2 ))/( sqrt(2.0*s2*PI));

		return g;
	};

	std::string  Mean("Mean"); 	// mean of gaussian
	std::string Sigma("Sigma"); // sigma of gaussian
	hydra::Parameter  mean_p  = hydra::Parameter::Create().Name(Mean).Value( 2.5) .Error(0.0001).Limits(0.0, 10.0);
	hydra::Parameter  sigma_p = hydra::Parameter::Create().Name(Sigma).Value(0.5).Error(0.0001).Limits(0.01, 1.5);

    auto gaussian = hydra::wrap_lambda(GAUSSIAN, mean_p, sigma_p);

    //--------------------------------------------
    //exponential
    auto EXPONENTIAL =  [=] __host__ __device__
    		(unsigned int npar, const hydra::Parameter* params,unsigned int narg, double* x )
    {
    	double tau = params[0];
    	return exp(x[0]*tau);
    };

    //parameters
    std::string  Tau("Tau"); 	// tau of the exponential
    hydra::Parameter  tau_p  = hydra::Parameter::Create().Name(Tau).Value(0.0) .Error(0.0001).Limits(-10.0, 10.0);

    //get a hydra lambda
    auto exponential = hydra::wrap_lambda(EXPONENTIAL, tau_p);

    //------------------
    //yields
	std::string na("N_Gauss");
	std::string nc("N_Exp");

	hydra::Parameter N_Gauss_p(na ,nentries, sqrt(nentries), nentries-nentries/2 , nentries+nentries/2) ;
	hydra::Parameter N_Exp_p(nc ,nentries, sqrt(nentries), nentries-nentries/2 , nentries+nentries/2) ;

    std::array<hydra::Parameter*, 2>  yields{ &N_Gauss_p, &N_Exp_p };

	//device
	//------------------------
#ifdef _ROOT_AVAILABLE_

	TH1D hist_data_dicriminating_d("data_discriminating_d", "Discriminating variable", 100, min, max);
	TH1D hist_data_control_d("data_control_d", "Control Variable", 100, min, max);
	TH1D hist_fit_d("fit_d", "Discriminating variable", 100, min, max);
	TH1D hist_control_1_d("control_1_d", "Control Variable: Gaussian PDF",    100, min, max);
	TH1D hist_control_2_d("control_2_d", "Control Variable: Exponential PDF",    100, min, max);
#endif //_ROOT_AVAILABLE_

	{
		std::cout << "=========================================="<<std::endl;
		std::cout << "|            <--- DEVICE --->            |"  <<std::endl;
		std::cout << "=========================================="<<std::endl;

		//------------------
	    //make model
		//numerical integral to normalize the pdfs
		hydra::GaussKronrodQuadrature<61,50, hydra::omp::sys_t> GKQ61_d(min,  max);

		//convert functors to pdfs
		auto Gauss_PDF = hydra::make_pdf(gaussian  , GKQ61_d);
		auto    Exp_PDF = hydra::make_pdf(exponential, GKQ61_d);

		auto model = hydra::add_pdfs(yields, Gauss_PDF, Exp_PDF);

		model.SetExtended(1);

		//1D data containers
		hydra::multiarray<2, double, hydra::device::sys_t> data_d(2*nentries);
		hydra::multiarray<2, double, hydra::host::sys_t>   data_h(2*nentries);

		//-------------------------------------------------------
		// Generate toy data

		//first component: [Gaussian] x [Exponential]
		// gaussian
		Generator.Gauss(mean_p.GetValue()+2.5, sigma_p.GetValue()+0.5, data_d.begin(0), data_d.begin(0)+nentries);

		// exponential

		Generator.Exp(tau_p.GetValue()+1.0, data_d.begin(1),  data_d.begin(1)+nentries);

		//second component: [Exponential] -> [Gaussian]
		// gaussian
		Generator.Gauss(mean_p.GetValue()-1.0, 0.5, data_d.begin(1) + nentries, data_d.begin(1) + nentries + nentries/2);
		Generator.Gauss(mean_p.GetValue()+4.5, 0.5, data_d.begin(1) + nentries + nentries/2, data_d.end(1));

		// exponential
		Generator.Exp(tau_p.GetValue()+5.0, data_d.begin(0)+nentries,  data_d.end(0));

		std::cout<< std::endl<< "Generated data:"<< std::endl;
		for(size_t i=0; i<10; i++)
			std::cout << "[" << i << "] :" << data_d[i] << std::endl;

		//-------------------------------------------------------
		// Bring data to host and suffle it to avoid biases

		hydra::copy(data_d.begin(), data_d.end(),  data_h.begin());

		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(data_h.begin(), data_h.end(), g);

		hydra::copy(data_h.begin(), data_h.end(),  data_d.begin());

		std::cout<< std::endl<< "Suffled data:"<< std::endl;
		for(size_t i=0; i<10; i++)
			std::cout << "[" << i << "] :" << data_d[i] << std::endl;

		//filtering
		auto FILTER = [=]__host__ __device__(unsigned int n, double* x){
			return (x[0] > min) && (x[0] < max );
		};

		auto filter = hydra::wrap_lambda(FILTER);
		auto range  = hydra::apply_filter(data_d,  filter);

		std::cout<< std::endl<< "Filtered data:"<< std::endl;
		for(size_t i=0; i<10; i++)
			std::cout << "[" << i << "] :" << range.begin()[i] << std::endl;


		//make model and fcn
		auto fcn   = hydra::make_loglikehood_fcn(range.begin(), range.end(), model);

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
		std::cout<<"Minimum: "<< minimum_d << std::endl;

		//time
		std::cout << "-----------------------------------------"<<std::endl;
		std::cout << "| [Fit] GPU Time (ms) ="<< elapsed_d.count() <<std::endl;
		std::cout << "-----------------------------------------"<<std::endl;

		//--------------------------------------------
		//splot 2 components
		//hold weights
		hydra::multiarray<2, double, hydra::device::sys_t> sweigts_d(range.size());

		//create splot
		auto splot  = hydra::make_splot(fcn.GetPDF() );

		start_d = std::chrono::high_resolution_clock::now();
		auto covar = splot.Generate( range.begin(), range.end(), sweigts_d.begin());
		end_d = std::chrono::high_resolution_clock::now();
		elapsed_d = end_d - start_d;

		//time
		std::cout << "-----------------------------------------"<<std::endl;
		std::cout << "| [sPlot] GPU Time (ms) ="<< elapsed_d.count() <<std::endl;
		std::cout << "-----------------------------------------"<<std::endl;

		std::cout << "Covariance matrix "<< std::endl << covar<< std::endl << std::endl;
		std::cout<< std::endl << "sWeights:" << std::endl;
		for(size_t i = 0; i<10; i++)
			std::cout<<  "[" << i << "] :" <<  sweigts_d[i] << std::endl;
		std::cout<< std::endl << std::endl;

		//bring data to device
		hydra::multiarray<2, double, hydra::device::sys_t> data2_d(range.size());
		hydra::copy( range.begin() , range.end(), data2_d.begin() );

        //_______________________________
		//histograms
		size_t nbins = 100;

        hydra::DenseHistogram<1, double> Hist_Data(nbins, min, max);

        start_d = std::chrono::high_resolution_clock::now();
        Hist_Data.Fill(data2_d.begin(0), data2_d.end(0));
        end_d = std::chrono::high_resolution_clock::now();
        elapsed_d = end_d - start_d;

        //time
        std::cout << "-----------------------------------------"<<std::endl;
        std::cout << "| [Histograming data] GPU Time (ms) ="<< elapsed_d.count() <<std::endl;
        std::cout << "-----------------------------------------"<<std::endl;

        hydra::DenseHistogram<1, double> Hist_Control(nbins, min, max);

        start_d = std::chrono::high_resolution_clock::now();
        Hist_Control.Fill(data2_d.begin(1), data2_d.end(1));
        end_d = std::chrono::high_resolution_clock::now();
        elapsed_d = end_d - start_d;

        //time
        std::cout << "-----------------------------------------"<<std::endl;
        std::cout << "| [Histograming control] GPU Time (ms) ="<< elapsed_d.count() <<std::endl;
        std::cout << "-----------------------------------------"<<std::endl;

        hydra::DenseHistogram<1, double> Hist_Control_1(nbins, min, max);

        start_d = std::chrono::high_resolution_clock::now();
        Hist_Control_1.Fill(data2_d.begin(1), data2_d.end(1), sweigts_d.begin(0) );
        end_d = std::chrono::high_resolution_clock::now();
        elapsed_d = end_d - start_d;

        //time
        std::cout << "-----------------------------------------"<<std::endl;
        std::cout << "| [Histograming control 1] GPU Time (ms) ="<< elapsed_d.count() <<std::endl;
        std::cout << "-----------------------------------------"<<std::endl;

        hydra::DenseHistogram<1, double> Hist_Control_2(nbins, min, max);

        start_d = std::chrono::high_resolution_clock::now();
        Hist_Control_2.Fill(data2_d.begin(1), data2_d.end(1), sweigts_d.begin(1) );
        end_d = std::chrono::high_resolution_clock::now();
        elapsed_d = end_d - start_d;

        //time
        std::cout << "-----------------------------------------"<<std::endl;
        std::cout << "| [Histograming control 2] GPU Time (ms) ="<< elapsed_d.count() <<std::endl;
        std::cout << "-----------------------------------------"<<std::endl;





#ifdef _ROOT_AVAILABLE_

        for(size_t bin=0; bin < nbins; bin++){

        	hist_data_dicriminating_d.SetBinContent(bin+1,  Hist_Data[bin] );
        	hist_data_control_d.SetBinContent(bin+1,  Hist_Control[bin] );
        	hist_control_1_d.SetBinContent(bin+1,  Hist_Control_1[bin] );
        	hist_control_2_d.SetBinContent(bin+1,  Hist_Control_2[bin] );

        }


		//draw fitted function
		for (size_t i=0 ; i<=100 ; i++) {
			double x = hist_fit_d.GetBinCenter(i);
	        hist_fit_d.SetBinContent(i, fcn.GetPDF()(x) );
		}
		hist_fit_d.Scale(hist_data_dicriminating_d.Integral()/hist_fit_d.Integral() );


#endif //_ROOT_AVAILABLE_

	}//device end



	//host
	//------------------------
#ifdef _ROOT_AVAILABLE_

	TH1D hist_data_dicriminating_h("data_discriminating_h", "Discriminating variable [HOST]", 100, min, max);
	TH1D hist_data_control_h("data_control_h", "Control Variable [HOST]", 100, min, max);
	TH1D hist_fit_h("fit_h", "Discriminating variable [HOST]", 100, min, max);
	TH1D hist_control_1_h("control_1_h", "Control Variable: Gaussian PDF [HOST]",    100, min, max);
	TH1D hist_control_2_h("control_2_h", "Control Variable: Exponential PDF [HOST]",    100, min, max);
#endif //_ROOT_AVAILABLE_

	{
		std::cout << "=========================================="<<std::endl;
		std::cout << "|            <--- HOST --->            |"  <<std::endl;
		std::cout << "=========================================="<<std::endl;

		//------------------
	    //make model
		//numerical integral to normalize the pdfs
		hydra::GaussKronrodQuadrature<61,50, hydra::host::sys_t> GKQ61_d(min,  max);

		//convert functors to pdfs
		auto Gauss_PDF = hydra::make_pdf(gaussian  , GKQ61_d);
		auto    Exp_PDF = hydra::make_pdf(exponential, GKQ61_d);

		auto model = hydra::add_pdfs(yields, Gauss_PDF, Exp_PDF);

		model.SetExtended(1);

		//1D data containers
		hydra::multiarray<2, double, hydra::host::sys_t>   data_h(2*nentries);

		//-------------------------------------------------------
		// Generate toy data

		//first component: [Gaussian] x [Exponential]
		// gaussian
		Generator.Gauss(mean_p.GetValue()+2.5, sigma_p.GetValue()+0.5, data_h.begin(0), data_h.begin(0)+nentries);

		// exponential
		Generator.Exp(tau_p.GetValue()+1.0, data_h.begin(1),  data_h.begin(1)+nentries);

		//second component: [Exponential] -> [Gaussian]
		// gaussian
		Generator.Gauss(mean_p.GetValue()-1.0, 0.5, data_h.begin(1) + nentries, data_h.begin(1) + nentries + nentries/2);
	    Generator.Gauss(mean_p.GetValue()+4.5, 0.5, data_h.begin(1) + nentries + nentries/2, data_h.end(1));

		// exponential
		Generator.Exp(tau_p.GetValue()+5.0, data_h.begin(0)+nentries,  data_h.end(0));

		std::cout<< std::endl<< "Generated data:"<< std::endl;
		for(size_t i=0; i<10; i++)
			std::cout << "[" << i << "] :" << data_h[i] << std::endl;

		//-------------------------------------------------------
		//suffle the data

		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(data_h.begin(), data_h.end(), g);

		std::cout<< std::endl<< "Suffled data:"<< std::endl;
		for(size_t i=0; i<10; i++)
			std::cout << "[" << i << "] :" << data_h[i] << std::endl;

		//filtering
		auto FILTER = [=]__host__ __device__(unsigned int n, double* x){
			return (x[0] > min) && (x[0] < max );
		};

		auto filter = hydra::wrap_lambda(FILTER);
		auto range  = hydra::apply_filter(data_h,  filter);

		std::cout<< std::endl<< "Filtered data:"<< std::endl;
		for(size_t i=0; i<10; i++)
			std::cout << "[" << i << "] :" << range.begin()[i] << std::endl;


		//make model and fcn
		auto fcn   = hydra::make_loglikehood_fcn(range.begin(), range.end(), model);

		//-------------------------------------------------------
		//fit
		ROOT::Minuit2::MnPrint::SetLevel(3);
		hydra::Print::SetLevel(hydra::WARNING);
		//minimization strategy
		MnStrategy strategy(2);

		// create Migrad minimizer
		MnMigrad migrad_h(fcn, fcn.GetParameters().GetMnState() ,  strategy);

		std::cout<<fcn.GetParameters().GetMnState()<<std::endl;

		// ... Minimize and profile the time

		auto start_h = std::chrono::high_resolution_clock::now();
		FunctionMinimum minimum_h =  FunctionMinimum(migrad_h(std::numeric_limits<unsigned int>::max(), 5));
		auto end_h = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed_h = end_h - start_h;

		// output
		std::cout<<"Minimum: "<< minimum_h << std::endl;

		//time
		std::cout << "-----------------------------------------"<<std::endl;
		std::cout << "| [Fit] GPU Time (ms) ="<< elapsed_h.count() <<std::endl;
		std::cout << "-----------------------------------------"<<std::endl;

		//--------------------------------------------
		//splot 2 components
		//hold weights
		hydra::multiarray<2, double, hydra::host::sys_t> sweigts_h(range.size());

		//create splot
		auto splot  = hydra::make_splot(fcn.GetPDF() );

		start_h = std::chrono::high_resolution_clock::now();
		auto covar = splot.Generate( range.begin(), range.end(), sweigts_h.begin());
		end_h = std::chrono::high_resolution_clock::now();
		elapsed_h = end_h - start_h;

		//time
		std::cout << "-----------------------------------------"<<std::endl;
		std::cout << "| [sPlot] GPU Time (ms) ="<< elapsed_h.count() <<std::endl;
		std::cout << "-----------------------------------------"<<std::endl;

		std::cout << "Covariance matrix "<< std::endl << covar<< std::endl << std::endl;
		std::cout<< std::endl << "sWeights:" << std::endl;
		for(size_t i = 0; i<10; i++)
			std::cout<<  "[" << i << "] :" <<  sweigts_h[i] << std::endl;
		std::cout<< std::endl << std::endl;

		//bring data to device
		hydra::multiarray<2, double, hydra::host::sys_t>   buffer(range.size());
		hydra::copy( range.begin() , range.end(),  buffer.begin() );

#ifdef _ROOT_AVAILABLE_
		for(size_t i=0; i< buffer.size(); i++){

			hist_data_dicriminating_h.Fill(*(buffer.begin(0)+i) );
			hist_data_control_h.Fill(*(buffer.begin(1)+i) );

			hist_control_1_h.Fill(*(buffer.begin(1)+i), *(sweigts_h.begin(0)+i) );
			hist_control_2_h.Fill(*(buffer.begin(1)+i), *(sweigts_h.begin(1)+i) );

		}

		//draw fitted function
		for (size_t i=0 ; i<=100 ; i++) {
			double x = hist_fit_d.GetBinCenter(i);
	        hist_fit_h.SetBinContent(i, fcn.GetPDF()(x) );
		}
		hist_fit_h.Scale(hist_data_dicriminating_h.Integral()/hist_fit_h.Integral() );


#endif //_ROOT_AVAILABLE_

	}//host end

#ifdef _ROOT_AVAILABLE_
	TApplication *myapp=new TApplication("myapp",0,0);

	//draw histograms
	TCanvas canvas_1_d("canvas_1_d" ,"Distributions - Device", 500, 500);

	hist_data_dicriminating_d.Draw("hist");
	hist_fit_d.Draw("histsameC");
	hist_fit_d.SetLineColor(2);

	TCanvas canvas_2_d("canvas_2_d" ,"Distributions - Device", 1000, 500);
	canvas_2_d.Divide(2,1);
	canvas_2_d.cd(1);
	hist_control_1_d.Draw("hist");
	canvas_2_d.cd(2);
	hist_control_2_d.Draw("hist");

	//draw histograms
	TCanvas canvas_1_h("canvas_1_h" ,"Distributions - Host", 500, 500);

	hist_data_dicriminating_h.Draw("hist");
	hist_fit_h.Draw("histsameC");
	hist_fit_h.SetLineColor(2);

	TCanvas canvas_2_h("canvas_2_h" ,"Distributions - Host", 1000, 500);
	canvas_2_h.Divide(2,1);
	canvas_2_h.cd(1);
	hist_control_1_h.Draw("hist");
	canvas_2_h.cd(2);
	hist_control_2_h.Draw("hist");


	myapp->Run();

#endif //_ROOT_AVAILABLE_

	return 0;



}




#endif /* SPLOT_INL_ */
