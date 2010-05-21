/*****************************************************************************
*
* ALPS Project: Algorithms and Libraries for Physics Simulations
*
* ALPS Libraries
*
* Copyright (C) 1994-2010 by Matthias Troyer <troyer@comp-phys.org>,
*                            Beat Ammon <beat.ammon@bluewin.ch>,
*                            Andreas Laeuchli <laeuchli@comp-phys.org>,
*                            Synge Todo <wistaria@comp-phys.org>,
*                            Andreas Lange <alange@phys.ethz.ch>,
*                            Ping Nang Ma <pingnang@itp.phys.ethz.ch>
*                            Lukas Gamper <gamperl@gmail.com>
*
* This software is part of the ALPS libraries, published under the ALPS
* Library License; you can use, redistribute it and/or modify it under
* the terms of the license, either version 1 or (at your option) any later
* version.
* 
* You should have received a copy of the ALPS Library License along with
* the ALPS Libraries; see the file LICENSE.txt. If not, the license is also
* available from http://alps.comp-phys.org/.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
* SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
* FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
* DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

/* $Id: mcdata.hpp 3545 2010-03-22 10:00:00Z gamperl $ */

#ifndef ALPS_ALEA_MCDATA_HPP
#define ALPS_ALEA_MCDATA_HPP

#include <alps/config.h>
#include <alps/alea/nan.h>
#include <alps/parser/parser.h>
#include <alps/utility/resize.hpp>
#include <alps/numeric/functional.hpp>
#include <alps/alea/simpleobservable.h>
#include <alps/utility/numeric_cast.hpp>
#include <alps/utility/set_zero.hpp>
#include <alps/numeric/outer_product.hpp>
#include <alps/type_traits/param_type.hpp>
#include <alps/type_traits/average_type.hpp>
#include <alps/numeric/vector_functions.hpp>
#include <alps/numeric/valarray_functions.hpp>
#include <alps/type_traits/covariance_type.hpp>
#include <alps/type_traits/change_value_type.hpp>
#include <alps/numeric/vector_valarray_conversion.hpp>

#include <boost/config.hpp>
#include <boost/functional.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/optional/optional.hpp>
#include <boost/serialization/vector.hpp>

#include <vector>
#include <numeric>
#include <iostream>

namespace alps { 
    namespace alea {
        template <typename T> class mcdata {
            public:
                template <typename X> friend class mcdata;
                typedef T value_type;
                typedef typename alps::element_type<T>::type element_type;
                typedef typename change_value_type<T,double>::type time_type;
                typedef std::size_t size_type;
                typedef double count_type;
                typedef typename average_type<T>::type result_type;
                typedef typename change_value_type<T,int>::type convergence_type;
                typedef typename covariance_type<T>::type covariance_type;
                mcdata()
                    :  count_(0)
                    , mean_()
                    , error_()
                    , binsize_(0)
                    , max_bin_number_(0)
                    , data_is_analyzed_(true)
                    , jacknife_bins_valid_(true)
                    , cannot_rebin_(false)
                {}
                template <typename X> mcdata(mcdata<X> const & rhs)
                  : count_(rhs.count_)
                  , mean_(rhs.mean_)
                  , error_(rhs.error_)
                  , binsize_(rhs.binsize_)
                  , max_bin_number_(rhs.max_bin_number_)
                  , data_is_analyzed_(rhs.data_is_analyzed_)
                  , jacknife_bins_valid_(rhs.jacknife_bins_valid_)
                  , cannot_rebin_(rhs.cannot_rebin_)
                  , variance_opt_(rhs.variance_opt_)
                  , tau_opt_(rhs.tau_opt_)
                  , values_(rhs.values_)
                  , jack_(rhs.jack_)
                {}
                template <typename X, typename S> mcdata(mcdata<X> const & rhs, S s)
                  : count_(rhs.count_)
                  , binsize_(rhs.binsize_)
                  , max_bin_number_(rhs.max_bin_number_)
                  , data_is_analyzed_(rhs.data_is_analyzed_)
                  , jacknife_bins_valid_(rhs.jacknife_bins_valid_)
                  , cannot_rebin_(rhs.cannot_rebin_)
                {
                    mean_ = slice_value(rhs.mean_, s);
                    error_ = slice_value(rhs.error_, s);
                    if (rhs.variance_opt_) 
                        variance_opt_ = slice_value(*(rhs.variance_opt_), s);
                    if (rhs.tau_opt_)
                        tau_opt_ = slice_value(*(rhs.tau_opt_), s);
                    values_.reserve(rhs.values_.size());
                    std::transform(rhs.values_.begin(), rhs.values_.end(), std::back_inserter(values_), boost::bind2nd(slice_it<X>(), s));
                    if (rhs.jacknife_bins_valid_)
                        std::transform(rhs.jack_.begin(), rhs.jack_.end(), std::back_inserter(jack_), boost::bind2nd(slice_it<X>(), s));
                }
                template <typename X> mcdata(AbstractSimpleObservable<X> const & obs)
                    : count_(obs.count())
                    , binsize_(obs.bin_size())
                    , max_bin_number_(obs.max_bin_number())
                    , data_is_analyzed_(false)
                    , jacknife_bins_valid_(false)
                    , cannot_rebin_(false)
                {
                    using boost::numeric::operators::operator/;
                    if (count()) {
                        mean_ = replace_valarray_by_vector(obs.mean());
                        error_ = replace_valarray_by_vector(obs.error());
                        if (obs.has_variance())
                            variance_opt_ = replace_valarray_by_vector(obs.variance());
                        if (obs.has_tau())
                            tau_opt_ = replace_valarray_by_vector(obs.tau());
                        for (std::size_t i = 0; i < obs.bin_number(); ++i)
                            values_.push_back(obs.bin_value(i) / double(binsize_));
                    }
                }
                mcdata(int64_t count, value_type const & mean, value_type const & error, value_type const & variance, uint64_t binsize, std::vector<value_type> const & values)
                    : count_(count)
                    , mean_(mean)
                    , error_(error)
                    , variance_opt_(variance)
                    , binsize_(binsize)
                    , max_bin_number_(0)
                    , data_is_analyzed_(false)
                    , jacknife_bins_valid_(false)
                    , cannot_rebin_(false)
                    , values_(values)
                {}
                bool can_rebin() const { return !cannot_rebin_;}
                bool jackknife_valid() const { return jacknife_bins_valid_;}
                void swap(mcdata<T> & rhs) {
                    std::swap(count_, rhs.count_);
                    std::swap(mean_, rhs.mean_);
                    std::swap(error_, rhs.error_);
                    std::swap(binsize_, rhs.binsize_);
                    std::swap(rhs.max_bin_number_, rhs.max_bin_number_);
                    std::swap(data_is_analyzed_, rhs.data_is_analyzed_);
                    std::swap(jacknife_bins_valid_, rhs.jacknife_bins_valid_);
                    std::swap(cannot_rebin_, rhs.cannot_rebin_);
                    std::swap(variance_opt_, rhs.variance_opt_);
                    std::swap(tau_opt_, rhs.tau_opt_);
                    std::swap(values_, rhs.values_);
                    std::swap(jack_, rhs.jack_);
                }
                inline uint64_t count() const { 
                    return count_;
                }
                inline uint64_t bin_size() const { 
                    return binsize_;
                }
                inline uint64_t max_bin_number() const {
                    return max_bin_number_;
                }
                inline std::size_t bin_number() const { 
                    return values_.size(); 
                }
                inline std::vector<value_type> const & bins() const { 
                    return values_;  
                }
                inline result_type const & mean() const {
                    analyze();
                    return mean_;
                }
                inline result_type const & error() const {
                    analyze();
                    return error_;
                }
                inline result_type const & variance() const {
                    analyze();  
                    if (!variance_opt_)
                        boost::throw_exception(std::logic_error("observable does not have variance"));
                    return *variance_opt_;
                };
                inline time_type const & tau() const {
                    analyze();  
                    if (!tau_opt_)
                        boost::throw_exception(std::logic_error("observable does not have autocorrelation information"));
                    return *tau_opt_;
                }
                covariance_type covariance(mcdata<T> const & obs) const {
                    fill_jack();
                    obs.fill_jack();
                    if (jack_.size() && obs.jack_.size()) {
                        result_type unbiased_mean1_;
                        result_type unbiased_mean2_;
                        resize_same_as(unbiased_mean1_, jack_[0]);
                        resize_same_as(unbiased_mean2_, obs.jack_[0]);
                        if (jack_.size() != obs.jack_.size()) 
                            boost::throw_exception(std::runtime_error("unequal number of bins in calculation of covariance matrix"));
                        unbiased_mean1_ = 0;
                        unbiased_mean2_ = 0;
                        unbiased_mean1_ = std::accumulate(jack_.begin()+1, jack_.end(), unbiased_mean1_);
                        unbiased_mean2_ = std::accumulate(obs.jack_.begin()+1, obs.jack_.end(), unbiased_mean2_);
                        unbiased_mean1_ /= count_type(bin_number());
                        unbiased_mean2_ /= count_type(obs.bin_number());
                        using alps::numeric::outer_product;
                        covariance_type cov = outer_product(jack_[1],obs.jack_[1]);
                        for (uint32_t i = 1; i < bin_number(); ++i)
                            cov += outer_product(jack_[i+1],obs.jack_[i+1]);
                        cov /= count_type(bin_number());
                        cov -= outer_product(unbiased_mean1_, unbiased_mean2_);
                        cov *= count_type(bin_number() - 1);
                        return cov;
                    } else {
                        boost::throw_exception(std::runtime_error ("no binning information available for calculation of covariances"));
                        return covariance_type();
                    }
                }
                inline void set_bin_size(uint64_t binsize) {
                    collect_bins(( binsize - 1 ) / binsize_ + 1 );
                    binsize_ = binsize;
                }
                inline void set_bin_number(uint64_t bin_number) {
                    collect_bins(( values_.size() - 1 ) / bin_number + 1 );
                }
                void output(std::ostream& out, boost::mpl::true_) const {
                    if(count() == 0)
                        out << "no measurements" << std::endl;
                    else {
                        out << std::setprecision(6) << alps::numeric::round<2>(mean()) << " +/- "
                            << std::setprecision(3) << alps::numeric::round<2>(error());
                        if(tau_opt_)
                            out << std::setprecision(3) <<  "; tau = " << (alps::numeric::is_nonzero<2>(error()) ? *tau_opt_ : 0);
                        out << std::setprecision(6) << std::endl;
                    }
                }
                void output_vector(std::ostream& out, boost::mpl::false_) const {
                    if(count() == 0)
                        out << "no measurements" << std::endl;
                    else
                        for (typename alps::slice_index<result_type>::type it= slices(mean_).first; it != slices(mean_).second; ++it) {
                            out << "Entry[" << slice_name(mean_, it) << "]: "
                                << alps::numeric::round<2>(slice_value(mean_, it)) << " +/- "
                                << alps::numeric::round<2>(slice_value(error_, it));
                            if(tau_opt_)
                                out << "; tau = " << (alps::numeric::is_nonzero<2>(slice_value(error_, it)) ? slice_value(*tau_opt_, it) : 0);
                            out << std::endl;
                        }
                }
                void serialize(hdf5::iarchive & ar) {
                    using boost::numeric::operators::operator/;
                    data_is_analyzed_ = true;
                    ar
                        >> make_pvp("count", count_)
                        >> make_pvp("mean/value", mean_)
                        >> make_pvp("mean/error", error_)
                    ;
                    if (ar.is_attribute("@nonlinearoperations"))
                        ar >> make_pvp("@nonlinearoperations", cannot_rebin_);
                    else if (ar.is_attribute("@cannotrebin"))
                        ar >> make_pvp("@cannotrebin", cannot_rebin_);
                    else
                        cannot_rebin_ = false;
                    if (ar.is_data("variance/value")) {
                        variance_opt_.reset(result_type());
                        ar
                            >> make_pvp("variance/value", *variance_opt_)
                        ;
                    }
                    else
                        variance_opt_ = boost::none_t();
                    if (ar.is_data("tau/value")) {
                        tau_opt_.reset(time_type());
                        ar
                            >> make_pvp("tau/value", *tau_opt_)
                        ;
                    }
                    if (ar.is_data("timeseries/data")) {
                      ar
                          >> make_pvp("timeseries/data", values_)
                          >> make_pvp("timeseries/data/@maxbinnum", max_bin_number_)
                      ;
                      if (ar.is_attribute("timeseries/data/@binsize"))
                          ar
                              >> make_pvp("timeseries/data/@binsize", binsize_)
                          ;
                      else
                          binsize_ = count_ / values_.size();
                      if (!ar.is_attribute("@cannotrebin"))
                          values_ = values_ / double(binsize_);
                    }
                    if ((jacknife_bins_valid_ = ar.is_data("jacknife/data")))
                        ar
                            >> make_pvp("jacknife/data", jack_)
                        ;
                }
                void serialize(hdf5::oarchive & ar) const {
                    analyze();
                    ar
                        << make_pvp("count", count_)
                        << make_pvp("@cannotrebin", cannot_rebin_)
                        << make_pvp("mean/value", mean_)
                        << make_pvp("mean/error", error_)
                    ;
                    if (variance_opt_)
                        ar
                            << make_pvp("variance/value", *variance_opt_)
                        ;
                    if (tau_opt_)
                        ar
                            << make_pvp("tau/value", *tau_opt_)
                        ;
                    ar
                        << make_pvp("timeseries/data", values_)
                        << make_pvp("timeseries/data/@binsize", binsize_)
                        << make_pvp("timeseries/data/@maxbinnum", max_bin_number_)
                        << make_pvp("timeseries/data/@binningtype", "linear")
                    ;
                    if (jacknife_bins_valid_)
                        ar
                            << make_pvp("jacknife/data", jack_)
                            << make_pvp("jacknife/data/@binningtype", "linear")
                        ;
                }
                void save(std::string const & filename, std::string const & path) const {
                    hdf5::oarchive ar(filename);
                    ar << make_pvp(path, *this);
                }
                void load(std::string const & filename, std::string const & path) {
                    hdf5::iarchive ar(filename);
                    ar >> make_pvp(path, *this);
                }
                boost::tuple<int64_t, value_type, value_type, value_type, value_type> get_reduceable_data(std::size_t numbins) {
                    using alps::numeric::sq;
                    T binvalue = 0.;
                    for (std::size_t i = 0; i < numbins; ++i)
                        binvalue += values_[i];
                    return boost::tuple<int64_t, value_type, value_type, value_type, value_type>(
                        count_, 
                        mean_ * double(count_), 
                        error_ * error_ * sq(double(count_)), 
                        *variance_opt_ * double(count_),
                        binvalue / count_type(numbins)
                    );
                }
                mcdata<T> & operator<<(mcdata<T> const & rhs) {
                    using std::sqrt;
                    using alps::numeric::sq;
                    using alps::numeric::sqrt;
                    if (rhs.count()) {
                        if (count()) {
                            jacknife_bins_valid_ = false;
                            data_is_analyzed_ = data_is_analyzed_ && rhs.data_is_analyzed_;
                            cannot_rebin_ = cannot_rebin_ && rhs.cannot_rebin_;
                            mean_ *= double(count_);
                            mean_ += double(rhs.count_) * rhs.mean_;
                            mean_ /= double(count_ + rhs.count_);
                            result_type tmp = error_, tmp2 = rhs.error_;
                            tmp *= error_ * sq(double(count_));
                            tmp2 *= rhs.error_ * sq(double(rhs.count_));
                            error_ = sqrt(tmp + tmp2);
                            error_ /= double(count_ + rhs.count_);
                            if(variance_opt_ && rhs.variance_opt_) {
                                *variance_opt_ *= double(count_);
                                *variance_opt_ += double(rhs.count_) * *rhs.variance_opt_;
                                *variance_opt_ /= double(count_ + rhs.count_);
                            } else
                                variance_opt_ = boost::none_t();
                            if(tau_opt_ && rhs.tau_opt_) {
                                *tau_opt_ *= double(count_);
                                *tau_opt_ += double(rhs.count_) * *rhs.tau_opt_;
                                *tau_opt_ /= double(count_ + rhs.count_);
                            } else
                                tau_opt_ = boost::none_t();
                            count_ += rhs.count();
                            if (binsize_ <= rhs.bin_size()) {
                                if (binsize_ < rhs.bin_size())
                                    set_bin_size(rhs.bin_size());
                                std::copy(rhs.values_.begin(), rhs.values_.end(), std::back_inserter(values_));
                            } else {
                                mcdata<T> tmp(rhs);
                                tmp.set_bin_size(binsize_);
                                std::copy(tmp.values_.begin(), tmp.values_.end(), std::back_inserter(values_));
                            }
                            if (max_bin_number_ && max_bin_number_ < bin_number())
                                set_bin_number(max_bin_number_);
                        } else
                            *this = rhs;
                    }
                    return *this;
                }
                mcdata<T> & operator=(mcdata<T> rhs) {
                    rhs.swap(*this);
                    return *this;
                }
                template <typename S> element_type slice(S s) const {
                    return element_type(*this, s);
                }
                // wtf? check why!!! (after 2.0b1)
                template <typename X> bool operator==(mcdata<X> const & rhs) {
                    return count_ == rhs.count_
                        && binsize_ == rhs.binsize_
                        && max_bin_number_ == rhs.max_bin_number_
                        && mean_ == rhs.mean_
                        && error_ == rhs.error_
                        && variance_opt_ == rhs.variance_opt_
                        && tau_opt_ == rhs.tau_opt_
                        && values_ == rhs.values_
                    ;
                }
                mcdata<T> & operator+=(mcdata<T> const & rhs) {
                    using std::sqrt;
                    using alps::numeric::sq;
                    using alps::numeric::sqrt;
                    using boost::numeric::operators::operator+;
                    transform(rhs, alps::numeric::plus<T>(), sqrt(sq(error_) + sq(rhs.error_)));
                    return *this;
                }
                mcdata<T> & operator-=(mcdata<T> const & rhs) {
                    using std::sqrt;
                    using alps::numeric::sq;
                    using alps::numeric::sqrt;
                    using boost::numeric::operators::operator+;
                    using boost::numeric::operators::operator-;
                    transform(rhs, alps::numeric::minus<T>(), sqrt(sq(error_) + sq(rhs.error_)));
                    return *this;
                }
                template <typename X> mcdata<T> & operator*=(mcdata<X> const & rhs) {
                    using std::sqrt;
                    using alps::numeric::sq;
                    using alps::numeric::sqrt;
                    using boost::numeric::operators::operator+;
                    using boost::numeric::operators::operator*;
                    transform(rhs, alps::numeric::multiplies<T>(), sqrt(sq(rhs.mean_) * sq(error_) + sq(mean_) * sq(rhs.error_)));
                    return *this;
                }
                template <typename X> mcdata<T> & operator/=(mcdata<X> const & rhs) {
                    using std::sqrt;
                    using alps::numeric::sq;
                    using alps::numeric::sqrt;
                    using boost::numeric::operators::operator+;
                    using boost::numeric::operators::operator*;
                    using boost::numeric::operators::operator/;
                    transform(rhs, alps::numeric::divides<T>(), sqrt(sq(rhs.mean_) * sq(error_) + sq(mean_) * sq(rhs.error_)) / sq(rhs.mean_));
                    return *this;
                }
                template <typename X> mcdata<T> & operator+=(X const & rhs) {
                    using boost::numeric::operators::operator+;
                    transform_linear(boost::lambda::bind(alps::numeric::plus<X>(), boost::lambda::_1, rhs), error_, variance_opt_);
                    return *this;
                }
                template <typename X> mcdata<T> & operator-=(X const & rhs) {
                    using boost::numeric::operators::operator-;
                    transform_linear(boost::lambda::bind(alps::numeric::minus<X>(), boost::lambda::_1, rhs), error_, variance_opt_);
                    return *this;
                }
                template <typename X> mcdata<T> & operator*=(X const & rhs) {
                    using std::abs;
                    using alps::numeric::abs;
                    using boost::numeric::operators::operator*;
                    transform_linear(boost::lambda::bind(alps::numeric::multiplies<X>(), boost::lambda::_1, rhs), abs(error_ * rhs), variance_opt_ ? boost::optional<result_type>(*variance_opt_ * rhs * rhs) : boost::none_t());
                    return *this;
                }
                template <typename X> mcdata<T> & operator/=(X const & rhs) {
                    using std::abs;
                    using alps::numeric::abs;
                    using boost::numeric::operators::operator/;
                    using boost::numeric::operators::operator*;
                    transform_linear(boost::lambda::bind(alps::numeric::divides<X>(), boost::lambda::_1, rhs), abs(error_ / rhs), variance_opt_ ? boost::optional<result_type>(*variance_opt_ / ( rhs * rhs )) : boost::none_t());
                    return (*this);
                }
                mcdata<T> & operator+() {
                    return *this;
                }
                mcdata<T> & operator-() {
                    // TODO: this is ugly
                    T zero;
                    resize_same_as(zero, error_);
                    transform_linear(boost::lambda::bind(alps::numeric::minus<T>(), zero, boost::lambda::_1), error_, variance_opt_);
                    return *this;
                }
                template <typename X> void subtract_from(X const & x) {
                    using boost::numeric::operators::operator-;
                    transform_linear(boost::lambda::bind(alps::numeric::minus<X>(), x, boost::lambda::_1), error_, variance_opt_);
                }
                template <typename X> void divide(X const & x) {
                    using boost::numeric::operators::operator*;
                    using boost::numeric::operators::operator/;
                    error_ = x * error_ / mean_ / mean_;
                    cannot_rebin_ = true;
                    mean_ = x / mean_;
                    std::transform(values_.begin(), values_.end(), values_.begin(), boost::lambda::bind(alps::numeric::divides<X>(), x * bin_size() * bin_size(), boost::lambda::_1));
                    fill_jack();
                    std::transform(jack_.begin(), jack_.end(), jack_.begin(), boost::lambda::bind(alps::numeric::divides<X>(), x, boost::lambda::_1));
                }
                template <typename OP> void transform_linear(OP op, value_type const & error, boost::optional<result_type> variance_opt = boost::none_t()) {
                    if (count() == 0)
                        boost::throw_exception(std::runtime_error("the observable needs measurements"));
                    mean_ = op(mean_);
                    error_ = error;
                    variance_opt_ = variance_opt_;
                    std::transform(values_.begin(), values_.end(), values_.begin(), op);
                    if (jacknife_bins_valid_)
                        std::transform(jack_.begin(), jack_.end(), jack_.begin(), op);
                }
                template <typename OP> void transform(OP op, value_type const & error, boost::optional<result_type> variance_opt = boost::none_t()) {
                    if (count() == 0)
                        boost::throw_exception(std::runtime_error("the observable needs measurements"));
                    data_is_analyzed_ = false;
                    if (!jacknife_bins_valid_)
                      fill_jack();
                    cannot_rebin_ = true;
                    mean_ = op(mean_);
                    error_ = error;
                    variance_opt_ = variance_opt_;
                    if (!variance_opt_)
                        tau_opt_ = boost::none_t();
                    std::transform(values_.begin(), values_.end(), values_.begin(), op);
                    if (jacknife_bins_valid_)
                       std::transform(jack_.begin(), jack_.end(), jack_.begin(), op);
                }
                template <typename X, typename OP> void transform(mcdata<X> const & rhs, OP op, value_type const & error, boost::optional<result_type> variance_opt = boost::none_t()) {
                    if (count() == 0 || rhs.count() == 0)
                        boost::throw_exception(std::runtime_error("both observables need measurements"));
                    if (!jacknife_bins_valid_)
                      fill_jack();
                    if (!rhs.jacknife_bins_valid_)
                      rhs.fill_jack();
                    data_is_analyzed_= false;
                    cannot_rebin_ = true;
                    mean_ = op(mean_, rhs.mean_);
                    error_ = error;
                    variance_opt_ = variance_opt_;
                    if (!variance_opt_)
                        tau_opt_ = boost::none_t();
                    std::transform(values_.begin(), values_.end(), rhs.values_.begin(), values_.begin(), op);
                    if (rhs.jacknife_bins_valid_ && jacknife_bins_valid_)
                        std::transform(jack_.begin(), jack_.end(), rhs.jack_.begin(), jack_.begin(), op);
                }
            private:
                T const & replace_valarray_by_vector(T const & value) {
                    return value;
                }
                template <typename X> std::vector<X> replace_valarray_by_vector(std::valarray<X> const & value) {
                    std::vector<X> res;
                    std::copy(&value[0], &value[0] + value.size(), std::back_inserter(res));
                    return res;
                }
                void collect_bins(uint64_t howmany) {
                    if (cannot_rebin_)
                        boost::throw_exception(std::runtime_error("cannot change bins after nonlinear operations"));
                    if (values_.empty() || howmany <= 1) 
                        return;
                    uint64_t newbins = values_.size() / howmany;
                    for (uint64_t i = 0; i < newbins; ++i) {
                        values_[i] = values_[howmany * i];
                        for (uint64_t j = 1; j < howmany; ++j)
                            values_[i] += values_[howmany * i + j];
                        values_[i]/=count_type(howmany);
                    }
                    values_.resize(newbins);
                    binsize_ *= howmany;
                    jacknife_bins_valid_ = false;
                    data_is_analyzed_ = false;
                }
                void fill_jack() const {
                    using boost::numeric::operators::operator-;
                    using boost::numeric::operators::operator+;
                    using boost::numeric::operators::operator*;
                    using boost::numeric::operators::operator/;
                    // build jackknife data structure
                    if (bin_number() && !jacknife_bins_valid_) {
                        if (cannot_rebin_)
                          boost::throw_exception(std::runtime_error("Cannot rebuild jackknife data structure after nonlinear operations"));
                        jack_.clear();
                        jack_.resize(bin_number() + 1);
                        // Order-N initialization of jackknife data structure
                        resize_same_as(jack_[0], values_[0]);
                        set_zero(jack_[0]);
                        for(std::size_t j = 0; j < bin_number(); ++j) // to this point, jack_[0] = \sum_{j} values_[j] 
                            jack_[0] = jack_[0] + alps::numeric_cast<result_type>(values_[j]);
                        double sum2 = 0.;
                        for(std::size_t i = 0; i < bin_number(); ++i) {// to this point, jack_[i+1] = \sum_{j != i} values_[j]  
                            jack_[i+1] = jack_[0] - alps::numeric_cast<result_type>(values_[i]);
                            double dx = (slice_value(values_[i], 0) - slice_value(jack_[0] / count_type(bin_number()),0));
                            sum2 +=  dx*dx;
                        }
                        //  Next, we want the following:
                        //    a)  jack_[0]   =  <x>
                        //    b)  jack_[i+1] =  <x_i>_{jacknife}
                        jack_[0] = jack_[0] / count_type(bin_number()); // up to this point, jack_[0] is the jacknife mean...
                        for (uint64_t j = 0; j < bin_number(); ++j)  
                            jack_[j+1] = jack_[j+1] / count_type(bin_number() - 1);
                    }
                    jacknife_bins_valid_ = true;
                }
                void analyze() const {
                    using std::sqrt;
                    using alps::numeric::sqrt;
                    using alps::numeric::operator*;
                    using boost::numeric::operators::operator/;
                    using boost::numeric::operators::operator+;
                    using boost::numeric::operators::operator-;
                    if (count() == 0)
                        boost::throw_exception(NoMeasurementsError());
                    if (data_is_analyzed_) 
                        return;
                    if (bin_number()) {
                        count_ = bin_size() * bin_number();
                        fill_jack();
                        if (jack_.size()) {
                            result_type unbiased_mean_;
                            resize_same_as(error_, jack_[0]);
                            resize_same_as(unbiased_mean_, jack_[0]);
                            set_zero(unbiased_mean_);
                            set_zero(error_);
                            for (typename std::vector<result_type>::const_iterator it = jack_.begin() + 1; it != jack_.end(); ++it)
                                unbiased_mean_ = unbiased_mean_ + *it / count_type(bin_number());
                            mean_ = jack_[0] - (unbiased_mean_ - jack_[0]) * (count_type(bin_number() - 1));
                            for (uint64_t i = 0; i < bin_number(); ++i)
                                error_ = error_ + (jack_[i + 1] - unbiased_mean_) * (jack_[i+1] - unbiased_mean_);
                            error_ = sqrt(error_ / count_type(bin_number()) *  count_type(bin_number() - 1));
                        }
                        variance_opt_ = boost::none_t();
                        tau_opt_ = boost::none_t();
                    }
                    data_is_analyzed_ = true;
                }
                friend class boost::serialization::access;
                template <typename Archive> void serialize(Archive & ar, const unsigned int version) {
                    ar & count_;
                    ar & binsize_;
                    ar & max_bin_number_;
                    ar & data_is_analyzed_;
                    ar & jacknife_bins_valid_;
                    ar & cannot_rebin_;
                    ar & mean_;
                    ar & error_;
                    ar & variance_opt_;
                    ar & tau_opt_;
                    ar & values_;
                    ar & jack_;
                }
                mutable uint64_t count_;
                mutable uint64_t binsize_;
                mutable uint64_t max_bin_number_;
                mutable bool data_is_analyzed_;
                mutable bool jacknife_bins_valid_;
                mutable bool cannot_rebin_;
                mutable result_type mean_;
                mutable result_type error_;
                mutable boost::optional<result_type> variance_opt_;
                mutable boost::optional<time_type> tau_opt_;
                mutable std::vector<value_type> values_;
                mutable std::vector<result_type> jack_;
        };
        template <typename T> inline std::ostream& operator<<(std::ostream & out, mcdata<T> const & obs) {
            obs.output(out, typename boost::is_scalar<T>::type());
            return out;
        }
        #define ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(OPERATOR_NAME, OPERATOR_ASSIGN)                                                                       \
            template <typename T> inline mcdata<T> OPERATOR_NAME(mcdata<T> arg1, mcdata<T> const & arg2) {                                                 \
                return arg1 OPERATOR_ASSIGN arg2;                                                                                                          \
            }                                                                                                                                              \
            template <typename T> inline mcdata<T> OPERATOR_NAME(mcdata<T> arg1, T const & arg2) {                                                         \
                return arg1 OPERATOR_ASSIGN arg2;                                                                                                          \
            }
        ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(operator+,+=)
        ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(operator-,-=)
        ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(operator*,*=)
        ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(operator/,/=)
        #undef ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION
        template <typename T> inline mcdata<T> operator+(T const & arg1, mcdata<T> arg2) {
            return arg2 += arg1;
        }
        template <typename T> inline mcdata<T> operator-(T const & arg1, mcdata<T> arg2) {
            arg2.subtract_from(arg1);
            return arg2;
        }
        template <typename T> inline mcdata<T> operator*(T const & arg1, mcdata<T> arg2) {
            return arg2 *= arg1;
        }
        template <typename T>  inline mcdata<T> operator/(T const & arg1, mcdata<T> arg2) {
            arg2.divide(arg1);
            return arg2;
        }
        #define ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(OPERATOR_NAME, OPERATOR_ASSIGN)                                                                       \
            template <typename T> inline mcdata<std::vector<T> > OPERATOR_NAME(                                                                            \
                typename mcdata<std::vector<T> >::element_type const & elem, mcdata<std::vector<T> > const & arg2                                          \
            ) {                                                                                                                                            \
                std::vector<T> arg1(arg2.mean().size(), elem);                                                                                             \
                return arg1 OPERATOR_ASSIGN arg2;                                                                                                          \
            }                                                                                                                                              \
            template <typename T> inline mcdata<std::vector<T> > OPERATOR_NAME(                                                                            \
                mcdata<std::vector<T> > const & arg1, typename mcdata<std::vector<T> >::element_type const & elem                                          \
            ) {                                                                                                                                            \
                std::vector<T> arg2(arg1.mean().size(), elem);                                                                                             \
                return arg1 OPERATOR_ASSIGN arg2;                                                                                                          \
            }                                                                                                                                              \
            template <typename T> inline std::vector<mcdata<T> > OPERATOR_NAME(                                                                            \
                typename std::vector<mcdata<T> > arg1, std::vector<mcdata<T> > const & arg2                                                                \
            ) {                                                                                                                                            \
                std::transform(                                                                                                                            \
                    arg1.begin(), arg1.end(), arg2.begin(), arg1.begin(), static_cast<mcdata<T> (*)(mcdata<T>, mcdata<T> const &)>(&OPERATOR_NAME)         \
                );                                                                                                                                         \
                return arg1;                                                                                                                               \
            }                                                                                                                                              \
            template <typename T> inline std::vector<mcdata<T> > OPERATOR_NAME(                                                                            \
                typename std::vector<mcdata<T> > arg1, std::vector<T> const & arg2                                                                         \
            ) {                                                                                                                                            \
                std::transform(                                                                                                                            \
                    arg1.begin(), arg1.end(), arg2.begin(), arg1.begin(), static_cast<mcdata<T> (*)(mcdata<T>, T const &)>(&OPERATOR_NAME)                 \
                );                                                                                                                                         \
                return arg1;                                                                                                                               \
            }                                                                                                                                              \
            template <typename T> inline std::vector<mcdata<T> > OPERATOR_NAME(                                                                            \
                typename std::vector<T> const & arg1, std::vector<mcdata<T> > arg2                                                                         \
            ) {                                                                                                                                            \
                return arg2 OPERATOR_ASSIGN arg1;                                                                                                          \
            }                                                                                                                                              \
            template <typename T> inline std::vector<mcdata<T> > OPERATOR_NAME(                                                                            \
                typename std::vector<mcdata<T> > arg1, typename mcdata<T>::element_type const & elem                                                       \
            ) {                                                                                                                                            \
                std::vector<T> arg2(arg1.size(), elem);                                                                                                    \
                return arg1 OPERATOR_ASSIGN arg2;                                                                                                          \
            }                                                                                                                                              \
            template <typename T> inline std::vector<mcdata<T> > OPERATOR_NAME(                                                                            \
                typename mcdata<T>::element_type const & elem, typename std::vector<mcdata<T> > arg2                                                       \
            ) {                                                                                                                                            \
                std::vector<T> arg1(arg2.size(), elem);                                                                                                    \
                return arg1 OPERATOR_ASSIGN arg2;                                                                                                          \
            }
        ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(operator+,+)
        ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(operator-,-)
        ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(operator*,*)
        ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION(operator/,/)
        #undef ALPS_ALEA_MCDATA_IMPLEMENT_OPERATION
        template <typename T> mcdata<T> pow(mcdata<T> rhs, typename mcdata<T>::element_type exponent) {
            if (exponent == 1.)
              return rhs;
            else {
                using std::pow;
                using std::abs;
                using alps::numeric::pow;
                using alps::numeric::abs;
                using boost::lambda::bind;
                using boost::numeric::operators::operator-;
                using boost::numeric::operators::operator*;
                rhs.transform(bind<T>(static_cast<
                    typename mcdata<T>::value_type(*)(typename mcdata<T>::value_type, typename mcdata<T>::element_type)
                >(&pow), boost::lambda::_1, exponent), abs(exponent * pow(rhs.mean(), exponent - 1.) * rhs.error()));
                return rhs;
            }
        }
        template <typename T> std::vector<mcdata<T> > pow(std::vector<mcdata<T> > rhs, typename mcdata<T>::element_type exponent) {
            using std::pow;
            std::transform(rhs.begin(), rhs.end(), rhs.begin(), boost::lambda::bind<mcdata<T> >(static_cast<mcdata<T>(*)(mcdata<T>, typename mcdata<T>::element_type)>(&pow), boost::lambda::_1, exponent));
            return rhs;
        }
        #define ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(FUNCTION_NAME, ERROR)                                                                                  \
            template <typename T> inline mcdata<T> FUNCTION_NAME (mcdata<T> rhs) {                                                                         \
                using alps::numeric::sq;                                                                                                                   \
                using alps::numeric::cbrt;                                                                                                                 \
                using alps::numeric::cb;                                                                                                                   \
                using std::sqrt;                                                                                                                           \
                using alps::numeric::sqrt;                                                                                                                 \
                using std::exp;                                                                                                                            \
                using alps::numeric::exp;                                                                                                                  \
                using std::log;                                                                                                                            \
                using alps::numeric::log;                                                                                                                  \
                using std::abs;                                                                                                                            \
                using alps::numeric::abs;                                                                                                                  \
                using std::sqrt;                                                                                                                           \
                using alps::numeric::sqrt;                                                                                                                 \
                using std::sin;                                                                                                                            \
                using alps::numeric::pow;                                                                                                                  \
                using std::pow;                                                                                                                            \
                using alps::numeric::sin;                                                                                                                  \
                using std::cos;                                                                                                                            \
                using alps::numeric::cos;                                                                                                                  \
                using std::tan;                                                                                                                            \
                using alps::numeric::tan;                                                                                                                  \
                using std::sinh;                                                                                                                           \
                using alps::numeric::sinh;                                                                                                                 \
                using std::cosh;                                                                                                                           \
                using alps::numeric::cosh;                                                                                                                 \
                using std::tanh;                                                                                                                           \
                using alps::numeric::tanh;                                                                                                                 \
                using std::asin;                                                                                                                           \
                using alps::numeric::asin;                                                                                                                 \
                using std::acos;                                                                                                                           \
                using alps::numeric::acos;                                                                                                                 \
                using std::atan;                                                                                                                           \
                using alps::numeric::atan;                                                                                                                 \
                using boost::math::asinh;                                                                                                                  \
                using alps::numeric::asinh;                                                                                                                \
                using boost::math::acosh;                                                                                                                  \
                using alps::numeric::acosh;                                                                                                                \
                using boost::math::atanh;                                                                                                                  \
                using alps::numeric::atanh;                                                                                                                \
                using boost::numeric::operators::operator+;                                                                                                \
                using boost::numeric::operators::operator-;                                                                                                \
                using boost::numeric::operators::operator*;                                                                                                \
                using boost::numeric::operators::operator/;                                                                                                \
                using alps::numeric::operator+;                                                                                                            \
                using alps::numeric::operator-;                                                                                                            \
                using alps::numeric::operator*;                                                                                                            \
                using alps::numeric::operator/;                                                                                                            \
                rhs.transform(static_cast<typename mcdata<T>::value_type(*)(typename mcdata<T>::value_type)>(&FUNCTION_NAME), ERROR);                      \
                return rhs;                                                                                                                                \
            }                                                                                                                                              \
            template <typename T> std::vector<mcdata<T> > FUNCTION_NAME(std::vector<mcdata<T> > rhs) {                                                      \
                std::transform(rhs.begin(), rhs.end(), rhs.begin(), static_cast<mcdata<T> (*)(mcdata<T>)>(&FUNCTION_NAME));                                \
                return rhs;                                                                                                                                \
            }
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(sin, abs(cos(rhs.mean()) * rhs.error()))
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(cos, abs(-sin(rhs.mean()) * rhs.error()))
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(tan, abs(1. / (cos(rhs.mean()) * cos(rhs.mean())) * rhs.error()))
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(sinh, abs(cosh(rhs.mean()) * rhs.error()))
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(cosh, abs(sinh(rhs.mean()) * rhs.error()))
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(tanh, abs(1. / (cosh(rhs.mean()) * cosh(rhs.mean())) * rhs.error()))
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(asin, abs(1. / sqrt(1. - rhs.mean() * rhs.mean()) * rhs.error()))
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(acos, abs(-1. / sqrt(1. - rhs.mean() * rhs.mean()) * rhs.error()));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(atan, abs(1. / (1. + rhs.mean() * rhs.mean()) * rhs.error()));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(asinh, abs(1. / sqrt(rhs.mean() * rhs.mean() + 1.) * rhs.error()));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(acosh, abs(1. / sqrt(rhs.mean() * rhs.mean() - 1.) * rhs.error()));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(atanh, abs(1./(1. - rhs.mean() * rhs.mean()) * rhs.error()));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(abs, rhs.error());
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(sq, abs(2. * rhs.mean() * rhs.error()));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(cb, abs(3. * sq(rhs.mean()) * rhs.error()));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(sqrt, abs(rhs.error() / (2. * sqrt(rhs.mean()))));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(cbrt, abs(rhs.error() / (3. * sq(pow(rhs.mean(), 1. / 3)))));
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(exp, exp(rhs.mean()) * rhs.error());
        ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION(log, abs(rhs.error() / rhs.mean()));
        #undef ALPS_ALEA_MCDATA_IMPLEMENT_FUNCTION
    }
}
#endif