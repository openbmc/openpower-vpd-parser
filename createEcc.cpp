#include "createEcc.hpp"


using namespace openpower::vpd;
using namespace std;

namespace openpower
{
namespace vpd
{
namespace modify
{

int createWriteEccForThisRecord(Binary&& vpd,const LE2ByteData recordOffset, LE2ByteData recordLength,LE2ByteData eccOffset, LE2ByteData eccLength)
{
    int rc = eccStatus::SUCCESS;
    auto vpdPtr = vpd.cbegin();

    auto l_status = vpdecc_create_ecc(
        const_cast<uint8_t*>(&vpdPtr[recordOffset]), recordLength,
        const_cast<uint8_t*>(&vpdPtr[eccOffset]),
        reinterpret_cast<size_t*>(&eccLength));
    if (l_status != VPD_ECC_OK)
    {
        rc = eccStatus::FAILED;
    }
    return rc;
}

}
}
}

