#pragma once

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

namespace vpd
{
/**
 * @brief Concrete class to implement DDIMM VPD parsing.
 *
 * The class inherits ParserInterface interface class and overrides the parser
 * functionality to implement parsing logic for DDIMM VPD format.
 */
class DdimmVpdParser : public ParserInterface
{
  public:
    // Deleted API's
    DdimmVpdParser() = delete;
    DdimmVpdParser(const DdimmVpdParser&) = delete;
    DdimmVpdParser& operator=(const DdimmVpdParser&) = delete;
    DdimmVpdParser(DdimmVpdParser&&) = delete;
    DdimmVpdParser& operator=(DdimmVpdParser&&) = delete;

    /**
     * @brief Defaul destructor.
     */
    ~DdimmVpdParser() = default;

    /**
     * @brief Constructor
     *
     * @param[in] i_vpdVector - VPD data.
     */
    DdimmVpdParser(const types::BinaryVector& i_vpdVector) :
        m_vpdVector(i_vpdVector), m_logger(Logger::getLoggerInstance())
    {
        if ((constants::DDIMM_11S_BARCODE_START +
             constants::DDIMM_11S_BARCODE_LEN) > m_vpdVector.size())
        {
            throw(DataException("Malformed DDIMM VPD"));
        }
    }

    /**
     * @brief API to parse DDIMM VPD file.
     *
     * @return parsed VPD data
     */
    virtual types::VPDMapVariant parse() override;

  private:
    /**
     * @brief API to read keyword data based on the type DDR4/DDR5.
     *
     * Updates the m_parsedVpdMap with read keyword data.
     * @param[in] i_iterator - iterator to buffer containing VPD
     */
    void readKeywords(types::BinaryVector::const_iterator i_iterator);

    /**
     * @brief API to calculate DDIMM size from DDIMM VPD
     *
     * @param[in] i_iterator - iterator to buffer containing VPD
     * @return calculated size or 0 in case of any error.
     */
    size_t getDdimmSize(types::BinaryVector::const_iterator i_iterator);

    /**
     * @brief This function calculates DDR5 based DDIMM's capacity
     *
     * @param[in] i_iterator - iterator to buffer containing VPD
     * @return calculated size or 0 in case of any error.
     */
    size_t getDdr5BasedDdimmSize(
        types::BinaryVector::const_iterator i_iterator);

    /**
     * @brief This function calculates DDR4 based DDIMM's capacity
     *
     * @param[in] i_iterator - iterator to buffer containing VPD
     * @return calculated size or 0 in case of any error.
     */
    size_t getDdr4BasedDdimmSize(
        types::BinaryVector::const_iterator i_iterator);

    /**
     * @brief This function calculates DDR5 based die per package
     *
     * @param[in] i_ByteValue - the bit value for calculation
     * @return die per package value.
     */
    uint8_t getDdr5DiePerPackage(uint8_t i_ByteValue);

    /**
     * @brief This function calculates DDR5 based density per die
     *
     * @param[in] i_ByteValue - the bit value for calculation
     * @return density per die.
     */
    uint8_t getDdr5DensityPerDie(uint8_t i_ByteValue);

    /**
     * @brief This function checks the validity of the bits
     *
     * @param[in] i_ByteValue - the byte value with relevant bits
     * @param[in] i_shift - shifter value to selects needed bits
     * @param[in] i_minValue - minimum value it can contain
     * @param[in] i_maxValue - maximum value it can contain
     * @return true if valid else false.
     */
    bool checkValidValue(uint8_t i_ByteValue, uint8_t i_shift,
                         uint8_t i_minValue, uint8_t i_maxValue);

    // VPD file to be parsed
    const types::BinaryVector& m_vpdVector;

    // Stores parsed VPD data.
    types::DdimmVpdMap m_parsedVpdMap{};

    // Shared pointer to Logger object.
    std::shared_ptr<Logger> m_logger;
};
} // namespace vpd
