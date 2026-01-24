#include "ddimm_parser.hpp"

#include "constants.hpp"
#include "exceptions.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>

namespace vpd
{

static constexpr auto SDRAM_DENSITY_PER_DIE_24GB = 24;
static constexpr auto SDRAM_DENSITY_PER_DIE_32GB = 32;
static constexpr auto SDRAM_DENSITY_PER_DIE_48GB = 48;
static constexpr auto SDRAM_DENSITY_PER_DIE_64GB = 64;
static constexpr auto SDRAM_DENSITY_PER_DIE_UNDEFINED = 0;

static constexpr auto PRIMARY_BUS_WIDTH_32_BITS = 32;
static constexpr auto PRIMARY_BUS_WIDTH_UNUSED = 0;
static constexpr auto DRAM_MANUFACTURER_ID_OFFSET = 0x228;
static constexpr auto DRAM_MANUFACTURER_ID_LENGTH = 0x02;

bool DdimmVpdParser::checkValidValue(uint8_t i_ByteValue, uint8_t i_shift,
                                     uint8_t i_minValue, uint8_t i_maxValue)
{
    bool l_isValid = true;
    uint8_t l_ByteValue = i_ByteValue >> i_shift;
    if ((l_ByteValue > i_maxValue) || (l_ByteValue < i_minValue))
    {
        logging::logMessage(
            "Non valid Value encountered value[" + std::to_string(l_ByteValue) +
            "] range [" + std::to_string(i_minValue) + ".." +
            std::to_string(i_maxValue) + "] found ");
        return false;
    }
    return l_isValid;
}

uint8_t DdimmVpdParser::getDdr5DensityPerDie(uint8_t i_ByteValue)
{
    uint8_t l_densityPerDie = SDRAM_DENSITY_PER_DIE_UNDEFINED;
    if (i_ByteValue < constants::VALUE_5)
    {
        l_densityPerDie = i_ByteValue * constants::VALUE_4;
    }
    else
    {
        switch (i_ByteValue)
        {
            case constants::VALUE_5:
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_24GB;
                break;

            case constants::VALUE_6:
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_32GB;
                break;

            case constants::VALUE_7:
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_48GB;
                break;

            case constants::VALUE_8:
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_64GB;
                break;

            default:
                logging::logMessage(
                    "default value encountered for density per die");
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_UNDEFINED;
                break;
        }
    }
    return l_densityPerDie;
}

uint8_t DdimmVpdParser::getDdr5DiePerPackage(uint8_t i_ByteValue)
{
    uint8_t l_DiePerPackage = constants::VALUE_0;
    if (i_ByteValue < constants::VALUE_2)
    {
        l_DiePerPackage = i_ByteValue + constants::VALUE_1;
    }
    else
    {
        l_DiePerPackage =
            pow(constants::VALUE_2, (i_ByteValue - constants::VALUE_1));
    }
    return l_DiePerPackage;
}

size_t DdimmVpdParser::getDdr5BasedDdimmSize(
    types::BinaryVector::const_iterator i_iterator)
{
    size_t l_dimmSize = 0;

    do
    {
        if (!checkValidValue(i_iterator[constants::SPD_BYTE_235] &
                                 constants::MASK_BYTE_BITS_01,
                             constants::SHIFT_BITS_0, constants::VALUE_1,
                             constants::VALUE_3) ||
            !checkValidValue(i_iterator[constants::SPD_BYTE_235] &
                                 constants::MASK_BYTE_BITS_345,
                             constants::SHIFT_BITS_3, constants::VALUE_1,
                             constants::VALUE_3))
        {
            logging::logMessage(
                "Capacity calculation failed for channels per DIMM. DDIMM Byte "
                "235 value [" +
                std::to_string(i_iterator[constants::SPD_BYTE_235]) + "]");
            break;
        }
        uint8_t l_channelsPerPhy =
            (((i_iterator[constants::SPD_BYTE_235] &
               constants::MASK_BYTE_BITS_01)
                  ? constants::VALUE_1
                  : constants::VALUE_0) +
             ((i_iterator[constants::SPD_BYTE_235] &
               constants::MASK_BYTE_BITS_345)
                  ? constants::VALUE_1
                  : constants::VALUE_0));

        uint8_t l_channelsPerDdimm =
            (((i_iterator[constants::SPD_BYTE_235] &
               constants::MASK_BYTE_BIT_6) >>
              constants::VALUE_6) +
             ((i_iterator[constants::SPD_BYTE_235] &
               constants::MASK_BYTE_BIT_7) >>
              constants::VALUE_7)) *
            l_channelsPerPhy;

        if (!checkValidValue(i_iterator[constants::SPD_BYTE_235] &
                                 constants::MASK_BYTE_BITS_012,
                             constants::SHIFT_BITS_0, constants::VALUE_1,
                             constants::VALUE_3))
        {
            logging::logMessage(
                "Capacity calculation failed for bus width per channel. DDIMM "
                "Byte 235 value [" +
                std::to_string(i_iterator[constants::SPD_BYTE_235]) + "]");
            break;
        }
        uint8_t l_busWidthPerChannel =
            (i_iterator[constants::SPD_BYTE_235] &
             constants::MASK_BYTE_BITS_012)
                ? PRIMARY_BUS_WIDTH_32_BITS
                : PRIMARY_BUS_WIDTH_UNUSED;

        if (!checkValidValue(i_iterator[constants::SPD_BYTE_4] &
                                 constants::MASK_BYTE_BITS_567,
                             constants::SHIFT_BITS_5, constants::VALUE_0,
                             constants::VALUE_5))
        {
            logging::logMessage(
                "Capacity calculation failed for die per package. DDIMM Byte 4 "
                "value [" +
                std::to_string(i_iterator[constants::SPD_BYTE_4]) + "]");
            break;
        }
        uint8_t l_diePerPackage = getDdr5DiePerPackage(
            (i_iterator[constants::SPD_BYTE_4] &
             constants::MASK_BYTE_BITS_567) >>
            constants::VALUE_5);

        if (!checkValidValue(i_iterator[constants::SPD_BYTE_4] &
                                 constants::MASK_BYTE_BITS_01234,
                             constants::SHIFT_BITS_0, constants::VALUE_1,
                             constants::VALUE_8))
        {
            logging::logMessage(
                "Capacity calculation failed for SDRAM Density per Die. DDIMM "
                "Byte 4 value [" +
                std::to_string(i_iterator[constants::SPD_BYTE_4]) + "]");
            break;
        }
        uint8_t l_densityPerDie = getDdr5DensityPerDie(
            i_iterator[constants::SPD_BYTE_4] &
            constants::MASK_BYTE_BITS_01234);

        uint8_t l_ranksPerChannel = 0;

        if (((i_iterator[constants::SPD_BYTE_234] &
              constants::MASK_BYTE_BIT_7) >>
             constants::VALUE_7))
        {
            l_ranksPerChannel = ((i_iterator[constants::SPD_BYTE_234] &
                                  constants::MASK_BYTE_BITS_345) >>
                                 constants::VALUE_3) +
                                constants::VALUE_1;
        }
        else if (((i_iterator[constants::SPD_BYTE_235] &
                   constants::MASK_BYTE_BIT_6) >>
                  constants::VALUE_6))
        {
            l_ranksPerChannel = (i_iterator[constants::SPD_BYTE_234] &
                                 constants::MASK_BYTE_BITS_012) +
                                constants::VALUE_1;
        }

        if (!checkValidValue(i_iterator[constants::SPD_BYTE_6] &
                                 constants::MASK_BYTE_BITS_567,
                             constants::SHIFT_BITS_5, constants::VALUE_0,
                             constants::VALUE_3))
        {
            logging::logMessage(
                "Capacity calculation failed for dram width DDIMM Byte 6 value "
                "[" +
                std::to_string(i_iterator[constants::SPD_BYTE_6]) + "]");
            break;
        }
        uint8_t l_dramWidth =
            constants::VALUE_4 *
            (constants::VALUE_1 << ((i_iterator[constants::SPD_BYTE_6] &
                                     constants::MASK_BYTE_BITS_567) >>
                                    constants::VALUE_5));

        // DDIMM size is calculated in GB
        l_dimmSize = (l_channelsPerDdimm * l_busWidthPerChannel *
                      l_diePerPackage * l_densityPerDie * l_ranksPerChannel) /
                     (8 * l_dramWidth);

    } while (false);

    return constants::CONVERT_GB_TO_KB * l_dimmSize;
}

size_t DdimmVpdParser::getDdr4BasedDdimmSize(
    types::BinaryVector::const_iterator i_iterator)
{
    size_t l_dimmSize = 0;
    try
    {
        uint8_t l_tmpValue = 0;

        // Calculate SDRAM capacity
        l_tmpValue = i_iterator[constants::SPD_BYTE_4] &
                     constants::JEDEC_SDRAM_CAP_MASK;

        /* Make sure the bits are not Reserved */
        if (l_tmpValue > constants::JEDEC_SDRAMCAP_RESERVED)
        {
            throw std::runtime_error(
                "Bad data in VPD byte 4. Can't calculate SDRAM capacity and so "
                "dimm size.\n ");
        }

        uint16_t l_sdramCapacity = 1;
        l_sdramCapacity = (l_sdramCapacity << l_tmpValue) *
                          constants::JEDEC_SDRAMCAP_MULTIPLIER;

        /* Calculate Primary bus width */
        l_tmpValue = i_iterator[constants::SPD_BYTE_13] &
                     constants::JEDEC_PRI_BUS_WIDTH_MASK;

        if (l_tmpValue > constants::JEDEC_RESERVED_BITS)
        {
            throw std::runtime_error(
                "Bad data in VPD byte 13. Can't calculate primary bus width "
                "and so dimm size.");
        }

        uint8_t l_primaryBusWid = 1;
        l_primaryBusWid = (l_primaryBusWid << l_tmpValue) *
                          constants::JEDEC_PRI_BUS_WIDTH_MULTIPLIER;

        /* Calculate SDRAM width */
        l_tmpValue = i_iterator[constants::SPD_BYTE_12] &
                     constants::JEDEC_SDRAM_WIDTH_MASK;

        if (l_tmpValue > constants::JEDEC_RESERVED_BITS)
        {
            throw std::runtime_error(
                "Bad data in VPD byte 12. Can't calculate SDRAM width and so "
                "dimm size.");
        }

        uint8_t l_sdramWidth = 1;
        l_sdramWidth = (l_sdramWidth << l_tmpValue) *
                       constants::JEDEC_SDRAM_WIDTH_MULTIPLIER;

        /* Calculate Number of ranks */
        l_tmpValue = i_iterator[constants::SPD_BYTE_12] &
                     constants::JEDEC_NUM_RANKS_MASK;
        l_tmpValue >>= constants::JEDEC_RESERVED_BITS;

        if (l_tmpValue > constants::JEDEC_RESERVED_BITS)
        {
            throw std::runtime_error(
                "Bad data in VPD byte 12, can't calculate number of ranks. Invalid data found.");
        }

        uint8_t l_logicalRanksPerDimm = l_tmpValue + 1;

        // Determine is single load stack (3DS) or not
        l_tmpValue = i_iterator[constants::SPD_BYTE_6] &
                     constants::JEDEC_SIGNAL_LOADING_MASK;

        if (l_tmpValue == constants::JEDEC_SINGLE_LOAD_STACK)
        {
            // Fetch die count
            l_tmpValue = i_iterator[constants::SPD_BYTE_6] &
                         constants::JEDEC_DIE_COUNT_MASK;
            l_tmpValue >>= constants::JEDEC_DIE_COUNT_RIGHT_SHIFT;

            uint8_t l_dieCount = l_tmpValue + 1;
            l_logicalRanksPerDimm *= l_dieCount;
        }

        l_dimmSize =
            (l_sdramCapacity / constants::JEDEC_PRI_BUS_WIDTH_MULTIPLIER) *
            (l_primaryBusWid / l_sdramWidth) * l_logicalRanksPerDimm;

        // Converting dimm size from MB to KB
        l_dimmSize *= constants::CONVERT_MB_TO_KB;
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("DDR4 DDIMM calculation is failed, reason: " +
                            std::string(l_ex.what()));
        Logger::getLoggerInstance()->logMessage(
            std::string("DDR4 DDIMM calculation is failed, reason: ") +
                std::string(l_ex.what()),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::InternalFailure,
                                types::SeverityType::Warning, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt});
    }
    return l_dimmSize;
}

size_t DdimmVpdParser::getDdimmSize(
    types::BinaryVector::const_iterator i_iterator)
{
    size_t l_dimmSize = 0;
    if (i_iterator[constants::SPD_BYTE_2] == constants::SPD_DRAM_TYPE_DDR5)
    {
        l_dimmSize = getDdr5BasedDdimmSize(i_iterator);
    }
    else if (i_iterator[constants::SPD_BYTE_2] == constants::SPD_DRAM_TYPE_DDR4)
    {
        l_dimmSize = getDdr4BasedDdimmSize(i_iterator);
    }
    else
    {
        logging::logMessage(
            "Error: DDIMM is neither DDR4 nor DDR5. DDIMM Byte 2 value [" +
            std::to_string(i_iterator[constants::SPD_BYTE_2]) + "]");
    }
    return l_dimmSize;
}

void DdimmVpdParser::readKeywords(
    types::BinaryVector::const_iterator i_iterator)
{
    // collect DDIMM size value
    auto l_dimmSize = getDdimmSize(i_iterator);
    if (!l_dimmSize)
    {
        throw(DataException("Error: Calculated dimm size is 0."));
    }

    m_parsedVpdMap.emplace("MemorySizeInKB", l_dimmSize);
    // point the i_iterator to DIMM data and skip "11S"
    advance(i_iterator, constants::DDIMM_11S_BARCODE_START +
                            constants::DDIMM_11S_FORMAT_LEN);
    types::BinaryVector l_partNumber(i_iterator,
                                     i_iterator + constants::PART_NUM_LEN);

    advance(i_iterator, constants::PART_NUM_LEN);
    types::BinaryVector l_serialNumber(i_iterator,
                                       i_iterator + constants::SERIAL_NUM_LEN);

    advance(i_iterator, constants::SERIAL_NUM_LEN);
    types::BinaryVector l_ccin(i_iterator, i_iterator + constants::CCIN_LEN);

    types::BinaryVector l_mfgId(DRAM_MANUFACTURER_ID_LENGTH);
    std::copy_n((m_vpdVector.cbegin() + DRAM_MANUFACTURER_ID_OFFSET),
                DRAM_MANUFACTURER_ID_LENGTH, l_mfgId.begin());

    m_parsedVpdMap.emplace("FN", l_partNumber);
    m_parsedVpdMap.emplace("PN", move(l_partNumber));
    m_parsedVpdMap.emplace("SN", move(l_serialNumber));
    m_parsedVpdMap.emplace("CC", move(l_ccin));
    m_parsedVpdMap.emplace("DI", move(l_mfgId));
}

types::VPDMapVariant DdimmVpdParser::parse()
{
    try
    {
        // Read the data and return the map
        auto l_iterator = m_vpdVector.cbegin();
        readKeywords(l_iterator);
        return m_parsedVpdMap;
    }
    catch (const std::exception& exp)
    {
        logging::logMessage(exp.what());
        throw exp;
    }
}

} // namespace vpd
