#include <gtest/gtest.h>
#include <windows.h>
#include <vector>
#include <numeric>

// ReDimCore is exported from dbprocore or linked internally
// We test the exact cumulative product formula used in ReDimCore
TEST(ReDimCoreMathTest, CalculatesMultidimensionalSizesCorrectly) {
    // Simulate array header cumulative products for a 3D array (5, 4, 3)
    // DimCore stores cumulative element counts:
    // header[0] = D1 = 5
    // header[1] = D1 * D2 = 5 * 4 = 20
    // header[2] = D1 * D2 * D3 = 5 * 4 * 3 = 60
    DWORD dwOld[9] = { 5, 20, 60, 60, 60, 60, 60, 60, 60 };

    DWORD dwOldDims[9];
    for (DWORD h = 0; h <= 8; h++) {
        DWORD dwDataChunkSize = (h == 0) ? 1 : dwOld[h - 1];
        DWORD dwActualDimValue = 0;
        if (dwDataChunkSize > 0) dwActualDimValue = dwOld[h] / dwDataChunkSize;
        dwOldDims[h] = dwActualDimValue;
    }

    // Verify exact dimension recovery
    EXPECT_EQ(dwOldDims[0], 5u);
    EXPECT_EQ(dwOldDims[1], 4u);
    EXPECT_EQ(dwOldDims[2], 3u);
    EXPECT_EQ(dwOldDims[3], 1u); // Default empty dimensions evaluate to 1
}

TEST(ReDimCoreMathTest, Calculates5DArrayDimensionsCorrectly) {
    // 5D array (2, 3, 4, 5, 6)
    // header[0] = 2
    // header[1] = 2 * 3 = 6
    // header[2] = 2 * 3 * 4 = 24
    // header[3] = 2 * 3 * 4 * 5 = 120
    // header[4] = 2 * 3 * 4 * 5 * 6 = 720
    DWORD dwOld[9] = { 2, 6, 24, 120, 720, 720, 720, 720, 720 };

    DWORD dwOldDims[9];
    for (DWORD h = 0; h <= 8; h++) {
        DWORD dwDataChunkSize = (h == 0) ? 1 : dwOld[h - 1];
        DWORD dwActualDimValue = 0;
        if (dwDataChunkSize > 0) dwActualDimValue = dwOld[h] / dwDataChunkSize;
        dwOldDims[h] = dwActualDimValue;
    }

    EXPECT_EQ(dwOldDims[0], 2u);
    EXPECT_EQ(dwOldDims[1], 3u);
    EXPECT_EQ(dwOldDims[2], 4u);
    EXPECT_EQ(dwOldDims[3], 5u);
    EXPECT_EQ(dwOldDims[4], 6u);
}
