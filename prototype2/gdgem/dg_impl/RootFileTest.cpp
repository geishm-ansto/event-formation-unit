/** Copyright (C) 2017 European Spallation Source ERIC */

#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <gdgem/dg_impl/RootFile.h>
#include <gdgem/dg_impl/TestData.h>
#include <test/TestBase.h>

class RootFileTest : public TestBase {
protected:
  virtual void SetUp() {  }
  virtual void TearDown() { }
};

TEST_F(RootFileTest, Run16_1_to_16)
{
	std::vector<int> pXChips {0, 1, 6, 7};
	std::vector<int> pYChips {10, 11, 14, 15};
	int pTAC = 60;
	int pBC = 20;
	int pAcqWin = 4000;
	int pADCThreshold=0;
  	int pMinClusterSize = 3;
	//Maximum time difference between strips in time sorted cluster (x or y)
  	float pDeltaTimeHits = 200;
	//Number of missing strips in strip sorted cluster (x or y)
	int pDeltaStripHits = 2;
	//Maximum time span for total cluster (x or y)
	float pDeltaTimeSpan = 500;
	//Maximum cluster time difference between matching clusters in x and y
	//Cluster time is either calculated with center-of-mass or uTPC method
	float pDeltaTimePlanes = 200;


	RootFile nmxdata(pBC, pTAC, pAcqWin, pXChips, pYChips, pADCThreshold, pMinClusterSize, pDeltaTimeHits, pDeltaStripHits,pDeltaTimeSpan,pDeltaTimePlanes);

  	for (auto hit : Run16_1_to_16) { // replace with UDP receive()
		int result = nmxdata.AnalyzeHitData(hit.srs_timestamp, hit.framecounter, hit.fec, hit.chip_id, hit.channel, hit.bcid, hit.tdc, hit.adc, hit.overthreshold);
		if (result == -1) {
			printf("result == -1\n");
			break;
		}
	}

  	ASSERT_EQ(nmxdata.getNumClustersX(), 3);
  	ASSERT_EQ(nmxdata.getNumClustersY(), 3);
  	ASSERT_EQ(nmxdata.getNumClustersXY(), 3);
  	ASSERT_EQ(nmxdata.getNumClustersXY_uTPC(), 3);
}

int main(int argc, char **argv) {
  	testing::InitGoogleTest(&argc, argv);
  	return RUN_ALL_TESTS();
}
