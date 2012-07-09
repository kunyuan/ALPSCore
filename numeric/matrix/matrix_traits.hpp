/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                                 *
 * ALPS Project: Algorithms and Libraries for Physics Simulations                  *
 *                                                                                 *
 * ALPS Libraries                                                                  *
 *                                                                                 *
 * Copyright (C) 2010 - 2012 by Andreas Hehn <hehn@phys.ethz.ch>                   *
 *                                                                                 *
 * This software is part of the ALPS libraries, published under the ALPS           *
 * Library License; you can use, redistribute it and/or modify it under            *
 * the terms of the license, either version 1 or (at your option) any later        *
 * version.                                                                        *
 *                                                                                 *
 * You should have received a copy of the ALPS Library License along with          *
 * the ALPS Libraries; see the file LICENSE.txt. If not, the license is also       *
 * available from http://alps.comp-phys.org/.                                      *
 *                                                                                 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,        *
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT       *
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE       *
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,     *
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER     *
 * DEALINGS IN THE SOFTWARE.                                                       *
 *                                                                                 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#ifndef ALPS_MATRIX_TRAITS_HPP
#define ALPS_MATRIX_TRAITS_HPP

namespace alps {
namespace numeric {

    template <typename Matrix>
    struct associated_diagonal_matrix
    {
    };

    template <typename Matrix>
    struct associated_real_diagonal_matrix
    {
    };

    template <typename Matrix, typename Vector>
    struct matrix_vector_multiplies_return_type
    {
    };

    template <typename Matrix>
    struct associated_vector
    {
    };

    template <typename Matrix>
    struct associated_real_vector
    {
    };

    template <typename Matrix1, typename Matrix2>
    struct matrix_matrix_multiply_return_type {
    };

    template <typename Matrix>
    struct matrix_matrix_multiply_return_type<Matrix,Matrix> {
        typedef Matrix type;
    };

    template <typename Matrix, typename T>
    struct is_matrix_scalar_multiplication {
        static bool const value = true;
    };

//    
//    //
//    // std::vector are unfriendly with ambient, there we wrap as associated_diagonal_matrix
//    //
//    template<class T, class MemoryBlock>
//    struct associated_vector<matrix<T,MemoryBlock> >
//    {
//        typedef std::vector<T> type;
//    };
//    
//    template<class T, class MemoryBlock>
//    struct associated_real_vector<matrix<T,MemoryBlock> >
//    {
//        typedef std::vector<typename detail::real_type<T>::type> type;
//    };

} // end namespace numeric
} // end namespace alps
#endif //ALPS_MATRIX_TRAITS_HPP
