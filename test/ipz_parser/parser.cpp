#include <cassert>
#include <defines.hpp>
#include <fstream>
#include <iterator>
#include <parser.hpp>
#include <store.hpp>
#include <gtest/gtest.h>
#include <impl.hpp>
using namespace openpower::vpd;
TEST(runIpzApp, vpdGoodPath)
{
//PROB 1: Hardcoded local file, create a binary file ?
    std::ifstream vpdFile("4900_vpd", std::ios::binary);
    Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
               std::istreambuf_iterator<char>());

    //call app for this vpd
    parser::Impl p(std::move(vpd));
    Stores vpdStore = p.run();

    static const std::string record = "VINI";
    static const std::string keyword = "DR";

//PROB 2: Move this as an utility to store.hpp
    Binary dataFound ;

    ParsedBinData st_bin =  std::get<1>(vpdStore).get();

//*****print VPD received*********************************
    std::cout<<"*******************Received VPD Map***********************\n";
    for ( auto rec : st_bin)
    {
        std::cout<< rec.first<<"\n";
        for (auto kywd : rec.second)
        {
            std::cout<< kywd.first <<" : ";
            for (auto val : kywd.second)
            {
                std::cout<<std::hex<<val;
            }
            std::cout<<"\n";  
        }
    }
//******DONE*********************************************
    auto kw = st_bin.find(record);
    if (st_bin.end() != kw)
    {
        std::cout<<"DBG: record found \n";
        auto value = (kw->second).find(keyword);
        if ((kw->second).end() != value)
        {
            std::cout<<"DBG: keyword found \n";
            dataFound = value->second;
        }
    }

    std::string dataReceived = "";
    //check if data is correct
    for (auto eachData : dataFound)
    {
        dataReceived += std::to_string(eachData);

    }

    ASSERT_EQ(dataReceived, "APSS & TPM  CARD");
}

