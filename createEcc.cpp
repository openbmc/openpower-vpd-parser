#include "createEcc.hpp"

#include "utils.hpp"

using namespace openpower::vpd;
using namespace std;

namespace openpower
{
namespace vpd
{
namespace modify
{

int createWriteEccForThisRecord(Binary& vpd, Binary::const_iterator iterator)
{
    int rc = eccStatus::SUCCESS;

    // Get the Record's offset
    auto recordOffset = readUInt16LE(iterator);

    // Get the Record's length
    std::advance(iterator, sizeof(RecordOffset));
    auto recordLength = readUInt16LE(iterator);

    // Get the Record's ECC's offset
    std::advance(iterator, sizeof(RecordLength));
    auto eccOffset = readUInt16LE(iterator);

    // Get the Record's ECC's length
    std::advance(iterator, sizeof(ECCOffset));
    size_t eccLength = readUInt16LE(iterator);

    auto vpdPtr = vpd.cbegin();

    auto l_status = vpdecc_create_ecc(
        const_cast<uint8_t*>(&vpdPtr[recordOffset]), recordLength,
        const_cast<uint8_t*>(&vpdPtr[eccOffset]),
        &eccLength);
    if (l_status != VPD_ECC_OK)
    {
        rc = eccStatus::FAILED;
    }
    return rc;
}

} // namespace modify
} // namespace vpd
} // namespace openpower
