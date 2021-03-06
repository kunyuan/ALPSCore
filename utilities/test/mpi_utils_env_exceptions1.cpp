/*
 * Copyright (C) 1998-2016 ALPS Collaboration. See COPYRIGHT.TXT
 * All rights reserved. Use is subject to license terms. See LICENSE.TXT
 * For use in publications, see ACKNOWLEDGE.TXT
 */

#include <exception>
#include <alps/utilities/mpi.hpp>

#include <gtest/gtest.h>

#include "mpi_test_support.hpp"

/* Test MPI environment wrapper functionality: abort on exceptions. */

// we can initialize MPI exactly once, so everything is clubbed into this test
TEST(MpiEnvTest, ExceptionsAbort) {
    int argc=1;
    char arg0[]="";
    char* args[]={arg0};
    char** argv=args;

    ASSERT_EQ(0, get_mpi_abort_called()) << "Unexpected initial global counter state";
    ASSERT_FALSE(mpi_is_up());

    try {
        alps::mpi::environment env(argc, argv);
        ASSERT_TRUE(mpi_is_up());

        try {
            alps::mpi::environment sub_env(argc, argv);
            throw std::exception();
        } catch (std::exception&) {
            ASSERT_FALSE(mpi_is_down());
            ASSERT_EQ(0, get_mpi_abort_called()) << "MPI_Abort should not be called";
            throw;
        }
    } catch (std::exception&) {
        // MPI env object is destroyed during active exception,
        // should have called abort (and finalize MPI)
        ASSERT_EQ(1, get_mpi_abort_called()) << "MPI_Abort should be called by now";
        ASSERT_TRUE(mpi_is_down());
    }
}

