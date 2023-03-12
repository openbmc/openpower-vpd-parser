#include "parser_factory.hpp"

#include "constants.hpp"
#include "ddimm_parser.hpp"
#include "exceptions.hpp"
#include "ipz_parser.hpp"
#include "isdimm_parser.hpp"
#include "keyword_vpd_parser.hpp"

namespace vpd
{

/**
 * @brief Type of VPD formats.
 */
enum vpdType
{
    IPZ_VPD,                /**< IPZ VPD type */
    KEYWORD_VPD,            /**< Keyword VPD type */
    DDR4_DDIMM_MEMORY_VPD,  /**< DDR4 DDIMM Memory VPD type */
    DDR5_DDIMM_MEMORY_VPD,  /**< DDR5 DDIMM Memory VPD type */
    DDR4_ISDIMM_MEMORY_VPD, /**< DDR4 ISDIMM Memory VPD type */
    DDR5_ISDIMM_MEMORY_VPD, /**< DDR5 ISDIMM Memory VPD type */
    INVALID_VPD_FORMAT      /**< Invalid VPD type */
};

/**
 * @brief API to get the type of VPD.
 *
 * @param[in] i_vpdVector - VPD file content
 *
 * @return Type of VPD data, "INVALID_VPD_FORMAT" in case of unknown type.
 */
static vpdType vpdTypeCheck(const types::BinaryVector& i_vpdVector)
{
    if (i_vpdVector[constants::IPZ_DATA_START] == constants::IPZ_DATA_START_TAG)
    {
        return vpdType::IPZ_VPD;
    }
    else if (i_vpdVector[constants::KW_VPD_DATA_START] ==
             constants::KW_VPD_START_TAG)
    {
        return vpdType::KEYWORD_VPD;
    }
    else if (((i_vpdVector[constants::SPD_BYTE_3] &
               constants::SPD_BYTE_BIT_0_3_MASK) ==
              constants::SPD_MODULE_TYPE_DDIMM))
    {
        std::string l_is11SFormat;
        if (i_vpdVector.size() > (constants::DDIMM_11S_BARCODE_START +
                                  constants::DDIMM_11S_BARCODE_LEN))
        {
            // Read first 3 Bytes to check the 11S bar code format
            for (uint8_t l_index = 0; l_index < constants::DDIMM_11S_FORMAT_LEN;
                 l_index++)
            {
                l_is11SFormat +=
                    i_vpdVector[constants::DDIMM_11S_BARCODE_START + l_index];
            }
        }

        if (l_is11SFormat.compare(constants::DDIMM_11S_BARCODE_START_TAG) == 0)
        {
            // DDIMM memory VPD format
            if ((i_vpdVector[constants::SPD_BYTE_2] &
                 constants::SPD_BYTE_MASK) == constants::SPD_DRAM_TYPE_DDR5)
            {
                return vpdType::DDR5_DDIMM_MEMORY_VPD;
            }

            if ((i_vpdVector[constants::SPD_BYTE_2] &
                 constants::SPD_BYTE_MASK) == constants::SPD_DRAM_TYPE_DDR4)
            {
                return vpdType::DDR4_DDIMM_MEMORY_VPD;
            }
        }

        logging::logMessage("11S format is not found in the DDIMM VPD.");
        return vpdType::INVALID_VPD_FORMAT;
    }
    else if ((i_vpdVector[constants::SPD_BYTE_2] & constants::SPD_BYTE_MASK) ==
             constants::SPD_DRAM_TYPE_DDR5)
    {
        // ISDIMM memory VPD format
        return vpdType::DDR5_ISDIMM_MEMORY_VPD;
    }
    else if ((i_vpdVector[constants::SPD_BYTE_2] & constants::SPD_BYTE_MASK) ==
             constants::SPD_DRAM_TYPE_DDR4)
    {
        // ISDIMM memory VPD format
        return vpdType::DDR4_ISDIMM_MEMORY_VPD;
    }

    return vpdType::INVALID_VPD_FORMAT;
}

std::shared_ptr<ParserInterface>
    ParserFactory::getParser(const types::BinaryVector& i_vpdVector,
                             const std::string& i_vpdFilePath,
                             size_t i_vpdStartOffset)
{
    if (i_vpdVector.empty())
    {
        throw std::runtime_error("Empty VPD vector passed to parser factory");
    }

    vpdType l_type = vpdTypeCheck(i_vpdVector);

    switch (l_type)
    {
        case vpdType::IPZ_VPD:
        {
            return std::make_shared<IpzVpdParser>(i_vpdVector, i_vpdFilePath,
                                                  i_vpdStartOffset);
        }

        case vpdType::KEYWORD_VPD:
        {
            return std::make_shared<KeywordVpdParser>(i_vpdVector);
        }

        case vpdType::DDR5_DDIMM_MEMORY_VPD:
        case vpdType::DDR4_DDIMM_MEMORY_VPD:
        {
            return std::make_shared<DdimmVpdParser>(i_vpdVector);
        }

        case vpdType::DDR4_ISDIMM_MEMORY_VPD:
        case vpdType::DDR5_ISDIMM_MEMORY_VPD:
        {
            // return shared pointer to class object.
            logging::logMessage("ISDIMM parser selected for VPD path: " +
                                i_vpdFilePath);
            return std::make_shared<JedecSpdParser>(i_vpdVector);
        }

        default:
            throw DataException("Unable to determine VPD format");
    }
}
} // namespace vpd
