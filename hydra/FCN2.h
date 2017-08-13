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
 * FCN2.h
 *
 *  Created on: 10/08/2017
 *      Author: Antonio Augusto Alves Junior
 */

#ifndef FCN2_H_
#define FCN2_H_

namespace hydra {
#include <hydra/detail/Config.h>
#include <hydra/detail/BackendPolicy.h>
#include <hydra/Types.h>

#include <hydra/detail/utility/Utility_Tuple.h>
#include <hydra/detail/Hash.h>
#include <hydra/detail/functors/LogLikelihood.h>
#include <hydra/detail/utility/Arithmetic_Tuple.h>
#include <hydra/Point.h>
#include <hydra/UserParameters.h>

#include <thrust/distance.h>

#include <Minuit2/FCNBase.h>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <utility>


namespace hydra {

template<typename T>
class FCN2;

template< template<typename ...> class Estimator, typename PDF, typename Iterator, typename ...Visitors>
class FCN2<Estimator<PDF,Iterator,Visitors...>>: public ROOT::Minuit2::FCNBase, public Visitors...{

public:

	FCN2(PDF& pdf, Iterator begin, Iterator end, Visitors&& ...visitors):
	fPDF(pdf),
	fBegin(begin),
	fEnd(end),
	fErrorDef(0.5),
	fFCNCache(std::unordered_map<size_t, GReal_t>()),
	Visitors(std::forward<Visitors>(visitors))...{
		LoadFCNParameters();
	}

	FCN2(PDF& pdf, Iterator begin, Iterator end, Visitors const& ...visitors):
	fPDF(pdf),
	fBegin(begin),
	fEnd(end),
	fErrorDef(0.5),
	fFCNCache(std::unordered_map<size_t, GReal_t>()),
	Visitors(visitors)...{
		LoadFCNParameters();
	}


	FCN2(FCN2<PDF, Estimator, Iterator,Visitors...> const& other):
	ROOT::Minuit2::FCNBase(other),
	Visitors(other)...,
	fPDF(other.GetPdf()),
	fBegin(other.GetBegin()),
	fEnd(other.GetEnd()),
	fErrorDef(other.GetErrorDef()),
	fUserParameters(other.GetParameters()),
	fFCNCache(other.GetFcnCache())
	{
		LoadFCNParameters();
	}

	FCN2<PDF, Estimator, Iterator,Visitors...>&
	operator=(FCN2<PDF, Estimator, Iterator,Visitors...> const& other){

		if( this==&other ) return this;

		ROOT::Minuit2::FCNBase::operator=(other);
		auto x = {0,(Visitors::operator=(other), 0)...};
		x={};
		fPDF   = other.GetPdf();
		fBegin = other.GetBegin();
		fEnd   = other.GetEnd();
		fErrorDef = other.GetErrorDef();
		fUserParameters = other.GetParameters();
		fFCNCache = other.GetFcnCache();

		return this;
	}

    // from Minuit2
	double ErrorDef() const{
		return fErrorDef;
	}

    void   SetErrorDef(double error){
    	fErrorDef=error;
    }

	double Up() const{
		return fErrorDef;
	}

	//this class
	GReal_t GetErrorDef() const {
		return fErrorDef;
	}

	Iterator begin() const {
		return fBegin;
	}

	Iterator end() const {
		return fEnd;
	}

	void SetBegin(Iterator begin) {
		fBegin = begin;
	}

	void SetEnd(Iterator end) {
		fEnd = end;
	}

	PDF& GetPdf() {
		return fPDF;
	}

	PDF& GetPdf() const {
			return fPDF;
	}

	const hydra::UserParameters& GetParameters() {
			return fUserParameters;
		}

	const hydra::UserParameters& GetParameters() const {
		return fUserParameters;
	}

	void SetParameters(const hydra::UserParameters& userParameters) {
		fUserParameters = userParameters;
	}

private:

	std::unordered_map<size_t, GReal_t>& GetFcnCache() const {
		return fFCNCache;
	}

	void SetFcnCache(mutable std::unordered_map<size_t, GReal_t> fcnCache) {
		fFCNCache = fcnCache;
	}

	GReal_t GetFCNValue(const std::vector<double>& parameters) const {

		size_t key = hydra::detail::hash_range(parameters.begin(),parameters.end());

		auto search = fFCNCache.find(key);

		GReal_t value = 0.0;

		if (search != fFCNCache.end() && fFCNCache.size()>0) {
			value = search->second;
		}
		else {
			value = EvalFCN(parameters);
			fFCNCache[key] = value;
		}

		return value;
	}

	GReal_t EvalFCN(const std::vector<double>& parameters) const {
		return static_cast<const estimator_type*>(this)->Eval(parameters);
	}

	void LoadFCNParameters(){
		std::vector<hydra::Parameter*> temp;
		fPDF.AddUserParameters(temp );
		fUserParameters.SetVariables( temp);
	}

    PDF& fPDF;
    Iterator fBegin;
    Iterator fEnd;
    GReal_t fErrorDef;
    hydra::UserParameters fUserParameters ;
    mutable std::unordered_map<size_t, GReal_t> fFCNCache;


};

} //namespace hydra

#endif /* FCN2_H_ */
