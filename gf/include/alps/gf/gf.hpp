/*
 * Copyright (C) 1998-2016 ALPS Collaboration. See COPYRIGHT.TXT
 * All rights reserved. Use is subject to license terms. See LICENSE.TXT
 * For use in publications, see ACKNOWLEDGE.TXT
 */
#pragma once
#include <complex>
#include <algorithm>
#include <functional>
#include <boost/multi_array.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

// #include <alps/type_traits/is_complex.hpp>
#include <alps/hdf5/archive.hpp>
#include <alps/hdf5/complex.hpp>
#include <alps/hdf5/multi_array.hpp>

#ifdef ALPS_HAVE_MPI
#include "mpi_bcast.hpp"
#endif

#include "mesh.hpp"

namespace alps {
    namespace gf {

        const int minor_version=1;
        const int major_version=0;
       
        void save_version(alps::hdf5::archive& ar, const std::string& path);
        
        bool check_version(alps::hdf5::archive& ar, const std::string& path);
        
        namespace detail{
            template<typename T> void print_no_complex(std::ostream &os, const T &z){
                os<<z;
            }
            template<> void print_no_complex(std::ostream &os, const std::complex<double> &z); //specialization for printing complex as ... real imag ...

            /// Binary operation returning its RHS (used for assignment-as-binary-op)
            template <typename T>
            class choose_rhs {
              public:
                const T& operator()(const T&, const T& rhs) const
                {
                    return rhs;
                }
            };
        }

        template<class VTYPE, class MESH1> class one_index_gf
        :boost::additive<one_index_gf<VTYPE,MESH1>,
         boost::multiplicative2<one_index_gf<VTYPE,MESH1>,VTYPE> >
        {
            public:
            typedef boost::multi_array<VTYPE,1> container_type;
            typedef MESH1 mesh1_type;
            typedef VTYPE value_type;

            private:
            MESH1 mesh1_;

            container_type data_;

            /// Check if meshes are compatible, throw if not
            void check_meshes(const one_index_gf& rhs)
            {
                if (mesh1_!=rhs.mesh1_) {
                    throw std::invalid_argument("Green Functions have incompatible meshes");
                }
            }
          
            public:
            one_index_gf(const MESH1& mesh1)
                : mesh1_(mesh1),
                  data_(boost::extents[mesh1_.extent()])
            {
            }

            one_index_gf(const MESH1& mesh1,
                         const container_type& data)
                : mesh1_(mesh1),
                  data_(data)
            {
                if (mesh1_.extent()!=data_.shape()[0])
                    throw std::invalid_argument("Initialization of GF with the data of incorrect size");
            }

            const container_type& data() const { return data_; }

            const MESH1& mesh1() const { return mesh1_; }

            const value_type& operator()(typename MESH1::index_type i1) const
            {
                return data_[i1()];
            }

            value_type& operator()(typename MESH1::index_type i1)
            {
                return data_[i1()];
            }

            /// Initialize the GF data to value_type(0.)
            void initialize()
            {
                for (int i=0; i<mesh1_.extent(); ++i) {
                        data_[i]=value_type(0.0);
                }
            }
            /// Norm operation (FIXME: is it always double??)
            double norm() const
            {
                using std::abs;
                double v=0;
                for (const value_type* ptr=data_.origin(); ptr!=data_.origin()+data_.num_elements(); ++ptr) {
                    v=std::max(abs(*ptr), v);
                }
                return v;
            }

            /// Assignment-op with scalar
            template <typename op>
            one_index_gf& do_op(const value_type& scalar)
            {

                std::transform(data_.origin(), data_.origin()+data_.num_elements(), // inputs
                               data_.origin(), // output
                               std::bind2nd(op(), scalar)); // bound binary(?,scalar)

                return *this;
            }

            /// Assignment-op with another GF
            template <typename op>
            one_index_gf& do_op(const one_index_gf& rhs)
            {
                check_meshes(rhs);
                std::transform(data_.origin(), data_.origin()+data_.num_elements(), rhs.data_.origin(), // inputs
                               data_.origin(), // output
                               op());

                return *this;
            }

            /// Element-wise addition
            one_index_gf& operator+=(const one_index_gf& rhs)
            {
                return do_op< std::plus<value_type> >(rhs);
            }

            /// Element-wise subtraction
            one_index_gf& operator-=(const one_index_gf& rhs)
            {
                return do_op< std::minus<value_type> >(rhs);
            }

            /// Element-wise scaling 
            one_index_gf& operator*=(const value_type& scalar)
            {
                return do_op< std::multiplies<value_type> >(scalar);
            }

            /// Element-wise scaling 
            one_index_gf& operator/=(const value_type& scalar)
            {
                return do_op< std::divides<value_type> >(scalar);
            }

            /// Element-wise assignment
            /** @Note Copies only the data, requires identical grids. */
            // FIXME: this is a hack, to be replaced by a proper assignment later
            one_index_gf& operator=(const one_index_gf& rhs)
            {
                return do_op< detail::choose_rhs<value_type> >(rhs);
            }
            
            /// Save the GF to HDF5
            void save(alps::hdf5::archive& ar, const std::string& path) const
            {
                save_version(ar,path);
                ar[path+"/data"] << data_;
                ar[path+"/mesh/N"] << int(container_type::dimensionality);
                mesh1_.save(ar,path+"/mesh/1");
            }

            /// Load the GF from HDF5
            void load(alps::hdf5::archive& ar, const std::string& path)
            {
                if (!check_version(ar,path)) throw std::runtime_error("Incompatible archive version");

                int ndim;
                ar[path+"/mesh/N"] >> ndim;
                if (ndim != container_type::dimensionality) throw std::runtime_error("Wrong number of dimension reading Matsubara GF, ndim="+boost::lexical_cast<std::string>(ndim) );

                mesh1_.load(ar,path+"/mesh/1");

                data_.resize(boost::extents[mesh1_.extent()]);

                ar[path+"/data"] >> data_;
            }

#ifdef ALPS_HAVE_MPI
            /// Broadcast the GF (together with meshes)
          void broadcast(const alps::mpi::communicator& comm, int root)
            {
                mesh1_.broadcast(comm,root);
                detail::broadcast(comm, data_, root);
            }
#endif          

        };
        template<class value_type, class MESH1> std::ostream &operator<<(std::ostream &os, one_index_gf<value_type,MESH1> G){
          os<<G.mesh1();
          for(int i=0;i<G.mesh1().extent();++i){
            os<<G.mesh1().points()[i]<<" ";
            detail::print_no_complex<value_type>(os, G(typename MESH1::index_type(i)));
            os<<std::endl;
          }
          return os;
        }

        template<class VTYPE, class MESH1, class MESH2> class two_index_gf
        :boost::additive<two_index_gf<VTYPE,MESH1,MESH2>,
         boost::multiplicative2<two_index_gf<VTYPE,MESH1,MESH2>,VTYPE> >
        {
            public:
            typedef boost::multi_array<VTYPE,2> container_type;
            typedef MESH1 mesh1_type;
            typedef MESH2 mesh2_type;
            typedef VTYPE value_type;

            private: 
            MESH1 mesh1_;
            MESH2 mesh2_;

            container_type data_;

            /// Check if meshes are compatible, throw if not
            void check_meshes(const two_index_gf& rhs)
            {
                if (mesh1_!=rhs.mesh1_ ||
                    mesh2_!=rhs.mesh2_) {
                    throw std::invalid_argument("Green Functions have incompatible meshes");
                }
            }
          

            public:
            two_index_gf(const MESH1& mesh1,
                         const MESH2& mesh2)
                : mesh1_(mesh1), mesh2_(mesh2),
                  data_(boost::extents[mesh1_.extent()][mesh2_.extent()])
            {
            }

            two_index_gf(const MESH1& mesh1,
                         const MESH2& mesh2,
                         const container_type& data)
                : mesh1_(mesh1), mesh2_(mesh2),
                  data_(data)
            {
                if (mesh1_.extent()!=data_.shape()[0] || mesh2_.extent()!=data_.shape()[1])
                    throw std::invalid_argument("Initialization of GF with the data of incorrect size");
            }

            const container_type& data() const { return data_; }
            
            const MESH1& mesh1() const { return mesh1_; } 
            const MESH2& mesh2() const { return mesh2_; } 

            const value_type& operator()(typename MESH1::index_type i1, typename MESH2::index_type i2) const
            {
                return data_[i1()][i2()];
            }
        
            value_type& operator()(typename MESH1::index_type i1, typename MESH2::index_type i2)
            {
                return data_[i1()][i2()];
            }
        
            /// Initialize the GF data to value_type(0.)
            void initialize()
            {
                for (int i=0; i<mesh1_.extent(); ++i) {
                    for (int j=0; j<mesh2_.extent(); ++j) {
                        data_[i][j]=value_type(0.0);
                    }
                }
            }
            /// Norm operation (FIXME: is it always double??)
            double norm() const
            {
                using std::abs;
                double v=0;
                for (const value_type* ptr=data_.origin(); ptr!=data_.origin()+data_.num_elements(); ++ptr) {
                    v=std::max(abs(*ptr), v);
                }
                return v;
            }

            /// Assignment-op with another GF
            template <typename op>
            two_index_gf& do_op(const two_index_gf& rhs)
            {
                check_meshes(rhs);
                std::transform(data_.origin(), data_.origin()+data_.num_elements(), rhs.data_.origin(), // inputs
                               data_.origin(), // output
                               op());

                return *this;
            }

            /// Element-wise addition
            two_index_gf& operator+=(const two_index_gf& rhs)
            {
                return do_op< std::plus<value_type> >(rhs);
            }

            /// Element-wise subtraction
            two_index_gf& operator-=(const two_index_gf& rhs)
            {
                return do_op< std::minus<value_type> >(rhs);
            }
        
            /// Assignment-op with scalar
            template <typename op>
            two_index_gf& do_op(const value_type& scalar)
            {

                std::transform(data_.origin(), data_.origin()+data_.num_elements(), // inputs
                               data_.origin(), // output
                               std::bind2nd(op(), scalar)); // bound binary(?,scalar)

                return *this;
            }

            /// Element-wise scaling 
            two_index_gf& operator*=(const value_type& scalar)
            {
                return do_op< std::multiplies<value_type> >(scalar);
            }

            /// Element-wise scaling 
            two_index_gf& operator/=(const value_type& scalar)
            {
                return do_op< std::divides<value_type> >(scalar);
            }

            /// Element-wise assignment
            /** @Note Copies only the data, requires identical grids. */
            // FIXME: this is a hack, to be replaced by a proper assignment later
            two_index_gf& operator=(const two_index_gf& rhs)
            {
                return do_op< detail::choose_rhs<value_type> >(rhs);
            }
            
            /// Save the GF to HDF5
            void save(alps::hdf5::archive& ar, const std::string& path) const
            {
                save_version(ar,path);
                ar[path+"/data"] << data_;
                ar[path+"/mesh/N"] << int(container_type::dimensionality);
                mesh1_.save(ar,path+"/mesh/1");
                mesh2_.save(ar,path+"/mesh/2");
            }
        
            /// Load the GF from HDF5
            void load(alps::hdf5::archive& ar, const std::string& path)
            {
                if (!check_version(ar,path)) throw std::runtime_error("Incompatible archive version");
          
                int ndim;
                ar[path+"/mesh/N"] >> ndim;
                if (ndim != container_type::dimensionality) throw std::runtime_error("Wrong number of dimension reading Matsubara GF, ndim="+boost::lexical_cast<std::string>(ndim) );
          
                mesh1_.load(ar,path+"/mesh/1");
                mesh2_.load(ar,path+"/mesh/2");
          
                data_.resize(boost::extents[mesh1_.extent()][mesh2_.extent()]);
          
                ar[path+"/data"] >> data_;
            }
        
#ifdef ALPS_HAVE_MPI
            /// Broadcast the GF (with meshes)
          void broadcast(const alps::mpi::communicator& comm, int root)
            {
                mesh1_.broadcast(comm, root);
                mesh2_.broadcast(comm, root);
                detail::broadcast(comm, data_, root);
            }
#endif

        };

        template<class value_type, class MESH1, class MESH2> std::ostream &operator<<(std::ostream &os, two_index_gf<value_type,MESH1,MESH2> G){
          os<<G.mesh1()<<G.mesh2();
          for(int i=0;i<G.mesh1().extent();++i){
            os<<G.mesh1().points()[i]<<" ";
            for(int k=0;k<G.mesh2().extent();++k){
              detail::print_no_complex<value_type>(os, G(typename MESH1::index_type(i),typename MESH2::index_type(k))); os<<" ";
            }
            os<<std::endl;
          }
          return os;
        }


        template<class VTYPE, class MESH1, class MESH2, class MESH3> class three_index_gf
        :boost::additive<three_index_gf<VTYPE,MESH1,MESH2,MESH3>,
         boost::multiplicative2<three_index_gf<VTYPE,MESH1,MESH2,MESH3>,VTYPE> >
        {
            public:
            typedef VTYPE value_type;
            typedef boost::multi_array<value_type,3> container_type;
            typedef MESH1 mesh1_type;
            typedef MESH2 mesh2_type;
            typedef MESH3 mesh3_type;

            private:
            mesh1_type mesh1_;
            mesh2_type mesh2_;
            mesh3_type mesh3_;
        
            container_type data_;
            
            /// Check if meshes are compatible, throw if not
            void check_meshes(const three_index_gf& rhs)
            {
                if (mesh1_!=rhs.mesh1_ ||
                    mesh2_!=rhs.mesh2_ ||
                    mesh3_!=rhs.mesh3_) {
                    throw std::invalid_argument("Green Functions have incompatible meshes");
                }
            }
            
            public:
            three_index_gf(const MESH1& mesh1,
                           const MESH2& mesh2,
                           const MESH3& mesh3)
                : mesh1_(mesh1), mesh2_(mesh2), mesh3_(mesh3),
                  data_(boost::extents[mesh1_.extent()][mesh2_.extent()][mesh3_.extent()])
            {
            }

            three_index_gf(const MESH1& mesh1,
                           const MESH2& mesh2,
                           const MESH3& mesh3,
                           const container_type& data)
                : mesh1_(mesh1), mesh2_(mesh2), mesh3_(mesh3),
                  data_(data)
            {
                if (mesh1_.extent()!=data_.shape()[0] || mesh2_.extent()!=data_.shape()[1] || mesh3_.extent()!=data_.shape()[2])
                    throw std::invalid_argument("Initialization of GF with the data of incorrect size");
            }


            const MESH1& mesh1() const { return mesh1_; } 
            const MESH2& mesh2() const { return mesh2_; } 
            const MESH3& mesh3() const { return mesh3_; } 
            const container_type& data() const { return data_; }
            
            const value_type& operator()(typename MESH1::index_type i1, typename MESH2::index_type i2, typename MESH3::index_type i3) const
            {
                return data_[i1()][i2()][i3()];
            }
        
            value_type& operator()(typename MESH1::index_type i1, typename MESH2::index_type i2, typename MESH3::index_type i3)
            {
                return data_[i1()][i2()][i3()];
            }
        
            /// Initialize the GF data to value_type(0.)
            void initialize()
            {
                for (int i=0; i<mesh1_.extent(); ++i) {
                    for (int j=0; j<mesh2_.extent(); ++j) {
                        for (int k=0; k<mesh3_.extent(); ++k) {
                            data_[i][j][k]=value_type(0.0);
                        }
                    }
                }
            }
        
            /// Norm operation (FIXME: is it always double??)
            double norm() const
            {
                using std::abs;
                double v=0;
                for (const value_type* ptr=data_.origin(); ptr!=data_.origin()+data_.num_elements(); ++ptr) {
                    v=std::max(abs(*ptr), v);
                }
                return v;
            }

            /// Assignment-op with another GF
            template <typename op>
            three_index_gf& do_op(const three_index_gf& rhs)
            {
                check_meshes(rhs);
                std::transform(data_.origin(), data_.origin()+data_.num_elements(), rhs.data_.origin(), // inputs
                               data_.origin(), // output
                               op());

                return *this;
            }

            /// Element-wise addition
            three_index_gf& operator+=(const three_index_gf& rhs)
            {
                return do_op< std::plus<value_type> >(rhs);
            }

            /// Element-wise subtraction
            three_index_gf& operator-=(const three_index_gf& rhs)
            {
                return do_op< std::minus<value_type> >(rhs);
            }

            /// Assignment-op with scalar
            template <typename op>
            three_index_gf& do_op(const value_type& scalar)
            {

                std::transform(data_.origin(), data_.origin()+data_.num_elements(), // inputs
                               data_.origin(), // output
                               std::bind2nd(op(), scalar)); // bound binary(?,scalar)

                return *this;
            }

            /// Element-wise scaling 
            three_index_gf& operator*=(const value_type& scalar)
            {
                return do_op< std::multiplies<value_type> >(scalar);
            }

            /// Element-wise scaling 
            three_index_gf& operator/=(const value_type& scalar)
            {
                return do_op< std::divides<value_type> >(scalar);
            }

            /// Element-wise assignment
            /** @Note Copies only the data, requires identical grids. */
            // FIXME: this is a hack, to be replaced by a proper assignment later
            three_index_gf& operator=(const three_index_gf& rhs)
            {
                return do_op< detail::choose_rhs<value_type> >(rhs);
            }
            
            /// Save the GF to HDF5
            void save(alps::hdf5::archive& ar, const std::string& path) const
            {
                save_version(ar,path);
                ar[path+"/data"] << data_;
                ar[path+"/mesh/N"] << int(container_type::dimensionality);
                mesh1_.save(ar,path+"/mesh/1");
                mesh2_.save(ar,path+"/mesh/2");
                mesh3_.save(ar,path+"/mesh/3");
            }
        
            /// Load the GF from HDF5
            void load(alps::hdf5::archive& ar, const std::string& path)
            {
                if (!check_version(ar,path)) throw std::runtime_error("Incompatible archive version");
          
                int ndim;
                ar[path+"/mesh/N"] >> ndim;
                if (ndim != container_type::dimensionality) throw std::runtime_error("Wrong number of dimension reading Matsubara GF, ndim="+boost::lexical_cast<std::string>(ndim));
          
                mesh1_.load(ar,path+"/mesh/1");
                mesh2_.load(ar,path+"/mesh/2");
                mesh3_.load(ar,path+"/mesh/3");
          
                data_.resize(boost::extents[mesh1_.extent()][mesh2_.extent()][mesh3_.extent()]);
          
                ar[path+"/data"] >> data_;
            }
        
#ifdef ALPS_HAVE_MPI
            /// Broadcast the GF (with meshes)
          void broadcast(const alps::mpi::communicator& comm, int root)
            {
                mesh1_.broadcast(comm, root);
                mesh2_.broadcast(comm, root);
                mesh3_.broadcast(comm, root);
                detail::broadcast(comm, data_, root);
            }
#endif
        };

        template<class value_type, class MESH1, class MESH2, class MESH3> std::ostream &operator<<(std::ostream &os, three_index_gf<value_type,MESH1,MESH2,MESH3> G){
          os<<G.mesh1()<<G.mesh2()<<G.mesh3();
          for(int i=0;i<G.mesh1().extent();++i){
            os<<G.mesh1().points()[i]<<" ";
            for(int j=0;j<G.mesh2().extent();++j){
              for(int k=0;k<G.mesh3().extent();++k){
                detail::print_no_complex<value_type>(os, G(typename MESH1::index_type(i),typename MESH2::index_type(j),typename MESH3::index_type(k))); os<<" ";
              }
            }
            os<<std::endl;
          }
          return os;
        }

        template<class VTYPE, class MESH1, class MESH2, class MESH3, class MESH4> class four_index_gf 
        :boost::additive<four_index_gf<VTYPE,MESH1,MESH2,MESH3,MESH4>,
         boost::multiplicative2<four_index_gf<VTYPE,MESH1,MESH2,MESH3,MESH4>,VTYPE> > {
            public:
            typedef VTYPE value_type;
            typedef boost::multi_array<value_type,4> container_type;
            typedef MESH1 mesh1_type;
            typedef MESH2 mesh2_type;
            typedef MESH3 mesh3_type;
            typedef MESH4 mesh4_type;


            private:

            MESH1 mesh1_;
            MESH2 mesh2_;
            MESH3 mesh3_;
            MESH4 mesh4_;

            container_type data_;

            /// Check if meshes are compatible, throw if not
            void check_meshes(const four_index_gf& rhs)
            {
                if (mesh1_!=rhs.mesh1_ ||
                    mesh2_!=rhs.mesh2_ ||
                    mesh3_!=rhs.mesh3_ ||
                    mesh4_!=rhs.mesh4_) {
                    throw std::invalid_argument("Green Functions have incompatible meshes");
                }
            }

            public:

            four_index_gf(const MESH1& mesh1,
                          const MESH2& mesh2,
                          const MESH3& mesh3,
                          const MESH4& mesh4)
                : mesh1_(mesh1), mesh2_(mesh2), mesh3_(mesh3), mesh4_(mesh4),
                  data_(boost::extents[mesh1_.extent()][mesh2_.extent()][mesh3_.extent()][mesh4_.extent()])
            {
            }

            four_index_gf(const MESH1& mesh1,
                          const MESH2& mesh2,
                          const MESH3& mesh3,
                          const MESH4& mesh4,
                          const container_type& data)
                : mesh1_(mesh1), mesh2_(mesh2), mesh3_(mesh3), mesh4_(mesh4),
                  data_(data)
            {
                if (mesh1_.extent()!=data_.shape()[0] || mesh2_.extent()!=data_.shape()[1] ||
                    mesh3_.extent()!=data_.shape()[2] || mesh4_.extent()!=data_.shape()[3])
                    throw std::invalid_argument("Initialization of GF with the data of incorrect size");
            }

            const MESH1& mesh1() const { return mesh1_; } 
            const MESH2& mesh2() const { return mesh2_; } 
            const MESH3& mesh3() const { return mesh3_; } 
            const MESH4& mesh4() const { return mesh4_; } 
            const container_type& data() const { return data_; }
            
            const value_type& operator()(typename MESH1::index_type i1, typename MESH2::index_type i2, typename MESH3::index_type i3, typename MESH4::index_type i4) const
            {
                return data_[mesh1_(i1)][mesh2_(i2)][mesh3_(i3)][mesh4_(i4)];
            }

            value_type& operator()(typename MESH1::index_type i1, typename MESH2::index_type i2, typename MESH3::index_type i3, typename MESH4::index_type i4)
            {
                return data_[mesh1_(i1)][mesh2_(i2)][mesh3_(i3)][mesh4_(i4)];
            }


            /// Initialize the GF data to value_type(0.)
            void initialize()
            {
                for (int i=0; i<mesh1_.extent(); ++i) {
                    for (int j=0; j<mesh2_.extent(); ++j) {
                        for (int k=0; k<mesh3_.extent(); ++k) {
                            for (int l=0; l<mesh4_.extent(); ++l) {
                                data_[i][j][k][l]=value_type(0.0);
                            }
                        }
                    }
                }
            }
            double norm() const
            {
                using std::abs;
                double v=0;
                for (const value_type* ptr=data_.origin(); ptr!=data_.origin()+data_.num_elements(); ++ptr) {
                    v=std::max(abs(*ptr), v);
                }
                return v;
            }

            /// Assignment-op
            template <typename op>
            four_index_gf& do_op(const four_index_gf& rhs)
            {
                check_meshes(rhs);
                std::transform(data_.origin(), data_.origin()+data_.num_elements(), rhs.data_.origin(), // inputs
                               data_.origin(), // output
                               op());

                return *this;
            }

            /// Element-wise addition
            four_index_gf& operator+=(const four_index_gf& rhs)
            {
                return do_op< std::plus<value_type> >(rhs);
            }

            /// Element-wise subtraction
            four_index_gf& operator-=(const four_index_gf& rhs)
            {
                return do_op< std::minus<value_type> >(rhs);
            }

            /// Assignment-op with scalar
            template <typename op>
            four_index_gf& do_op(const value_type& scalar)
            {

                std::transform(data_.origin(), data_.origin()+data_.num_elements(), // inputs
                               data_.origin(), // output
                               std::bind2nd(op(), scalar)); // bound binary(?,scalar)

                return *this;
            }

            /// Element-wise scaling 
            four_index_gf& operator*=(const value_type& scalar)
            {
                return do_op< std::multiplies<value_type> >(scalar);
            }

            /// Element-wise scaling 
            four_index_gf& operator/=(const value_type& scalar)
            {
                return do_op< std::divides<value_type> >(scalar);
            }

            /// Element-wise assignment
            /** @Note Copies only the data, requires identical grids. */
            // FIXME: this is a hack, to be replaced by a proper assignment later
            four_index_gf& operator=(const four_index_gf& rhs)
            {
                return do_op< detail::choose_rhs<value_type> >(rhs);
            }
            
            /// Save the GF to HDF5
            void save(alps::hdf5::archive& ar, const std::string& path) const
            {
                save_version(ar,path);
                ar[path+"/data"] << data_;
                ar[path+"/mesh/N"] << int(container_type::dimensionality);
                mesh1_.save(ar,path+"/mesh/1");
                mesh2_.save(ar,path+"/mesh/2");
                mesh3_.save(ar,path+"/mesh/3");
                mesh4_.save(ar,path+"/mesh/4");
            }

            /// Load the GF from HDF5
            void load(alps::hdf5::archive& ar, const std::string& path)
            {
                if (!check_version(ar,path)) throw std::runtime_error("Incompatible archive version");

                int ndim;
                ar[path+"/mesh/N"] >> ndim;
                if (ndim != container_type::dimensionality) throw std::runtime_error("Wrong number of dimension reading Matsubara GF, ndim="+boost::lexical_cast<std::string>(ndim));

                mesh1_.load(ar,path+"/mesh/1");
                mesh2_.load(ar,path+"/mesh/2");
                mesh3_.load(ar,path+"/mesh/3");
                mesh4_.load(ar,path+"/mesh/4");

                data_.resize(boost::extents[mesh1_.extent()][mesh2_.extent()][mesh3_.extent()][mesh4_.extent()]);
                
                ar[path+"/data"] >> data_;
            }

#ifdef ALPS_HAVE_MPI
            /// Broadcast the GF (with meshes)
          void broadcast(const alps::mpi::communicator& comm, int root)
            {
                mesh1_.broadcast(comm,root);
                mesh2_.broadcast(comm,root);
                mesh3_.broadcast(comm,root);
                mesh4_.broadcast(comm,root);
                detail::broadcast(comm, data_, root);
            }
#endif
        };


        template<class value_type, class MESH1, class MESH2, class MESH3, class MESH4> std::ostream &operator<<(std::ostream &os, four_index_gf<value_type,MESH1,MESH2,MESH3,MESH4> G){
          os<<G.mesh1()<<G.mesh2()<<G.mesh3()<<G.mesh4();
          for(int i=0;i<G.mesh1().extent();++i){
            os<<G.mesh1().points()[i]<<" ";
            for(int j=0;j<G.mesh2().extent();++j){
              for(int k=0;k<G.mesh3().extent();++k){
                for(int l=0;l<G.mesh4().extent();++l){
                  detail::print_no_complex<value_type>(os, G(typename MESH1::index_type(i),typename MESH2::index_type(j),typename MESH3::index_type(k),typename MESH4::index_type(l))); os<<" ";
                }
              }
            }
            os<<std::endl;
          }
          return os;
        }

        template<class VTYPE, class MESH1, class MESH2, class MESH3, class MESH4, class MESH5> class five_index_gf
            :boost::additive<five_index_gf<VTYPE,MESH1,MESH2,MESH3,MESH4,MESH5>,
             boost::multiplicative2<five_index_gf<VTYPE,MESH1,MESH2,MESH3,MESH4,MESH5>,VTYPE> > {
            public:
            typedef VTYPE value_type;
            typedef boost::multi_array<value_type,5> container_type;
            typedef MESH1 mesh1_type;
            typedef MESH2 mesh2_type;
            typedef MESH3 mesh3_type;
            typedef MESH4 mesh4_type;
            typedef MESH5 mesh5_type;


            private:

            MESH1 mesh1_;
            MESH2 mesh2_;
            MESH3 mesh3_;
            MESH4 mesh4_;
            MESH5 mesh5_;

            container_type data_;

            /// Check if meshes are compatible, throw if not
            void check_meshes(const five_index_gf& rhs)
            {
                if (mesh1_!=rhs.mesh1_ ||
                    mesh2_!=rhs.mesh2_ ||
                    mesh3_!=rhs.mesh3_ ||
                    mesh4_!=rhs.mesh4_ ||
                    mesh5_!=rhs.mesh5_) {
                    throw std::invalid_argument("Green Functions have incompatible meshes");
                }
            }

            public:

            five_index_gf(const MESH1& mesh1,
                          const MESH2& mesh2,
                          const MESH3& mesh3,
                          const MESH4& mesh4,
                          const MESH5& mesh5)
                : mesh1_(mesh1), mesh2_(mesh2), mesh3_(mesh3), mesh4_(mesh4),mesh5_(mesh5),
                  data_(boost::extents[mesh1_.extent()][mesh2_.extent()][mesh3_.extent()][mesh4_.extent()][mesh5_.extent()])
            {
            }

            five_index_gf(const MESH1& mesh1,
                          const MESH2& mesh2,
                          const MESH3& mesh3,
                          const MESH4& mesh4,
                          const MESH5& mesh5,
                          const container_type& data)
                : mesh1_(mesh1), mesh2_(mesh2), mesh3_(mesh3), mesh4_(mesh4), mesh5_(mesh5),
                  data_(data)
            {
                if (mesh1_.extent()!=data_.shape()[0] || mesh2_.extent()!=data_.shape()[1] ||
                    mesh3_.extent()!=data_.shape()[2] || mesh4_.extent()!=data_.shape()[3] || mesh5_.extent()!=data_.shape()[4])
                    throw std::invalid_argument("Initialization of GF with the data of incorrect size");
            }

            const MESH1& mesh1() const { return mesh1_; }
            const MESH2& mesh2() const { return mesh2_; }
            const MESH3& mesh3() const { return mesh3_; }
            const MESH4& mesh4() const { return mesh4_; }
            const MESH5& mesh5() const { return mesh5_; }
            const container_type& data() const { return data_; }

            const value_type& operator()(typename MESH1::index_type i1, typename MESH2::index_type i2, typename MESH3::index_type i3, typename MESH4::index_type i4, typename MESH4::index_type i5) const
            {
                return data_[mesh1_(i1)][mesh2_(i2)][mesh3_(i3)][mesh4_(i4)][mesh5_(i5)];
            }

            value_type& operator()(typename MESH1::index_type i1, typename MESH2::index_type i2, typename MESH3::index_type i3, typename MESH4::index_type i4, typename MESH4::index_type i5)
            {
                return data_[mesh1_(i1)][mesh2_(i2)][mesh3_(i3)][mesh4_(i4)][mesh5_(i5)];
            }


            /// Initialize the GF data to value_type(0.)
            void initialize()
            {
                for (int i=0; i<mesh1_.extent(); ++i) {
                    for (int j=0; j<mesh2_.extent(); ++j) {
                        for (int k=0; k<mesh3_.extent(); ++k) {
                          for (int l=0; l<mesh4_.extent(); ++l) {
                            for (int m=0; m<mesh5_.extent(); ++m) {
                                data_[i][j][k][l][m]=value_type(0.0);
                            }
                          }
                        }
                    }
                }
            }
            double norm() const
            {
                using std::abs;
                double v=0;
                for (const value_type* ptr=data_.origin(); ptr!=data_.origin()+data_.num_elements(); ++ptr) {
                    v=std::max(abs(*ptr), v);
                }
                return v;
            }
            /// Assignment-op
            template <typename op>
            five_index_gf& do_op(const five_index_gf& rhs)
            {
                check_meshes(rhs);
                std::transform(data_.origin(), data_.origin()+data_.num_elements(), rhs.data_.origin(), // inputs
                               data_.origin(), // output
                               op());

                return *this;
            }

            /// Element-wise addition
            five_index_gf& operator+=(const five_index_gf& rhs)
            {
                return do_op< std::plus<value_type> >(rhs);
            }

            /// Element-wise subtraction
            five_index_gf& operator-=(const five_index_gf& rhs)
            {
                return do_op< std::minus<value_type> >(rhs);
            }
            /// Assignment-op with scalar
            template <typename op>
            five_index_gf& do_op(const value_type& scalar)
            {

                std::transform(data_.origin(), data_.origin()+data_.num_elements(), // inputs
                               data_.origin(), // output
                               std::bind2nd(op(), scalar)); // bound binary(?,scalar)

                return *this;
            }
            /// Element-wise scaling
            five_index_gf& operator*=(const value_type& scalar)
            {
                return do_op< std::multiplies<value_type> >(scalar);
            }

            /// Element-wise scaling
            five_index_gf& operator/=(const value_type& scalar)
            {
                return do_op< std::divides<value_type> >(scalar);
            }

            /// Element-wise assignment
            /** @Note Copies only the data, requires identical grids. */
            // FIXME: this is a hack, to be replaced by a proper assignment later
            five_index_gf& operator=(const five_index_gf& rhs)
            {
                return do_op< detail::choose_rhs<value_type> >(rhs);
            }
            
            /// Save the GF to HDF5
            void save(alps::hdf5::archive& ar, const std::string& path) const
            {
                save_version(ar,path);
                ar[path+"/data"] << data_;
                ar[path+"/mesh/N"] << int(container_type::dimensionality);
                mesh1_.save(ar,path+"/mesh/1");
                mesh2_.save(ar,path+"/mesh/2");
                mesh3_.save(ar,path+"/mesh/3");
                mesh4_.save(ar,path+"/mesh/4");
                mesh5_.save(ar,path+"/mesh/5");
            }

            /// Load the GF from HDF5
            void load(alps::hdf5::archive& ar, const std::string& path)
            {
                if (!check_version(ar,path)) throw std::runtime_error("Incompatible archive version");

                int ndim;
                ar[path+"/mesh/N"] >> ndim;
                if (ndim != container_type::dimensionality) throw std::runtime_error("Wrong number of dimension reading Matsubara GF, ndim="+boost::lexical_cast<std::string>(ndim));

                mesh1_.load(ar,path+"/mesh/1");
                mesh2_.load(ar,path+"/mesh/2");
                mesh3_.load(ar,path+"/mesh/3");
                mesh4_.load(ar,path+"/mesh/4");
                mesh5_.load(ar,path+"/mesh/5");

                data_.resize(boost::extents[mesh1_.extent()][mesh2_.extent()][mesh3_.extent()][mesh4_.extent()][mesh5_.extent()]);

                ar[path+"/data"] >> data_;
            }

#ifdef ALPS_HAVE_MPI
            /// Broadcast the GF (with meshes)
          void broadcast(const alps::mpi::communicator& comm, int root)
            {
                mesh1_.broadcast(comm,root);
                mesh2_.broadcast(comm,root);
                mesh3_.broadcast(comm,root);
                mesh4_.broadcast(comm,root);
                mesh5_.broadcast(comm,root);
                detail::broadcast(comm, data_, root);
            }
#endif
            
        };
        template<class value_type, class MESH1, class MESH2, class MESH3, class MESH4, class MESH5> std::ostream &operator<<(std::ostream &os, five_index_gf<value_type,MESH1,MESH2,MESH3,MESH4,MESH5> G){
          os<<G.mesh1()<<G.mesh2()<<G.mesh3()<<G.mesh4();
          for(int i=0;i<G.mesh1().extent();++i){
            os<<G.mesh1().points()[i]<<" ";
            for(int j=0;j<G.mesh2().extent();++j){
              for(int k=0;k<G.mesh3().extent();++k){
                for(int l=0;l<G.mesh4().extent();++l){
                  for(int m=0;m<G.mesh5().extent();++m){
                  detail::print_no_complex<value_type>(os, G(typename MESH1::index_type(i),typename MESH2::index_type(j),typename MESH3::index_type(k),typename MESH4::index_type(l),typename MESH5::index_type(m))); os<<" ";
                }
                }
              }
            }
            os<<std::endl;
          }
          return os;
        }

        typedef five_index_gf<std::complex<double>, matsubara_mesh<mesh::POSITIVE_ONLY>, real_space_index_mesh,real_space_index_mesh, index_mesh, index_mesh> omega_r1_r2_sigma1_sigma2_gf;
        typedef five_index_gf<             double , itime_mesh    , real_space_index_mesh,real_space_index_mesh, index_mesh, index_mesh> itime_r1_r2_sigma1_sigma2_gf;

        typedef four_index_gf<std::complex<double>, matsubara_mesh<mesh::POSITIVE_ONLY>, momentum_index_mesh, momentum_index_mesh, index_mesh> omega_k1_k2_sigma_gf;
        typedef four_index_gf<             double , itime_mesh    , momentum_index_mesh, momentum_index_mesh, index_mesh> itime_k1_k2_sigma_gf;
        typedef four_index_gf<std::complex<double>, matsubara_mesh<mesh::POSITIVE_ONLY>, real_space_index_mesh, real_space_index_mesh, index_mesh> omega_r1_r2_sigma_gf;
        typedef four_index_gf<             double , itime_mesh    , real_space_index_mesh, real_space_index_mesh, index_mesh> itime_r1_r2_sigma_gf;
        typedef four_index_gf<std::complex<double>, itime_mesh    , real_space_index_mesh, real_space_index_mesh, index_mesh> itime_r1_r2_sigma_complex_gf;
        typedef four_index_gf<std::complex<double>, matsubara_mesh<mesh::POSITIVE_ONLY>, momentum_index_mesh, index_mesh, index_mesh> omega_k_sigma1_sigma2_gf;
        typedef four_index_gf<             double , itime_mesh    , momentum_index_mesh, index_mesh, index_mesh> itime_k_sigma1_sigma2_gf;

        typedef three_index_gf<std::complex<double>, matsubara_mesh<mesh::POSITIVE_ONLY>, momentum_index_mesh, index_mesh> omega_k_sigma_gf;
        typedef three_index_gf<             double , itime_mesh    , momentum_index_mesh, index_mesh> itime_k_sigma_gf;
        typedef three_index_gf<             double , momentum_index_mesh, index_mesh, index_mesh> k_sigma1_sigma2_gf;
        
        typedef two_index_gf<std::complex<double>, matsubara_mesh<mesh::POSITIVE_ONLY>, index_mesh> omega_sigma_gf;
        typedef two_index_gf<             double , itime_mesh, index_mesh> itime_sigma_gf;
        typedef two_index_gf<double, momentum_index_mesh, index_mesh> k_sigma_gf;

        typedef one_index_gf<std::complex<double>, matsubara_mesh<mesh::POSITIVE_ONLY> >omega_gf;
        typedef one_index_gf<             double , itime_mesh> itime_gf;
        typedef one_index_gf<             double , index_mesh> sigma_gf;

        typedef omega_k1_k2_sigma_gf matsubara_gf;

    }
}
