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
 * Divide.h
 *
 *  Created on: 10/06/2016
 *      Author: Antonio Augusto Alves Junior
 */
/**
 * \file
 * \ingroup functor
 */

#ifndef DIVIDE_H_
#define DIVIDE_H_


#include <hydra/detail/Config.h>
#include <hydra/Types.h>
#include <hydra/detail/TypeTraits.h>
#include <hydra/detail/utility/Utility_Tuple.h>
#include <hydra/detail/base_functor.h>
#include <hydra/detail/Constant.h>
#include <hydra/Parameter.h>

namespace hydra {


template<typename F1, typename F2 >
struct  Divide
{
	//tag
	typedef void hydra_functor_tag;
	typedef   std::true_type is_functor;
	typedef typename detail::divide_result<typename F1::return_type, typename F2::return_type>::type  return_type;
	typedef typename HYDRA_EXTERNAL_NS::thrust::tuple<F1, F2> functors_type;

	__host__
	Divide():
	fIndex(-1),
	fCached(0)
	{};

	__host__
	Divide(F1 const& f1, F2 const& f2):
	fIndex(-1),
	fCached(0),
	fFtorTuple(HYDRA_EXTERNAL_NS::thrust::make_tuple(f1, f2))
	{ }

	__host__ __device__
	Divide(Divide<F1,F2> const& other):
	fFtorTuple( other.GetFunctors() ),
	fIndex( other.GetIndex() ),
	fCached( other.IsCached() )
	{ };

	__host__ __device__
	Divide<F1,F2>& operator=(Divide<F1,F2> const& other)
	{
		this->fFtorTuple = other.GetFunctors() ;
		this->fIndex = other.GetIndex() ;
		this->fCached = other.IsCached() ;
		return *this;
	}

	__host__ inline
	void AddUserParameters(std::vector<hydra::Parameter*>& user_parameters )
	{
		detail::add_parameters_in_tuple(user_parameters, fFtorTuple );
	}

	__host__ inline
	void SetParameters(const std::vector<double>& parameters){

		detail::set_functors_in_tuple(fFtorTuple, parameters);
	}

	__host__ inline
	void PrintRegisteredParameters()
	{
		HYDRA_CALLER ;
		HYDRA_MSG << "Registered parameters begin:\n" << HYDRA_ENDL;
		detail::print_parameters_in_tuple(fFtorTuple);
		HYDRA_MSG <<"Registered parameters end.\n" << HYDRA_ENDL;
		return;
	}

	__host__ __device__ inline
	functors_type GetFunctors() const {return this->fFtorTuple;}

	__host__ __device__ inline
	int GetIndex() const { return this->fIndex; }

	__host__ __device__ inline
	void SetIndex(int index) {this->fIndex = index;}

	__host__ __device__ inline
	bool IsCached() const
	{ return this->fCached;}

	__host__ __device__ inline
	void SetCached(bool cached=true)
	{ this->fCached = cached; }


	template<typename T1>
	__host__ __device__ inline
	return_type operator()(T1& t )
	{
		return HYDRA_EXTERNAL_NS::thrust::get<0>(fFtorTuple)(t)/HYDRA_EXTERNAL_NS::thrust::get<1>(fFtorTuple)(t);
	}

	template<typename T1, typename T2>
	__host__ __device__  inline
	return_type operator()( T1& t, T2& cache)
	{
		return fCached ? detail::extract<return_type,T2>(fIndex, std::forward<T2&>(cache)):\
				HYDRA_EXTERNAL_NS::thrust::get<0>(fFtorTuple)(t,cache)/HYDRA_EXTERNAL_NS::thrust::get<1>(fFtorTuple)(t,cache);
	}

private:
	functors_type fFtorTuple;
	int  fIndex;
	bool fCached;

};

// devide: / operator two functors
template <typename T1, typename T2, typename=typename std::enable_if< T1::is_functor::value && T2::is_functor::value> >
__host__  inline
Divide<T1,T2>
operator/(T1 const& F1, T2 const& F2)
{
	return  Divide<T1, T2>(F1, F2);
}

template <typename T1, typename T2,
typename=typename std::enable_if< (std::is_convertible<T1, double>::value ||\
		std::is_constructible<HYDRA_EXTERNAL_NS::thrust::complex<double>,T1>::value) && T2::is_functor::value>::type >
__host__  inline
Divide<Constant<T1>, T2>
operator/(T1 const cte, T2 const& F2){ return  Constant<T1>(cte)/F2; }


template <typename T1, typename T2,
typename=typename std::enable_if< (std::is_convertible<T1, double>::value ||\
		std::is_constructible<HYDRA_EXTERNAL_NS::thrust::complex<double>,T1>::value) && T2::is_functor::value>::type >
__host__  inline
Divide<T2,Constant<T1> >
operator/(T2 const& F2, T1 const cte ){	return  F2/Constant<T1>(cte); }


// Convenience function
template < typename T1, typename T2, typename=typename std::enable_if< T1::is_functor::value && T2::is_functor::value>::type >
__host__ inline
Divide<T1,T2>
divide(T1 const& F1, T1 const& F2)
{
	return  Divide<T1,T2>(F1, F2);
}


}


#endif /* DIVIDE_H_ */
