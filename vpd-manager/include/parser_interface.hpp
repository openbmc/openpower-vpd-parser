#pragma once

#include "types.hpp"

#include <variant>

namespace vpd
{
/**
 * @brief Interface class for parsers.
 *
 * Any concrete parser class, implementing parsing logic need to derive from
 * this interface class and override the parse method declared in this class.
 */
class ParserInterface
{
  public:
    /**
     * @brief Pure virtual function for parsing VPD file.
     *
     * The API needs to be overridden by all the classes deriving from parser
     * inerface class to implement respective VPD parsing logic.
     *
     * @return parsed format for VPD data, depending upon the
     * parsing logic.
     */
    virtual types::VPDMapVariant parse() = 0;

    /**
     * @brief Read keyword's value from hardware
     *
     * @param[in] types::ReadVpdParams - Parameters required to perform read.
     *
     * @return keyword's value on successful read. Exception on failure.
     */
    virtual types::DbusVariantType readKeywordFromHardware(
        const types::ReadVpdParams)
    {
        return types::DbusVariantType();
    }

    /**
     * @brief API to write keyword's value on hardware.
     *
     * This virtual method is created to achieve runtime polymorphism for
     * hardware writes on VPD of different types. This virtual method can be
     * redefined at derived classess to implement write according to the type of
     * VPD.
     *
     * @param[in] i_paramsToWriteData - Data required to perform write.
     *
     * @throw May throw exception depending on the implementation of derived
     * methods.
     * @return On success returns number of bytes written on hardware, On
     * failure returns -1.
     */
    virtual int writeKeywordOnHardware(
        const types::WriteVpdParams i_paramsToWriteData)
    {
        (void)i_paramsToWriteData;
        return -1;
    }

    /**
     * @brief Compares the parsed VPD data in this instance with that of
     *        the provided redundant VPD parser.
     *
     * This virtual method is created to achieve runtime polymorphism for
     * comparing VPDs of different types. This virtual method can be
     * redefined at derived classess to implement VPD comparion according to the
     * type of VPD.
     *
     * @param[in] i_redundantParser - Shared pointer to redundant parser
     * instance.
     *
     * @return true if all corresponding VPD fields match; false otherwise.
     */
    virtual bool compareData(
        [[maybe_unused]] const std::shared_ptr<vpd::ParserInterface>&
            i_redundantParser) noexcept
    {
        // @todo IpzVpdParser implements this API. Add implementations for all
        //       remaining concrete parser classes.
        return false;
    }

    /**
     * @brief Virtual destructor.
     */
    virtual ~ParserInterface() {}
};
} // namespace vpd
